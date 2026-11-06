"""Compile only the exact bound eight-CPP successor, without overwriting the full sweep."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('successor_evidence', HERE / 'wave5_successor.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
meta = e.read(e.META)
e.check_current(meta)
assert set(meta['affected_sources']) == e.EXPECTED_REPAIRS
raw = e.read(e.RAW)
assert e.sha(e.RAW) == meta['raw_report_sha256']
assert e.external_inputs(raw) == meta['external_inputs']
output = HERE.parent / 'wave5-repair'
temp = HERE.parent / 'wave5-repair-compiler-temp'
assert not output.exists() and not temp.exists(), 'Repair evidence must be new and immutable'
temp.mkdir()
os.environ['TEMP'] = os.environ['TMP'] = str(temp)
driver = e.WORK / 'tools/build_x360.py'
arguments = [str(driver), '--sdk', raw['sdk'], '--out', str(output), '--jobs', '2',
             '--timeout', '180', '--quiet']
extra = [argument for argument in raw['results'][0]['command']
         if argument.replace('\\', '/').lower().startswith(
             '/i' + str(e.CANONICAL).replace('\\', '/').lower() + '/vendor/')]
arguments += ['--compiler-arg=' + argument for argument in extra]
for source in meta['affected_sources']:
    arguments += ['--source', source]
e.save_new(HERE / 'repair-command.json', {
    'head': meta['head'], 'patch_sha256': meta['sha256'], 'argv': arguments,
    'private_temp': str(temp), 'driver_sha256': e.sha(driver),
    'source_metadata_sha256': e.sha(e.META), 'verifier_sha256': e.sha(__file__),
})
owner_path = e.SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
owner = e.read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave5_eight_cpp_native_repair', frozen_head=meta['head'],
             live_process_id=os.getpid(), live_session=None,
             live_report=str(output / 'compile-results.json'),
             instruction='Source frozen during actual eight-CPP successor compilation. '
                         'Full raw3610 sweep remains immutable. Exclusive owner wave5_integrator_sol.')
owner_path.write_text(json.dumps(owner, indent=2) + '\n', encoding='utf-8')
print('FROZEN', meta['head'], 'repair_sources', len(meta['affected_sources']), 'PID', os.getpid(), flush=True)
driver_spec = importlib.util.spec_from_file_location('actual_native_compiler_driver', driver)
module = importlib.util.module_from_spec(driver_spec)
driver_spec.loader.exec_module(module)
sys.argv = arguments
code = module.main()
e.save_new(HERE / 'repair-exit.json', {
    'head': meta['head'], 'patch_sha256': meta['sha256'], 'exit_code': code,
    'report': str(output / 'compile-results.json'),
    'report_sha256': e.sha(output / 'compile-results.json'),
})
raise SystemExit(code)
