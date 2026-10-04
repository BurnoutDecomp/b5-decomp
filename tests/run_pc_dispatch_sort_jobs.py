"""Original sort entries, native EAJobs and real dispatch bins across two frames."""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--omit-fifth-shadow', action='store_true')
p.add_argument('--truncate-count', action='store_true')
p.add_argument('--skip-reuse-wait', action='store_true')
p.add_argument('--skip-owner-pump', action='store_true')
p.add_argument('--skip-shutdown-join', action='store_true')
a = p.parse_args()
t = Tree()
base = 'src/GameShared/GameClasses/Graphics/Dispatch/'
bins = t.read(base + 'CgsDispatcher.cpp')
lists = t.read(base + 'CgsGraphicsDispatchList.cpp')
renderer = t.read('src/GameSource/Graphics/BrnRendererModule.cpp')
commands = t.read(base + 'CgsDispatcherCommands.cpp')
code = 'namespace CgsGraphics {\n'
for source, signatures in [
    (bins, ['inline u32 ClaimNextSharedBlock(', 'DispatchCommand* DispatchBin::AllocateCommand(',
            'void* DispatchBin::AllocateMemoryFast(', 'void DispatchBin::BeginPacket(',
            'void DispatchBin::HandleMemoryOverflow(', 'DispatchList* DispatchFrame::GetList(',
            'void DispatchFrame::Reset(']),
    (lists, ['DispatchList* DispatchList::ReserveKey(', 'void DispatchList::Submit(',
             'DispatchList* DispatchList::AllocateKeyBlock(', 'DispatchList* DispatchList::PrepareSortJobInfo(',
             'void DispatchList::RelocateForMainMemory(']),
    (commands, ['void DispatchFrame::RelocateForMainMemory(', 'void DispatchFrame::FlushBlockToSharedMemory('])]:
    for sig in signatures:
        code += definition(source, sig) + '\n'
code += '}\n' + definition(renderer, 'static void FillInSortJobData(') + '\nnamespace renderengine {\n'
# The constructor's initializer contains braces: extract up to the next definition.
start = renderer.index('    DispatchSortJobsPC::DispatchSortJobsPC()')
end = renderer.index('    DispatchSortJobsPC::~DispatchSortJobsPC()', start)
code += renderer[start:end]
for sig in ['DispatchSortJobsPC::~DispatchSortJobsPC(', 'static void WideSortEntryPC(',
            'void DispatchSortJobsPC::Begin(', 'void DispatchSortJobsPC::WaitIndex(',
            'void DispatchSortJobsPC::WaitList(', 'void DispatchSortJobsPC::WaitAll(']:
    body = definition(renderer, sig)
    if a.omit_fifth_shadow and sig == 'void DispatchSortJobsPC::Begin(':
        body = body.replace('lpScheduler->AddJobs(&maJobs[luJob], 1);',
                            'if (luJob != 4) lpScheduler->AddJobs(&maJobs[luJob], 1);')
    if a.truncate_count and sig == 'void DispatchSortJobsPC::Begin(':
        body = body.replace('> 0xffffu', '> 0xffffffffu')
    if a.skip_reuse_wait and sig == 'void DispatchSortJobsPC::Begin(':
        body = body.replace('WaitAll();', '')
    if a.skip_owner_pump and sig == 'void DispatchSortJobsPC::WaitIndex(':
        body = body.replace('lbOwner ? &MeshJobOwnerWaitPC::Poll : nullptr', 'nullptr')
    code += body + '\n'
code += '}\n'
code += definition(renderer, 'void BrnRendererModule::EndMeshFramesPC(') + '\n'
game = t.read('src/GameSource/Game/BrnGameModule.cpp')
end = definition(game, 'void BrnGameModule::EndFramesPC(')
if a.skip_shutdown_join:
    end = end.replace('mRenderModule.EndMeshFramesPC();', '')
code += 'namespace BrnGame {\n' + end + '\n}\n'
jobdir = 'src/SDKs/EATech/eajobs/'
sources = [REPO / jobdir / n for n in ['bucket_list_node.cpp', 'entrypoint.cpp', 'event.cpp',
    'job.cpp', 'job_instance_handle.cpp', 'job_scheduler.cpp', 'job_thread.cpp', 'job_thread_parameters.cpp',
    'jobs.cpp', 'reference_count.cpp', 'local_backend.cpp', 'detail.cpp']]
sources += [REPO / 'vendor/EAThread/source' / n for n in ['eathread.cpp', 'eathread_mutex.cpp',
    'eathread_condition.cpp', 'eathread_barrier.cpp', 'eathread_rwmutex.cpp',
    'pc/eathread_thread_pc.cpp', 'pc/eathread_semaphore_pc.cpp', 'pc/eathread_callstack_win64.cpp']]
sources += [REPO / 'vendor/coreallocator/source/icoreallocator_interface.cpp',
            REPO / 'src/GameShared/Jobs/RadixSort/RadixSort.cpp',
            REPO / 'src/GameShared/Jobs/RadixSort/RadixSortJob.cpp']
result = compile_and_run(Path(__file__).with_name('PCDispatchSortJobs.cpp'),
    'pc_dispatch_sort_jobs.inc', code, 'PCDispatchSortJobs',
    extra_flags='winmm.lib dbghelp.lib psapi.lib user32.lib', extra_sources=sources)
raise SystemExit(report('run_pc_dispatch_sort_jobs', [], result, 121))
