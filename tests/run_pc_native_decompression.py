"""Native jobs + actual engine heaps + production decompressor integration."""
import os,random,zlib
from pathlib import Path
from fxgs_common import REPO,compile_and_run,report

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
payload=random.Random(371).randbytes(700000)
arrays=''
for name,data in [('Payload',payload),('Packed',zlib.compress(payload)),('SmallPacked',zlib.compress(payload[:64]))]:
    arrays+='static const unsigned char '+name+'[]={'+','.join(map(str,data))+'};\n'
jobs=REPO/'src/SDKs/EATech/eajobs'
sources=[jobs/n for n in ['bucket_list_node.cpp','detail.cpp','entrypoint.cpp','event.cpp',
    'job.cpp','job_instance_handle.cpp','job_scheduler.cpp','job_thread.cpp','job_thread_parameters.cpp',
    'jobs.cpp','local_backend.cpp','reference_count.cpp']]
sources += [REPO/'vendor/EAThread/source'/n for n in ['eathread.cpp','eathread_mutex.cpp',
    'eathread_condition.cpp','eathread_barrier.cpp','eathread_rwmutex.cpp',
    'pc/eathread_thread_pc.cpp','pc/eathread_semaphore_pc.cpp','pc/eathread_callstack_win64.cpp']]
sources += [REPO/'vendor/coreallocator/source/icoreallocator_interface.cpp',
    REPO/'vendor/PPMalloc/src/EAGeneralAllocator.cpp',
    REPO/'src/GameShared/GameClasses/Memory/CgsHeapMalloc.cpp']
sources += [REPO/'src/GameShared/Jobs/DecompressionJob'/n for n in
    ['CgsDecompressor.cpp','DecompressionJobInterface.cpp']]
sources += [REPO/'vendor/zlib/src'/n for n in
    ['inflate.c','inftrees.c','inffast.c','adler32.c','crc32.c','zutil.c']]
numeric=compile_and_run(Path(__file__).with_name('PCNativeDecompression.cpp'),
    'pc_native_compressed_payload.inc',arrays,'PCNativeDecompression',
    extra_flags='/F8388608 winmm.lib dbghelp.lib psapi.lib',extra_sources=sources)
raise SystemExit(report('run_pc_native_decompression',[],numeric,10))
