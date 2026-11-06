"""Bind the actual successor consumer build and preserve the failed full-sweep layer."""
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('immutable_native_evidence', HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
config = e.read(HERE / 'campaign.json')
e.BASE = config['base']
name = 'native-parallel-wave6-final'
meta_path = HERE / (name + '-patch.json')
meta = e.read(meta_path)
e.check_current(meta)
terminal_path = HERE / 'full-stage1-terminal-audit.json'
terminal = e.read(terminal_path)
raw_path = HERE / 'full-stage1/compile-results.json'
raw = e.read(raw_path)
assert e.sha(raw_path) == terminal['report_sha256'] == meta['raw_report_sha256']
assert e.sha(terminal_path) == meta['terminal_audit_sha256']
e.verify_manifest(raw, terminal['artifact_manifest'])
repair_path = HERE / 'repair-stage1/compile-results.json'
repair, repairs, artifacts = e.common.report_check(repair_path)
assert set(repairs) == set(meta['selected_sources'])
changed, affected = e.closure(terminal['source_snapshot'], meta['source_snapshot'])
assert changed == set(meta['repair_paths'])
assert affected & set(meta['native_inventory']) == set(meta['affected_sources']) <= set(repairs)
assert e.external_inputs(repair) == terminal['external_inputs'] == meta['external_inputs']
exit_path = HERE / 'repair-stage1-exit.json'
exit_receipt = e.read(exit_path)
assert exit_receipt['head'] == meta['head'] and exit_receipt['report_sha256'] == e.sha(repair_path)
assert exit_receipt['exit_code'] == (1 if repair['failed'] else 0)
old_review_path = Path(meta['prior_review_result'])
assert e.sha(old_review_path) == meta['prior_review_result_sha256']
old_review = e.read(old_review_path)
assert old_review['verdict'] == 'fail' and old_review['head'] == meta['full_sweep_head']
assert e.sha(old_review['report']) == old_review['report_sha256']
assert e.sha(HERE / 'wave6-receipts.json') == meta['receipt_sha256']

repair_audit_path = HERE / 'repair-stage1-terminal-audit.json'
repair_audit = {
    'head': meta['head'], 'patch_sha256': meta['sha256'],
    'metadata_sha256': e.sha(meta_path), 'report': str(repair_path),
    'report_sha256': e.sha(repair_path), 'exit_receipt_sha256': e.sha(exit_path),
    'requested': repair['requested'], 'passed': repair['passed'], 'failed': repair['failed'],
    'artifact_manifest': artifacts, 'source_snapshot': meta['source_snapshot'],
    'external_inputs': terminal['external_inputs'], 'created_utc': e.now()}
e.save_new(repair_audit_path, repair_audit)
full_inventory_build = set(repairs) == set(meta['native_inventory'])
rows = [dict(repairs.get(row['source'], row),
             build_head=meta['head'] if row['source'] in repairs else meta['full_sweep_head'])
        for row in raw['results']]
effective = dict(raw, results=rows, passed=sum(row['passed'] for row in rows))
effective['failed'] = len(rows) - effective['passed']
layers = [{'head': meta['full_sweep_head'], 'report': str(raw_path),
           'report_sha256': e.sha(raw_path), 'terminal_audit': str(terminal_path),
           'terminal_audit_sha256': e.sha(terminal_path)},
          {'head': meta['head'], 'report': str(repair_path),
           'report_sha256': e.sha(repair_path), 'terminal_audit': str(repair_audit_path),
           'terminal_audit_sha256': e.sha(repair_audit_path)}]
effective['provenance'] = {
    'kind': ('Complete actual successor native sweep' if full_inventory_build else
             f'One complete {len(rows)}-source native sweep plus one {len(repairs)}-source actual consumer closure overlay'),
    'source_head': meta['head'], 'patch_sha256': meta['sha256'], 'layers': layers,
    'not_a_new_full_sweep': not full_inventory_build, 'input_invariance_verified': True,
    'closure_method': meta['source_snapshot']['method']}
effective_path = HERE / (name + '-effective.json')
e.save_new(effective_path, effective)
merged_artifacts = dict(terminal['artifact_manifest'], **artifacts)
manifest_path = HERE / (name + '-artifact-manifest.json')
e.save_new(manifest_path, {
    'head': meta['head'], 'patch_sha256': meta['sha256'],
    'effective_report': str(effective_path), 'effective_report_sha256': e.sha(effective_path),
    'layers': layers, 'artifact_manifest': merged_artifacts, 'created_utc': e.now()})
prior = e.read(config['baseline_report'])
assert e.sha(config['baseline_report']) == config['baseline_report_sha256']
summary = e.compare(effective, prior)
raw_index = {row['source']: row for row in raw['results']}
effective_index = {row['source']: row for row in rows}
raw_regressions = sorted(s for s, row in raw_index.items() if row['passed'] and not effective_index[s]['passed'])
raw_gains = sorted(s for s, row in raw_index.items() if not row['passed'] and effective_index[s]['passed'])
summary.update(retained_raw_full_passes=sum(row['passed'] and effective_index[s]['passed']
                                          for s, row in raw_index.items()),
               regressions_from_raw=raw_regressions, gains_from_raw=raw_gains)
e.save_new(HERE / 'final-compile-comparison.json', summary)
receipt_path = HERE / 'final-wave6-receipts.json'
with receipt_path.open('xb') as stream:
    stream.write((HERE / 'wave6-receipts.json').read_bytes())
completion = {
    'head': meta['head'], 'patch_sha256': meta['sha256'],
    'metadata': str(meta_path), 'metadata_sha256': e.sha(meta_path),
    'effective_report': str(effective_path), 'effective_report_sha256': e.sha(effective_path),
    'artifact_manifest': str(manifest_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'raw_failed_review_result': str(old_review_path), 'raw_failed_review_result_sha256': e.sha(old_review_path),
    'summary': summary, 'source_snapshot_hashes': len(meta['source_snapshot']['hashes']),
    'external_inputs_unchanged': True, 'actual_recompiled_sources': len(repairs),
    'all_conservative_native_consumers_recompiled': True,
    'final_receipts_sha256': e.sha(receipt_path),
    'verifier': str(Path(__file__)), 'verifier_sha256': e.sha(__file__), 'created_utc': e.now()}
e.save_new(HERE / 'final-binding-completion.json', completion)
print(json.dumps({k: v for k, v in summary.items() if k != 'new_passes'}, indent=2), flush=True)
print('NEW PASSES', len(summary['new_passes']), 'EFFECTIVE SHA256', e.sha(effective_path), flush=True)
assert summary['prior_passes_retained'] == config['baseline_passed']
assert summary['retained_raw_full_passes'] == raw['passed']
assert not summary['regressions'] and not raw_regressions and not summary['timeouts']
assert not summary['removed_sources']
