"""Original conversion scheduling and shared bins on the real native EAJobs workers.

Only the mesh emitter and allocator/diagnostic boundaries are fixtures. Production
partitioning, job entry, context isolation, key walking, block claims, relocation,
reconnection, merging and sorting execute unchanged.
"""
from pathlib import Path
import argparse, os
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--break-partition', action='store_true')
p.add_argument('--drop-final-flush', action='store_true')
p.add_argument('--chain', action='store_true')
a = p.parse_args()
t = Tree()
base = 'src/GameShared/GameClasses/Graphics/Dispatch/'
bins = t.read(base + 'CgsDispatcher.cpp')
lists = t.read(base + 'CgsGraphicsDispatchList.cpp')
commands = t.read(base + 'CgsDispatcherCommands.cpp')
renderer = t.read('src/GameSource/Graphics/BrnRendererModule.cpp')
code = 'namespace CgsGraphics {\n'
for source, signatures in [
    (bins, ['inline u32 ClaimNextSharedBlock(', 'DispatchCommand* DispatchBin::AllocateCommand(',
            'void* DispatchBin::AllocateMemoryFast(', 'void DispatchBin::Align(',
            'void DispatchBin::BeginPacket(', 'void DispatchBin::HandleMemoryOverflow(',
            'DispatchList* DispatchFrame::GetList(', 'void DispatchFrame::Reset(']),
    (lists, ['DispatchList* DispatchList::ReserveKey(', 'void DispatchList::Submit(',
             'DispatchList* DispatchList::AllocateKeyBlock(', 'DispatchList* DispatchList::PrepareSortJobInfo(',
             'void DispatchList::SortForDispatch(', 'void DispatchList::RelocateForMainMemory(',
             'void DispatchList::ReconnectChainBlocks(', 'void DispatchList::Append(']),
    (commands, ['inline u32 CommandIdOf(', 'inline u32* PacketFromRecord(',
                'DispatchPacketInterpreter::DispatchPacketInterpreter(',
                'void DispatchObjectContext::ResetShadowing(', 'void DispatchFrame::ConstructWithSharedBinMemory(',
                'void DispatchFrame::RelocateForMainMemory(', 'void DispatchFrame::FlushBlockToSharedMemory(',
                'DispatchCommand* DispatchList::DispatchAllObjectToMesh('])]:
    for sig in signatures:
        code += definition(source, sig) + '\n'
code += definition(commands, 'struct DispatchObjectContext_JobState') + ';\n}\n'
code += definition(commands, 'void ObjectToMeshJob::SharedMemoryChangeCallback(') + '\n'
execute = definition(commands, 'void ObjectToMeshJob::ExecuteImplementation(')
if a.drop_final_flush:
    assert execute.count('    lrOutput.FlushBlockToSharedMemory();') == 1
    execute = execute.replace('    lrOutput.FlushBlockToSharedMemory();', '')
code += execute + '\n'
code += 'namespace { CgsGraphics::DispatchCommand* spObjectToMeshSharedMemory;\n'
code += 'u32 suObjectToMeshSharedBlockMax; alignas(128) u32 suObjectToMeshNextBlock; }\n'
code += definition(renderer, 'static void FillInObjectToMeshJobData(') + '\n'
create = definition(renderer, 'void BrnRendererModule::CreateObjectToMeshJob(')
if a.break_partition:
    line = '    FillInObjectToMeshJobData(&maObjectToMeshJob[luJobIndex]'
    assert create.count(line) == 1
    create = create.replace(line, '    if (luJobIndex > 0u && luJobIndex < 4u) ++lInput.miStartIndex;\n' + line)
code += create + '\n' + definition(renderer, 'void BrnRendererModule::ConvertObjectsToMeshes(')
jobdir = 'src/SDKs/EATech/eajobs/'
sources = [REPO / jobdir / n for n in ['bucket_list_node.cpp', 'entrypoint.cpp', 'event.cpp',
    'job.cpp', 'job_instance_handle.cpp', 'job_scheduler.cpp', 'job_thread.cpp', 'job_thread_parameters.cpp',
    'jobs.cpp', 'reference_count.cpp', 'local_backend.cpp', 'detail.cpp']]
sources += [REPO / 'vendor/EAThread/source' / n for n in ['eathread.cpp', 'eathread_mutex.cpp',
    'eathread_condition.cpp', 'eathread_barrier.cpp', 'eathread_rwmutex.cpp',
    'pc/eathread_thread_pc.cpp', 'pc/eathread_semaphore_pc.cpp', 'pc/eathread_callstack_win64.cpp']]
sources += [REPO / 'vendor/coreallocator/source/icoreallocator_interface.cpp',
            REPO / 'src/GameShared/Jobs/ObjectToMesh/ObjectToMesh.cpp',
            REPO / 'src/GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.cpp']
result = compile_and_run(Path(__file__).with_name('PCObjectMeshJobs.cpp'), 'pc_object_mesh_jobs.inc', code,
    'PCObjectMeshJobs', extra_flags='winmm.lib dbghelp.lib psapi.lib' + (' /DMESH_TEST_CHAIN' if a.chain else ''),
    extra_sources=sources)
raise SystemExit(report('run_pc_object_mesh_jobs', [], result, 17))
