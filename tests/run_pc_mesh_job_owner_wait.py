"""Actual job/worker assertion/window dependency through the mesh owner's wait."""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--no-service', action='store_true')
p.add_argument('--no-messages', action='store_true')
a = p.parse_args()
t = Tree()
source = t.read('src/pc/gcm/renderengine/MeshJobOwnerWait.h')
if a.no_service:
    source = source.replace('CgsDev::Assert::ServiceWorkerAssertsWhileWaitingPC();', '(void)0;')
if a.no_messages:
    source = source.replace('while (PeekMessageW(&lMessage, nullptr, 0, 0, PM_REMOVE))', 'while (false)')
renderer = code_only(definition(t.read('src/GameSource/Graphics/BrnRendererModule.cpp'),
    'void BrnRendererModule::ConvertObjectsToMeshesPC('))
wiring = [('only the producer interpreter uses the owner callback',
           'lpInterpreter == mpMeshProducerInterpreterPC' in renderer
           and 'lbOwnerWait ? &renderengine::MeshJobOwnerWaitPC::Poll : nullptr' in renderer)]
jobdir = REPO / 'src/SDKs/EATech/eajobs'
sources = [jobdir / name for name in ['bucket_list_node.cpp', 'entrypoint.cpp', 'event.cpp',
    'job.cpp', 'job_instance_handle.cpp', 'job_scheduler.cpp', 'job_thread.cpp', 'job_thread_parameters.cpp',
    'jobs.cpp', 'reference_count.cpp', 'local_backend.cpp', 'detail.cpp']]
ea = REPO / 'vendor/EAThread/source'
sources += [ea / name for name in ['eathread.cpp', 'eathread_mutex.cpp', 'eathread_condition.cpp',
    'eathread_barrier.cpp', 'eathread_rwmutex.cpp', 'pc/eathread_thread_pc.cpp',
    'pc/eathread_semaphore_pc.cpp', 'pc/eathread_callstack_win64.cpp']]
sources += [REPO / 'vendor/coreallocator/source/icoreallocator_interface.cpp',
    REPO / 'src/GameShared/GameClasses/Core/CgsAssert.cpp',
    REPO / 'src/GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.cpp',
    REPO / 'vendor/renderware/src/rw/core/debug/DebugCriticalSection.cpp', STRSTREAM_CPP]
result = compile_and_run(Path(__file__).with_name('PCMeshJobOwnerWait.cpp'),
    'pc_mesh_job_owner_wait.inc', source, 'PCMeshJobOwnerWait', extra_sources=sources,
    extra_flags='/Gy /Gw winmm.lib dbghelp.lib psapi.lib user32.lib advapi32.lib')
raise SystemExit(report('run_pc_mesh_job_owner_wait', wiring, result, 5))
