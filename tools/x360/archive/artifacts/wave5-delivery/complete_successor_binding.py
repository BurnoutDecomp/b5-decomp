"""Complete the bound overlay after correcting its estimated pass-count assumption."""
import hashlib
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('successor_binding', HERE / 'wave5_successor.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
meta = e.read(e.META)
e.check_current(meta)
terminal = e.read(e.TERMINAL)
assert e.sha(e.TERMINAL) == meta['terminal_audit_sha256']
assert e.sha(e.RAW) == meta['raw_report_sha256'] == terminal['report_sha256']
raw = e.read(e.RAW)
repair_audit = e.read(HERE / 'repair-terminal-audit.json')
repair, repairs, repair_artifacts = e.common.report_check(e.REPAIR)
assert repair_artifacts == repair_audit['artifact_manifest']
assert repair_audit['report_sha256'] == e.sha(e.REPAIR)
assert repair_audit['source_snapshot'] == meta['source_snapshot']
assert set(repairs) == e.EXPECTED_REPAIRS and repair['passed'] == 7 and repair['failed'] == 1
assert [s for s, r in repairs.items() if not r['passed']] == [
    'src/GameShared/GameClasses/Gui/CgsGuiModuleBridges.cpp']
assert e.external_inputs(repair) == terminal['external_inputs']
effective_path = HERE / (e.NAME + '-effective.json')
manifest_path = HERE / (e.NAME + '-artifact-manifest.json')
effective = e.read(effective_path)
manifest = e.read(manifest_path)
expected_rows = [dict(repairs.get(r['source'], r),
                      build_head=meta['head'] if r['source'] in repairs else e.FROZEN)
                 for r in raw['results']]
assert effective['results'] == expected_rows
assert manifest['artifact_manifest'] == dict(terminal['artifact_manifest'], **repair_artifacts)
assert manifest['effective_report_sha256'] == e.sha(effective_path)
assert manifest['head'] == meta['head'] and manifest['patch_sha256'] == meta['sha256']
assert effective['provenance']['source_head'] == meta['head']
assert effective['provenance']['not_a_new_full_sweep'] is True
for layer in manifest['layers']:
    assert e.sha(layer['report']) == layer['report_sha256']
    assert e.sha(layer['terminal_audit']) == layer['terminal_audit_sha256']
full = {r['source']: r for r in raw['results']}
final = {r['source']: r for r in effective['results']}
expected_gains = e.EXPECTED_REPAIRS - {
    'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp',
    'src/GameShared/GameClasses/Gui/CgsGuiModuleBridges.cpp',
}
gains = {s for s, r in final.items() if r['passed'] and not full[s]['passed']}
assert gains == expected_gains and len(gains) == 6
assert all(final[s]['passed'] for s, r in full.items() if r['passed'])
assert effective['requested'] == len(final) == len(effective['results']) == 3610
assert effective['passed'] == sum(r['passed'] for r in final.values()) == 3335 + len(gains)
assert effective['failed'] == 3610 - effective['passed']
summary = e.compare(effective, e.read(e.BASELINE))
assert summary['passed'] == 3341 and summary['failed'] == 269
assert summary['prior_passes_retained'] == 3233 and len(summary['new_passes']) == 108
assert not summary['regressions'] and not summary['timeouts']
e.save_new(HERE / 'overlay-expected-count-failure.json', {
    'verifier': str(HERE / 'wave5_successor.py'),
    'verifier_sha256': e.sha(HERE / 'wave5_successor.py'),
    'phase': 'overlay', 'exit_code': 1, 'failed_line': 274,
    'failed_assertion': "summary['passed'] == 3340 and summary['failed'] == 270",
    'measured_passed': 3341, 'measured_failed': 269,
    'reason': 'The estimated gain count omitted TrafficRemove, which was a failure in the '
              'complete raw sweep and passed the actual repair build. The earlier verifier '
              'completed source/artifact/external binding before its final count assertion.',
    'source_or_compiler_failure': False,
    'raw_report_unchanged_sha256': e.sha(e.RAW),
})
e.save_new(HERE / 'final-compile-comparison.json', summary)
e.save_new(HERE / 'final-binding-completion.json', {
    'head': meta['head'], 'patch_sha256': meta['sha256'],
    'metadata': str(e.META), 'metadata_sha256': e.sha(e.META),
    'effective_report': str(effective_path), 'effective_report_sha256': e.sha(effective_path),
    'artifact_manifest': str(manifest_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'full_sweep_passes_retained': sum(r['passed'] for r in raw['results']),
    'exact_repaired_failures': sorted(gains), 'summary': summary,
    'source_snapshot_hashes': len(meta['source_snapshot']['hashes']),
    'external_inputs_unchanged': True,
    'verifier': str(Path(__file__)), 'verifier_sha256': e.sha(__file__),
    'created_utc': e.now(),
})
print(json.dumps({k: v for k, v in summary.items() if k != 'new_passes'}, indent=2))
print('NEW PASSES', len(summary['new_passes']), 'EFFECTIVE SHA256', e.sha(effective_path))
