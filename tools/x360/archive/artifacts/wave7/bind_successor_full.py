"""Bind the actual new complete successor sweep; preserve the earlier exact FAIL."""
from datetime import datetime, timezone
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('immutable_native_evidence',
    HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
config = e.read(HERE / 'campaign.json')
e.BASE = config['base']
NAME = 'native-parallel-wave7-final'
meta_path = HERE / (NAME + '-patch.json')
meta = e.read(meta_path)
e.check_current(meta)
assert e.sha(meta_path) == e.sha(HERE / 'native-parallel-wave7-full-successor1-patch.json')
assert e.sha(HERE / (NAME + '.diff')) == e.sha(HERE / 'native-parallel-wave7-full-successor1.diff')
raw_meta_path = HERE / 'native-parallel-wave7-full-stage1-patch.json'
raw_meta = e.read(raw_meta_path)
old_audit_path = HERE / 'full-stage1-terminal-audit.json'
old_audit = e.read(old_audit_path)
assert e.sha(old_audit_path) == 'c5cc40080d41f5e10ca7f31843a22073455b9a2446bda731186ac364cbce7921'
old_report = e.read(old_audit['report'])
assert e.sha(old_audit['report']) == old_audit['report_sha256']
e.verify_manifest(old_report, old_audit['artifact_manifest'])
old_review_path = HERE / 'native-parallel-wave7-full-stage1-review-result.json'
old_review = e.read(old_review_path)
assert e.sha(old_review_path) == '88941a261c6c62fd598f27271e4b1706bdde589ce33edf5152a927666c33cf62'
assert old_review['verdict'] == 'fail' and old_review['head'] == raw_meta['head']
assert e.sha(old_review['report']) == old_review['report_sha256']
plan = e.read(HERE / 'intake-plan-04.json')
changed, affected = e.closure(old_audit['source_snapshot'], meta['source_snapshot'])
assert changed == set(plan['successor_paths']) and len(changed) == 8
assert len(meta['source_snapshot']['hashes']) == 6280
receipts_path = HERE / 'wave7-receipts.json'
receipts = e.read(receipts_path)
assert receipts['state'] == 'complete' and receipts['head'] == meta['head'] and len(receipts['receipts']) == 34
assert e.sha(receipts_path) == meta['receipt_sha256']
assert receipts['receipts'][:31] == e.read(HERE / 'frozen-stage1-31-receipts.json')['receipts']
report_path = HERE / 'full-successor1/compile-results.json'
report, index, artifacts = e.common.report_check(report_path)
assert set(index) == set(meta['native_inventory']) == set(meta['selected_sources'])
assert set(index) == {p for p in e.common.inventory() if Path(p).suffix.lower() in ('.c', '.cpp')}
assert len(index) == 3615
exit_path = HERE / 'full-successor1-exit.json'
exit_receipt = e.read(exit_path)
assert exit_receipt['head'] == meta['head'] and exit_receipt['report_sha256'] == e.sha(report_path)
assert exit_receipt['exit_code'] == (1 if report['failed'] else 0)
baseline = e.read(config['baseline_report'])
baseline_manifest = e.read(config['baseline_artifact_manifest'])
assert e.sha(config['baseline_report']) == config['baseline_report_sha256'] == baseline_manifest['effective_report_sha256']
assert e.sha(config['baseline_artifact_manifest']) == config['baseline_artifact_manifest_sha256']
e.verify_manifest(baseline, baseline_manifest['artifact_manifest'])
summary = e.compare(report, baseline)
assert summary['new_sources'] == sorted([
    'src/SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.cpp',
    'src/GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.cpp'])
assert not summary['removed_sources']
old_index = {r['source']: r for r in old_report['results']}
old_regressions = sorted(s for s, row in old_index.items() if row['passed'] and not index[s]['passed'])
summary['retained_old_full_passes'] = sum(row['passed'] and index[s]['passed'] for s, row in old_index.items())
summary['old_full_regressions'] = old_regressions
external = e.external_inputs(report)
assert external == old_audit['external_inputs']
audit_path = HERE / 'full-successor1-terminal-audit.json'
audit = {'head': meta['head'], 'base': meta['base'], 'patch_sha256': meta['sha256'],
    'metadata': str(meta_path), 'metadata_sha256': e.sha(meta_path),
    'report': str(report_path), 'report_sha256': e.sha(report_path),
    'exit_receipt_sha256': e.sha(exit_path), 'summary': summary,
    'artifact_manifest': artifacts, 'source_snapshot': meta['source_snapshot'], 'external_inputs': external,
    'previous_failed_review_sha256': e.sha(old_review_path), 'previous_raw_audit_sha256': e.sha(old_audit_path),
    'receipts_sha256': e.sha(receipts_path), 'verifier_sha256': e.sha(__file__), 'created_utc': e.now()}
e.save_new(audit_path, audit)
effective_path = HERE / (NAME + '-effective.json')
rows = [dict(row, build_head=meta['head']) for row in report['results']]
effective = dict(report, results=rows, provenance={
    'kind': 'Complete actual successor native inventory sweep', 'source_head': meta['head'],
    'patch_sha256': meta['sha256'], 'not_a_new_full_sweep': False,
    'layers': [{'head': meta['head'], 'report': str(report_path), 'report_sha256': e.sha(report_path),
                'terminal_audit': str(audit_path), 'terminal_audit_sha256': e.sha(audit_path)}],
    'previous_failed_head': raw_meta['head'], 'previous_failed_review_sha256': e.sha(old_review_path)})
e.save_new(effective_path, effective)
manifest_path = HERE / (NAME + '-artifact-manifest.json')
e.save_new(manifest_path, {'head': meta['head'], 'patch_sha256': meta['sha256'],
    'effective_report': str(effective_path), 'effective_report_sha256': e.sha(effective_path),
    'artifact_manifest': artifacts, 'layers': effective['provenance']['layers'], 'created_utc': e.now()})
with (HERE / 'final-wave7-receipts.json').open('xb') as stream:
    stream.write(receipts_path.read_bytes())
e.save_new(HERE / 'final-binding-completion.json', {
    'head': meta['head'], 'patch_sha256': meta['sha256'], 'metadata_sha256': e.sha(meta_path),
    'effective_report_sha256': e.sha(effective_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'terminal_audit_sha256': e.sha(audit_path), 'summary': summary,
    'changed_inputs_from_previous_full': sorted(changed),
    'affected_sources_from_previous_full': sorted(affected & set(meta['native_inventory'])),
    'all_affected_sources_actually_compiled': True, 'fresh_full_sweep': True,
    'previous_failed_review_sha256': e.sha(old_review_path), 'created_utc': e.now()})
print(json.dumps({k: v for k, v in summary.items() if k != 'new_passes'}, indent=2), flush=True)
print('BASELINE GAINS', len(summary['new_passes']), 'EFFECTIVE', e.sha(effective_path), flush=True)
assert summary['prior_passes_retained'] == config['baseline_passed']
assert summary['retained_old_full_passes'] == old_report['passed']
assert not summary['regressions'] and not old_regressions and not summary['timeouts']
