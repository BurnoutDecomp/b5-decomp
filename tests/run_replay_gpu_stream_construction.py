"""CPU native ntdll stream lock and real SDK Job construction; no GPU execution.

--wrong-name temporarily restores the old Relocator header initializer.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import REPO,Tree,compile_and_run,definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wrong-name',action='store_true')
args=parser.parse_args()
tree=Tree()
source=tree.read('src/GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.cpp')
constructor=definition(source,'GPUDiskWriteStream::GPUDiskWriteStream()')
source=('extern "C" void RtlEnterCriticalSection(void*);\n'
        'extern "C" void RtlLeaveCriticalSection(void*);\n'
        'extern "C" long RtlInitializeCriticalSection(void*);\n'
        'namespace BrnReplays {\n'+constructor+'\n}\n')
shadow={}
if args.wrong_name:
    path='src/GameShared/Jobs/Relocator/CgsRelocator.h'
    shadow[path]=tree.read(path).replace('mJob(nullptr)','mJob("Relocator")',1)
sdk=REPO/'src/SDKs/EATech/eajobs'
job_source=tree.read('src/SDKs/EATech/eajobs/job.cpp')
source+='\nnamespace EA { namespace Jobs {\n'+'\n'.join(
    definition(job_source,signature) for signature in (
        'static EntryPoint MakeDefaultEntryPoint()', 'void Job::Clear()',
        '    Job::Job(const char* lpcName)', '    Job::~Job()\n'))+'\n} }\n'
numeric=compile_and_run(Path(__file__).with_name('ReplayGPUStreamConstruction.cpp'),
    'replay_gpu_stream_construction.inc',source,'ReplayGPUStreamConstruction',
    extra_flags='ntdll.lib',shadow=shadow,extra_sources=[
        sdk/'entrypoint.cpp',sdk/'event.cpp',sdk/'bucket_list_node.cpp',sdk/'jobs.cpp'])
if numeric is None: raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_gpu_stream_construction: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
