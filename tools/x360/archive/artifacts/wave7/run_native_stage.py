"""Freeze a new Wave7 source snapshot and run the actual portable Xenon compiler."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
CONFIG = json.loads((HERE / 'campaign.json').read_text(encoding='utf-8'))
WORK = Path(CONFIG['workspace'])
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name', required=True)
parser.add_argument('--mode', choices=('selected', 'full'), required=True)
args = parser.parse_args()


def git(*options):
    return subprocess.check_output(['git', '--no-optional-locks', *options], cwd=WORK)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)


assert not git('status', '--porcelain=v1').strip()
head = git('rev-parse', 'HEAD').decode().strip()
receipt = json.loads((HERE / 'wave7-receipts.json').read_text())
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
        'BrnGameStateModule.cpp', 'BrnGuiModule.cpp'})
    sources = sorted(selected)
batch = 'native-parallel-wave7-' + args.name
output = HERE / args.name
temp = HERE / (args.name + '-compiler-temp')
assert sources and not output.exists() and not temp.exists()
metadata = {'base': CONFIG['base'], 'head': head, 'workspace': str(WORK),
            'sha256': hashlib.sha256(patch).hexdigest(), 'paths': paths,
            'source_sha256': {p: sha(WORK / p) for p in paths},
            'source_snapshot': snapshot, 'selected_sources': sources, 'selection': args.mode,
            'native_inventory': native, 'receipt_sha256': sha(HERE / 'wave7-receipts.json'),
            'created_utc': datetime.now(timezone.utc).isoformat()}
save(HERE / (batch + '-patch.json'), metadata)
with (HERE / (batch + '.diff')).open('xb') as stream:
    stream.write(patch)
baseline = json.loads(Path(CONFIG['baseline_report']).read_text())
canonical = str(Path(CONFIG['canonical'])).replace('\\', '/').lower()
extra = [a for a in baseline['results'][0]['command']
         if a.replace('\\', '/').lower().startswith('/i' + canonical + '/vendor/')]
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
driver = WORK / 'tools/build_x360.py'
arguments = [str(driver), '--sdk', baseline['sdk'], '--out', str(output),
             '--jobs', '8', '--timeout', '180', '--quiet']
arguments += ['--compiler-arg=' + a for a in extra]
for source in sources:
    arguments += ['--source', source]
save(HERE / (args.name + '-command.json'), {'head': head, 'argv': arguments,
     'driver_sha256': sha(driver), 'private_temp': str(temp), 'metadata_sha256': sha(HERE / (batch + '-patch.json'))})
owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
owner = json.loads(owner_path.read_text())
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave7_' + args.name + '_native_build', frozen_head=head,
             live_process_id=os.getpid(), live_session=None,
             live_report=str(output / 'compile-results.json'),
             instruction='Source frozen during actual Wave7 native compilation. Poll the SAME handle until actual terminal; '
                         'never restart on observation timeout. All new artifacts/TEMP on D; no source mutation while running.')
owner_path.write_text(json.dumps(owner, indent=2) + '\n', encoding='utf-8')
print('FROZEN', head, 'mode', args.mode, 'sources', len(sources), 'inventory', len(native), 'PID', os.getpid(), flush=True)
driver_spec = importlib.util.spec_from_file_location('actual_native_driver', driver)
module = importlib.util.module_from_spec(driver_spec)
driver_spec.loader.exec_module(module)
sys.argv = arguments
code = module.main()
save(HERE / (args.name + '-exit.json'), {'head': head, 'exit_code': code,
     'report': str(output / 'compile-results.json'), 'report_sha256': sha(output / 'compile-results.json')})
raise SystemExit(code)
