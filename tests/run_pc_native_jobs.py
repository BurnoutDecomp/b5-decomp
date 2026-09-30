"""Actual EA scheduler/worker/dependency execution against the shipping native EAThread backend."""
import argparse,os
from pathlib import Path
from fxgs_common import REPO,Tree,definition,compile_and_run,report
os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--drop-arguments',action='store_true')
parser.add_argument('--truncate-wait-start',action='store_true')
args=parser.parse_args()
jobdir='src/SDKs/EATech/eajobs/'
backend=Tree().read(jobdir+'local_backend.cpp')
if args.drop_arguments:
    line='for (int i=0;i<4;++i) mExecution.mArguments[i]=pArguments[i];'
    assert backend.count(line)==1
    backend=backend.replace(line,'for (int i=0;i<4;++i) mExecution.mArguments[i]=Param();')
sources=[REPO/jobdir/n for n in ['bucket_list_node.cpp','entrypoint.cpp','event.cpp',
    'job.cpp','job_instance_handle.cpp','job_scheduler.cpp','job_thread.cpp','job_thread_parameters.cpp',
    'jobs.cpp','reference_count.cpp']]
sources += [REPO/'vendor/EAThread/source'/n for n in ['eathread.cpp','eathread_mutex.cpp',
    'eathread_condition.cpp','eathread_barrier.cpp','eathread_rwmutex.cpp',
    'pc/eathread_thread_pc.cpp','pc/eathread_semaphore_pc.cpp','pc/eathread_callstack_win64.cpp']]
sources += [REPO/'vendor/coreallocator/source/icoreallocator_interface.cpp']
shadow={}
if args.truncate_wait_start:
    detail=Tree().read(jobdir+'detail.cpp')
    line='static_cast<u64>(lCounter.QuadPart) - uStartTicks;'
    assert detail.count(line)==1
    shadow[jobdir+'detail.cpp']=detail.replace(line,
        'static_cast<u64>(lCounter.QuadPart) - static_cast<u32>(uStartTicks);')
result=compile_and_run(Path(__file__).with_name('PCNativeJobs.cpp'),'unused.inc','',
    'PCNativeJobs',extra_flags='winmm.lib dbghelp.lib psapi.lib',
    shadow=shadow,
    extra_files={'pc_native_backend.cpp':backend},extra_sources=['pc_native_backend.cpp',*sources])
raise SystemExit(report('run_pc_native_jobs',[],result,34))
