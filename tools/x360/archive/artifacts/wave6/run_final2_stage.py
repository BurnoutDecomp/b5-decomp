"""Freeze the authorized successor and compile its complete conservative consumer closure."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE / 'campaign.json').read_text())
WORK = Path(CONFIG['workspace'])
spec = importlib.util.spec_from_file_location('immutable_native_evidence', HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
e.BASE = CONFIG['base']
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode', choices=('closure', 'full'), required=True)
args = parser.parse_args()

assert not e.git('status', '--porcelain=v1').strip()
head = e.git('rev-parse', 'HEAD').decode().strip()
plan = e.read(HERE / 'intake-plan-04.json')
receipts = e.read(HERE / 'wave6-receipts.json')
frozen_receipts = e.read(HERE / 'frozen-stage1-55-receipts.json')
assert receipts['state'] == 'complete' and receipts['head'] == head
assert receipts['receipts'][:55] == frozen_receipts['receipts']
assert receipts['receipts'][:58] == e.read(HERE / 'final-wave6-receipts.json')['receipts']
assert [r['source_commit'] for r in receipts['receipts'][55:]] == plan['collect_now']
old_review_path = HERE / 'native-parallel-wave6-final-review-result.json'
old_review = e.read(old_review_path)
raw_meta_path = HERE / 'native-parallel-wave6-final-patch.json'
raw_meta = e.read(raw_meta_path)
assert old_review['head'] == raw_meta['head'] and old_review['sha256'] == raw_meta['sha256']
assert old_review['verdict'] == 'fail'
terminal_path = HERE / 'repair-stage1-terminal-audit.json'
terminal = e.read(terminal_path)
raw_path = HERE / 'repair-stage1/compile-results.json'
assert e.sha(raw_path) == terminal['report_sha256']
assert e.sha(raw_meta_path) == terminal['metadata_sha256']
snapshot = e.common.source_snapshot()
changed, affected = e.closure(terminal['source_snapshot'], snapshot)
expected_paths = set(plan['successor_paths'])
assert changed == expected_paths, (sorted(changed), sorted(expected_paths))
native = {p for p in e.common.inventory() if Path(p).suffix.lower() in ('.cpp', '.c')}
assert native == set(raw_meta['native_inventory'])
closure = affected & native
extra = {p for p in native if Path(p).name in {'CgsSplicerContent.cpp', 'BrnDebugComponent.cpp'}}
sources = native if args.mode == 'full' else closure | extra
assert sources and expected_paths & native <= sources
patch = e.git('diff', '--binary', CONFIG['base'])
paths = e.git('diff', '--name-only', CONFIG['base']).decode().splitlines()
output = HERE / 'final2-repair'
temp = HERE / 'final2-repair-compiler-temp'
assert not output.exists() and not temp.exists()
meta_path = HERE / 'native-parallel-wave6-final2-patch.json'
meta = {'base': CONFIG['base'], 'head': head, 'workspace': str(WORK),
        'sha256': hashlib.sha256(patch).hexdigest(), 'paths': paths,
        'source_sha256': {p: e.sha(WORK / p) for p in paths},
        'source_snapshot': snapshot, 'native_inventory': sorted(native),
        'selection': args.mode, 'repair_paths': sorted(changed),
        'affected_sources': sorted(closure), 'selected_sources': sorted(sources),
        'additional_direct_callers_and_provider': sorted(extra - closure),
        'receipt_sha256': e.sha(HERE / 'wave6-receipts.json'),
        'full_sweep_head': raw_meta['head'], 'raw_report': str(raw_path),
        'previous_full_metadata': str(raw_meta_path), 'previous_full_metadata_sha256': e.sha(raw_meta_path),
        'raw_report_sha256': e.sha(raw_path), 'terminal_audit': str(terminal_path),
        'terminal_audit_sha256': e.sha(terminal_path),
        'prior_review_result': str(old_review_path), 'prior_review_result_sha256': e.sha(old_review_path),
        'external_inputs': terminal['external_inputs'], 'created_utc': e.now()}
assert e.external_inputs(e.read(raw_path)) == terminal['external_inputs']
e.save_new(meta_path, meta)
with (HERE / 'native-parallel-wave6-final2.diff').open('xb') as stream:
    stream.write(patch)
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
baseline = e.read(CONFIG['baseline_report'])
canonical = str(e.CANONICAL).replace('\\', '/').lower()
extra_flags = [a for a in baseline['results'][0]['command']
               if a.replace('\\', '/').lower().startswith('/i' + canonical + '/vendor/')]
driver = WORK / 'tools/build_x360.py'
arguments = [str(driver), '--sdk', baseline['sdk'], '--out', str(output),
             '--jobs', '8', '--timeout', '180', '--quiet']
arguments += ['--compiler-arg=' + a for a in extra_flags]
for source in sorted(sources):
    arguments += ['--source', source]
e.save_new(HERE / 'final2-repair-command.json', {
    'head': head, 'argv': arguments, 'driver_sha256': e.sha(driver),
    'private_temp': str(temp), 'metadata_sha256': e.sha(meta_path)})
owner_path = e.SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
owner = e.read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave6_final2_repair_native_build', frozen_head=head,
             live_process_id=os.getpid(), live_session=None, live_session_kind='Actual native repair compiler',
             live_report=str(output / 'compile-results.json'),
             instruction='Exact successor source frozen during actual native consumer build. Poll same handle to terminal; do not restart or mutate source.')
owner_path.write_text(json.dumps(owner, indent=2) + '\n', encoding='utf-8')
print('FROZEN', head, 'mode', args.mode, 'changed inputs', len(changed),
      'affected native', len(closure), 'actual sources', len(sources), 'PID', os.getpid(), flush=True)
driver_spec = importlib.util.spec_from_file_location('actual_native_driver', driver)
module = importlib.util.module_from_spec(driver_spec)
driver_spec.loader.exec_module(module)
sys.argv = arguments
code = module.main()
e.save_new(HERE / 'final2-repair-exit.json', {
    'head': head, 'exit_code': code, 'report': str(output / 'compile-results.json'),
    'report_sha256': e.sha(output / 'compile-results.json')})
raise SystemExit(code)
