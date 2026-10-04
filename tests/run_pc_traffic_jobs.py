"""Native traffic scheduling against the actual serial vehicle-update kernel."""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, Tree, definition, compile_and_run, report, STRSTREAM_CPP

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--serial', action='store_true')
p.add_argument('--diagnostic', action='store_true')
p.add_argument('--shared-worker', action='store_true')
p.add_argument('--borrow-stack', action='store_true')
p.add_argument('--skip-join', action='store_true')
p.add_argument('--reverse-requests', action='store_true')
a = p.parse_args()
t = Tree()
module_dir = 'src/GameSource/World/EntityModules/TrafficEntityModule/'
job_dir = 'src/GameSource/Jobs/Traffic/'
module = t.read(module_dir + 'BrnTrafficEntityModule.cpp')
code = 'namespace BrnTraffic {\n'
for sig in ['void TrafficEntityModule::UpdateVehicles(', 'void TrafficEntityModule::SendPhysicalRequests(']:
    body = definition(module, sig).replace('TrafficEntityModule::', 'TrafficFixture::')
    body = body.replace('BrnTrafficIO::InputBuffer_PostPhysics', 'TestPostInput')
    body = body.replace('BrnTrafficIO::OutputBuffer_PostPhysics', 'TestPostOutput')
    body = body.replace('BrnTrafficIO::OutputBuffer_PrePhysics', 'TestPreOutput')
    if a.skip_join:
        body = body.replace('maJobs[luJob].WaitOn();', ';')
    if a.reverse_requests and 'SendPhysicalRequests' in sig:
        body = body.replace('maJobs[luJob].GetNewPhysicalRequests()',
                            'maJobs[muNumUpdateVehiclesJobs - 1 - luJob].GetNewPhysicalRequests()')
    code += body + '\n'
code += '}\n'
job = t.read(job_dir + 'BrnTrafficJob.cpp')
# An observation/delay boundary executes before the real worker body. The
# numeric oracle below still runs actual Initialise/Move/WriteBack and requests.
job = 'namespace BrnTraffic { union JobParams; }\nextern bool TrafficTestBeforeExecute(void*, BrnTraffic::JobParams*);\n' + job
needle = '    slWorker.Execute(static_cast<JobParams*>(lData.mpValue));'
assert job.count(needle) == 1
job = job.replace(needle, '    if (!TrafficTestBeforeExecute(&slWorker, static_cast<JobParams*>(lData.mpValue))) return;\n' + needle)
if a.shared_worker:
    job = job.replace('static thread_local TrafficJob slWorker;', 'static TrafficJob slWorker;')
if a.borrow_stack:
    job = job.replace('mJob.SetData(&mJobData, KU_JOB_DESCRIPTOR_BYTES);',
                      'mJob.SetData(lpParams, KU_JOB_DESCRIPTOR_BYTES);')
code += job
ea = 'src/SDKs/EATech/eajobs/'
sources = [REPO / ea / n for n in ['bucket_list_node.cpp', 'entrypoint.cpp', 'event.cpp',
    'job.cpp', 'job_instance_handle.cpp', 'job_scheduler.cpp', 'job_thread.cpp', 'job_thread_parameters.cpp',
    'jobs.cpp', 'reference_count.cpp', 'local_backend.cpp', 'detail.cpp']]
sources += [REPO / 'vendor/EAThread/source' / n for n in ['eathread.cpp', 'eathread_mutex.cpp',
    'eathread_condition.cpp', 'eathread_barrier.cpp', 'eathread_rwmutex.cpp',
    'pc/eathread_thread_pc.cpp', 'pc/eathread_semaphore_pc.cpp', 'pc/eathread_callstack_win64.cpp']]
sources += [REPO / 'vendor/coreallocator/source/icoreallocator_interface.cpp', STRSTREAM_CPP,
            REPO / 'src/GameShared/GameClasses/Numeric/CgsRandom.cpp']
sources += [REPO / job_dir / n for n in ['TrafficCommon.cpp', 'BrnUpdateVehiclesJob.cpp', 'BrnUpdateVehiclesJob_MoveToTarget.cpp']]
sources += [REPO / module_dir / n for n in ['BrnTrafficVehicle.cpp', 'BrnTrafficParam.cpp', 'BrnTrafficMiscRuntimeClasses.cpp']]
sources += [REPO / module_dir / 'BrnTrafficPatternGenerator.cpp', REPO / 'src/GameSource/Math/BrnMathUtils.cpp']
sources += [REPO / 'src/SharedClasses/Traffic' / n for n in ['BrnTrafficFuzzyEnvelopeSet.cpp', 'BrnTrafficHull.cpp', 'BrnTrafficSection.cpp']]
flags = 'winmm.lib dbghelp.lib psapi.lib user32.lib'
if a.serial: flags += ' /DTRAFFIC_TEST_SERIAL'
if a.diagnostic: flags += ' /DTRAFFIC_TEST_DIAGNOSTIC'
result = compile_and_run(Path(__file__).with_name('PCTrafficJobs.cpp'),
    'pc_traffic_jobs.inc', code, 'PCTrafficJobs', extra_flags=flags, extra_sources=sources)
raise SystemExit(report('run_pc_traffic_jobs', [], result, 105))
