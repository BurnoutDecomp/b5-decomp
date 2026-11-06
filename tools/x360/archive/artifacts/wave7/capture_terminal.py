"""Bind the actual complete Wave7 sweep and preserve any honest failures/regressions."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
config = json.loads((HERE / 'campaign.json').read_text())
spec = importlib.util.spec_from_file_location('immutable_native_evidence',
    HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
e.BASE = config['base']
meta_path = HERE / 'native-parallel-wave7-full-stage1-patch.json'
meta = e.read(meta_path)
e.check_current(meta)
report_path = HERE / 'full-stage1/compile-results.json'
report, index, artifacts = e.common.report_check(report_path)
assert set(index) == set(meta['native_inventory']) == set(meta['selected_sources'])
assert set(index) == {p for p in e.common.inventory() if Path(p).suffix.lower() in ('.cpp', '.c')}
assert report['requested'] == 3614
exit_path = HERE / 'full-stage1-exit.json'
terminal = e.read(exit_path)
assert terminal['head'] == meta['head'] and terminal['report_sha256'] == e.sha(report_path)
assert terminal['exit_code'] == (1 if report['failed'] else 0)
baseline_path = Path(config['baseline_report'])
prior = e.read(baseline_path)
baseline_manifest_path = Path(config['baseline_artifact_manifest'])
baseline_manifest = e.read(baseline_manifest_path)
assert e.sha(baseline_path) == config['baseline_report_sha256'] == baseline_manifest['effective_report_sha256']
assert e.sha(baseline_manifest_path) == config['baseline_artifact_manifest_sha256']
assert e.sha(config['baseline_metadata']) == config['baseline_metadata_sha256']
assert e.read(config['baseline_metadata'])['head'] == config['base']
e.verify_manifest(prior, baseline_manifest['artifact_manifest'])
summary = e.compare(report, prior)
assert summary['new_sources'] == ['src/SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.cpp']
assert not summary['removed_sources']
snapshot = meta['source_snapshot']
assert len(snapshot['hashes']) == 6279
assert snapshot['nonliteral_includes'] == [{'path': 'vendor/lua/src/lua.h', 'directive': 'LUA_USER_H'}]
receipt_path = HERE / 'wave7-receipts.json'
receipts = e.read(receipt_path)
assert receipts['head'] == meta['head'] and len(receipts['receipts']) == 31
assert e.sha(receipt_path) == meta['receipt_sha256']
context = {
    'head': meta['head'], 'base': meta['base'], 'patch_sha256': meta['sha256'],
    'metadata': str(meta_path), 'metadata_sha256': e.sha(meta_path),
    'report': str(report_path), 'report_sha256': e.sha(report_path),
    'exit_receipt': str(exit_path), 'exit_receipt_sha256': e.sha(exit_path),
    'baseline_report': str(baseline_path), 'baseline_report_sha256': e.sha(baseline_path),
    'baseline_artifact_manifest_sha256': e.sha(baseline_manifest_path),
    'summary': summary, 'artifact_manifest': artifacts, 'source_snapshot': snapshot,
    'external_inputs': e.external_inputs(report), 'receipts_sha256': e.sha(receipt_path),
    'intake_tree': e.git('rev-parse', 'HEAD^{tree}').decode().strip(),
    'parent_gitlink_index': e.git('ls-files', '--stage', '--', 'b5-decomp', cwd=e.PARENT).decode().strip(),
    'successful_object_listing_log_bytes': sum(Path(row[k]).stat().st_size for row in report['results']
                                              if row['passed'] for k in ('object', 'assembly', 'log')),
    'requires_successor_for_regressions': bool(summary['regressions']),
    'scope': 'Complete actual native inventory sweep. Raw errors and any source/compile regressions remain honest; this audit never waives exact sole-review PASS or zero3374-published-regression publication gates.',
    'verifier': str(Path(__file__)), 'verifier_sha256': e.sha(__file__),
    'common_verifier_sha256': e.sha(e.COMMON_PATH), 'created_utc': e.now()}
e.save_new(HERE / 'full-stage1-terminal-audit.json', context)
print(json.dumps({k: v for k, v in context.items() if k not in ('artifact_manifest', 'source_snapshot', 'external_inputs')}, indent=2), flush=True)
