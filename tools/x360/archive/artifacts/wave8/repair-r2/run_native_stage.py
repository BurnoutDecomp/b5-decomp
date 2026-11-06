"""Freeze a new Wave8 source snapshot and run the actual portable Xenon compiler."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
CONFIG = json.loads((HERE / 'campaign.json').read_text(encoding='utf-8'))
WORK = Path(CONFIG['workspace'])
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name', required=True)
parser.add_argument('--mode', choices=('selected', 'repair', 'full'), required=True)
args = parser.parse_args()


def git(*options):
    return subprocess.check_output(['git', '--no-optional-locks', *options], cwd=WORK)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)


assert not git('status', '--porcelain=v1').strip()
assert git('-C', CONFIG['canonical'], 'rev-parse', 'HEAD').decode().strip() == CONFIG['published_base']
assert not git('-C', CONFIG['canonical'], 'status', '--porcelain=v1').strip()
head = git('rev-parse', 'HEAD').decode().strip()
receipt = json.loads((HERE / 'repair-r2-receipts.json').read_text())
assert receipt['state'] == 'complete' and receipt['head'] == head
paths = git('diff', '--name-only', CONFIG['base']).decode().splitlines()
patch = git('diff', '--binary', CONFIG['base'])
native = sorted(p for p in git('ls-files', '*.cpp', '*.c').decode().splitlines()
                if p.startswith(('src/', 'vendor/')) and 'pc' not in [s.lower() for s in Path(p).parts])
spec = importlib.util.spec_from_file_location('native_input_graph',
    SCRATCH / 'parallel-native/wave4_integrator_sol/wave4_evidence.py')
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
snapshot = common.source_snapshot()
plan = json.loads(Path(CONFIG['current_plan']).read_text(encoding='utf-8-sig'))
assert len(native) == plan['expected_native_inventory'] == 3616
assert len(snapshot['hashes']) == plan['expected_native_text_inputs'] == 6293
assert set(paths) == set(plan['candidate_paths']), (set(paths) - set(plan['candidate_paths']), set(plan['candidate_paths']) - set(paths))
assert len(receipt['receipts']) == len(plan['named_originals']) and {r['source_commit'] for r in receipt['receipts']} == set(plan['named_originals'])
assert git('-C', str(PARENT), 'ls-files', '--stage', 'b5-decomp').decode().strip() == plan['parent_gitlink_index']
if args.mode == 'full':
    sources = native
else:
    selected = set(paths) & set(native)
    selected.update(p for p in native if Path(p).name in {
        'CgsDebugManager.cpp', 'CgsStrStream.cpp', 'SpliceObjects.cpp',
        'BrnGameModule.cpp', 'BrnMainX360.cpp', 'BrnBehaviourAttractMode.cpp',
        'CgsDepthStencilStateFactory.cpp', 'CgsRasterizerStateFactory.cpp',
        'depthstencilstate.cpp', 'rasterizerstate.cpp', 'vertexformat.cpp',
        'rwgpfxhelper.cpp', 'BrnRendererModule.cpp', 'BrnLionBlendIm3d.cpp',
        'CgsRelocator.cpp', 'CgsResourceBundleLoaderModule.cpp', 'eathread_rwmutex.cpp',
        'BrnGameStateModule.cpp', 'BrnGuiModule.cpp',
        'CgsSpatialPartition.cpp', 'CgsSpatialPartitionManager.cpp', 'CgsSceneManagerModule.cpp',
        'CgsGuiStateInterface_OutputGuiEvent_Inst.cpp', 'BrnNetworkStateManager_wN1_04.cpp'})
    # Actual whole-product sentinels from the final owner header/caller receipts.
    extra_names = {
        'BrnDirectorAllVehicleData.cpp', 'CgsMicrophone.cpp', 'Cgs3dEffectControl.cpp',
        'BrnRaceCarCache.cpp', 'CgsSoundUtils.cpp', 'CgsEnvironment.cpp',
        'BoxVolume.cpp', 'CapsuleVolume.cpp', 'CylinderVolume.cpp', 'LineSegIntersect.cpp',
        'GPRegistration.cpp', 'TriangleVolume.cpp', 'BrnGuiCache.cpp',
        'BrnZoneList.cpp', 'Zone.cpp', 'ZoneList.cpp', 'Engine.cpp', 'Wheel.cpp', 'BrnLoadingScreenRenderer.cpp', 'CameraUtils.cpp', 'BrnCameraUtils.cpp', 'BrnRenderMetrics.cpp', 'BrnCameraInterpolationController.cpp', 'BrnInterpolater.cpp', 'BrnLooker.cpp', 'BrnTagPoint.cpp', 'BrnIKDrivenPoint.cpp', 'CgsVectorFont.cpp', 'CgsAssertManager.cpp', 'CgsDebugUI.cpp', 'CgsWindow.cpp', 'CgsJobManager.cpp', 'CgsHardwareInitPS3.cpp', 'eathread_semaphore.cpp',
        'Profiler.cpp', 'TimerManager.cpp', 'Mixer.cpp', 'stream.cpp',
        'sndplayer1.cpp', 'sndplayer1shared.cpp', 'CgsGenericRwacVoice.cpp',
        'BrnTriangleCacheManager.cpp', 'BrnPhysicalTrafficManager.cpp',
        'CgsLineTests.cpp', 'CgsPolygonSoupTests.cpp', 'CgsTriangleSphere.cpp', 'CgsBox.cpp', 'CgsSphere.cpp', 'CgsAxisAlignedBox.cpp', 'CgsFrustum.cpp', 'CgsRenderTarget.cpp', 'BrnPostFx.cpp', 'BrnPfxHookBlender.cpp', 'BrnEnvironmentManager.cpp', 'CgsShaderConstantTable.cpp', 'blendstate.cpp', 'vertexdescriptor.cpp', 'programbuffer.cpp',
        'rwgpfxrendertarget.cpp', 'rwgpfxtint.cpp', 'BrnLionBlendRenderer.cpp',
        'BrnLionBlendVertex.cpp', 'CgsDebug3DImmediateRender.cpp'}
    selected.update(p for p in native if Path(p).name in extra_names)
    sources = sorted(selected)
    if args.mode == 'repair':
        sources = sorted(p for p in native if Path(p).name in {
            'BrnSatNavIcon.cpp', 'BrnMapIconManager.cpp', 'BrnCrashNavMap.cpp'})
        assert len(sources) == 3
batch = 'native-parallel-wave8-r2-' + args.name
output = HERE / args.name
temp = HERE / (args.name + '-compiler-temp')
assert sources and not output.exists() and not temp.exists()
metadata = {'base': CONFIG['base'], 'head': head, 'workspace': str(WORK),
            'sha256': hashlib.sha256(patch).hexdigest(), 'paths': paths,
            'source_sha256': {p: sha(WORK / p) for p in paths},
            'source_snapshot': snapshot, 'selected_sources': sources, 'selection': args.mode,
            'native_inventory': native, 'receipt_sha256': sha(HERE / 'repair-r2-receipts.json'),
            'created_utc': datetime.now(timezone.utc).isoformat()}
save(HERE / (batch + '-patch.json'), metadata)
with (HERE / (batch + '.diff')).open('xb') as stream:
    stream.write(patch)
baseline = json.loads(Path(CONFIG['baseline_report']).read_text())
canonical = str(Path(CONFIG['canonical'])).replace('\\', '/').lower()
extra = [a for a in baseline['results'][0]['command']
         if a.replace('\\', '/').lower().startswith('/i' + canonical + '/vendor/')]
# Bind tool/includes/submodules before the actual compiler; preserve a separate terminal observation.
context_spec = importlib.util.spec_from_file_location('readonly_external_context', HERE.parent.parent / 'wave5-delivery/wave5_delivery.py')
context_module = importlib.util.module_from_spec(context_spec)
context_spec.loader.exec_module(context_module)
context_module.BASE = CONFIG['base']
external = context_module.external_inputs(baseline)
external['scope'] = 'Conservative canonical include/compiler/submodule fingerprint observed BEFORE this stage; not an include trace.'
save(HERE / (args.name + '-external-inputs-before.json'), {'head':head, 'inputs':external, 'created_utc':datetime.now(timezone.utc).isoformat()})
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
driver = WORK / 'tools/build_x360.py'
arguments = [str(driver), '--sdk', baseline['sdk'], '--out', str(output),
             '--jobs', '8', '--timeout', '180', '--quiet']
arguments += ['--compiler-arg=' + a for a in extra]
for source in sources:
    arguments += ['--source', source]
save(HERE / (args.name + '-command.json'), {'head': head, 'argv': arguments,
     'driver_sha256': sha(driver), 'launcher_sha256': sha(__file__), 'private_temp': str(temp), 'metadata_sha256': sha(HERE / (batch + '-patch.json'))})
owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
owner = json.loads(owner_path.read_text())
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave8_repair_r2_' + args.name + '_native_build', frozen_head=head,
             live_process_id=os.getpid(), live_session=None,
             live_report=str(output / 'compile-results.json'),
             source_freeze_status='FROZEN during actual ' + args.mode + ' native gate',
             source_metadata=str(HERE / (batch + '-patch.json')),
             source_metadata_sha256=sha(HERE / (batch + '-patch.json')),
             instruction='Source frozen during actual Wave8 native compilation. Poll the SAME handle until actual terminal; '
                         'never restart on observation timeout. All new artifacts/TEMP on D; no source mutation while running.')
owner_path.write_text(json.dumps(owner, indent=2) + '\n', encoding='utf-8')
operation = SCRATCH / 'parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md'
operation.write_text(
    '# Current native Wave8 actual ' + args.mode + ' gate\n\n'
    'Exclusive owner `/root/wave5_integrator_sol`. Canonical remains published3534; no publication authorized.\n'
    'Detached intake is FROZEN at`' + head + '`; ' + str(len(receipt['receipts'])) + ' NEW receipts.\n'
    'Stage`' + args.name + '` requests' + str(len(sources)) + ' whole native sources; inventory' + str(len(native)) + ', input hashes' + str(len(snapshot['hashes'])) + '.\n'
    'Actual Python PID' + str(os.getpid()) + '; unified handle is recorded in INTEGRATION_OWNER. Poll SAME handle to actual terminal; do not restart or mutate source/inputs.\n'
    'Report`' + str(output / 'compile-results.json') + '`. Metadata`' + str(HERE / (batch + '-patch.json')) + '`. Partial counts are not a final result.\n\n'
    'First c0d full2812/804/audit8a9fb095 and R1full2886/730/audit986aff remain immutable. This is new finiteR2 source; no new global result before fullterminal.\n'
    'Named sealedR2 Camera/Rig/Wheel/Env included; all audioDAC/Mask4/PhysicalBodyPart/Deformable/Volume and otherunsealed/laterWIP excluded. Same sole reviewerW2 after genuine fixes, no new reviewer. No forced link, main/dev/parent-index or production ledger changes. Whole native goal ACTIVE; image/link/launch unproved.\n', encoding='utf-8')
print('FROZEN', head, 'mode', args.mode, 'sources', len(sources), 'inventory', len(native), 'PID', os.getpid(), flush=True)
driver_spec = importlib.util.spec_from_file_location('actual_native_driver', driver)
module = importlib.util.module_from_spec(driver_spec)
driver_spec.loader.exec_module(module)
sys.argv = arguments
code = module.main()
external_after = context_module.external_inputs(json.loads((output / 'compile-results.json').read_text(encoding='utf-8')))
for key in ('include_trees', 'submodules', 'compiler_and_runtime'):
    assert external[key] == external_after[key], ('External input changed during actual compiler', key)
save(HERE / (args.name + '-external-inputs-after.json'), {'head':head, 'inputs':external_after, 'created_utc':datetime.now(timezone.utc).isoformat()})
save(HERE / (args.name + '-exit.json'), {'head': head, 'exit_code': code,
     'report': str(output / 'compile-results.json'), 'report_sha256': sha(output / 'compile-results.json')})
raise SystemExit(code)
