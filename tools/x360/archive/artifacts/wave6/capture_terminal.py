"""Bind the actual complete Wave6 sweep, retaining its regression/failure history."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE / 'campaign.json').read_text())
spec = importlib.util.spec_from_file_location('immutable_wave5_evidence',
    HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
e.BASE = CONFIG['base']
metadata_path = HERE / 'native-parallel-wave6-full-stage1-patch.json'
meta = e.read(metadata_path)
assert e.git('rev-parse', 'HEAD').decode().strip() == meta['head']
assert not e.git('status', '--porcelain=v1').strip()
assert hashlib.sha256(e.git('diff', '--binary', meta['base'])).hexdigest() == meta['sha256']
assert e.common.source_snapshot() == meta['source_snapshot']
report_path = HERE / 'full-stage1/compile-results.json'
report, index, artifacts = e.common.report_check(report_path)
assert set(index) == set(meta['native_inventory']) == set(meta['selected_sources'])
expected = {p for p in e.common.inventory() if Path(p).suffix.lower() in ('.cpp', '.c')}
assert set(index) == expected
exit_path = HERE / 'full-stage1-exit.json'
terminal = e.read(exit_path)
assert terminal['head'] == meta['head']
assert terminal['report_sha256'] == e.sha(report_path)
assert terminal['exit_code'] == (1 if report['failed'] else 0)
baseline_path = Path(CONFIG['baseline_report'])
prior = e.read(baseline_path)
prior_manifest_path = Path(CONFIG['baseline_artifact_manifest'])
prior_manifest = e.read(prior_manifest_path)
assert e.sha(baseline_path) == CONFIG['baseline_report_sha256'] == prior_manifest['effective_report_sha256']
assert e.sha(prior_manifest_path) == CONFIG['baseline_artifact_manifest_sha256']
e.verify_manifest(prior, prior_manifest['artifact_manifest'])
summary = e.compare(report, prior)
assert summary['requested'] == 3613
assert summary['new_sources'] == sorted([
    'src/GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.cpp',
    'src/GameShared/GameClasses/Graphics/Dispatch/CgsShadowSamplerX360.cpp',
    'src/SDKs/RenderEngineClub/MAIN/components/src/statehelper.cpp'])
assert not summary['removed_sources']
snapshot = meta['source_snapshot']
assert snapshot['nonliteral_includes'] == [{'path': 'vendor/lua/src/lua.h', 'directive': 'LUA_USER_H'}]
external = e.external_inputs(report)
actual_bytes = sum(Path(row['object']).stat().st_size + Path(row['assembly']).stat().st_size
                   + Path(row['log']).stat().st_size for row in report['results'] if row['passed'])
result = {'head': meta['head'], 'base': meta['base'], 'patch_sha256': meta['sha256'],
          'metadata': str(metadata_path), 'metadata_sha256': e.sha(metadata_path),
          'report': str(report_path), 'report_sha256': e.sha(report_path),
          'exit_receipt': str(exit_path), 'exit_receipt_sha256': e.sha(exit_path),
          'baseline_report': str(baseline_path), 'baseline_report_sha256': e.sha(baseline_path),
          'baseline_artifact_manifest_sha256': e.sha(prior_manifest_path),
          'summary': summary, 'artifact_manifest': artifacts, 'source_snapshot': snapshot,
          'external_inputs': external, 'successful_object_listing_log_bytes': actual_bytes,
          'requires_successor_for_regressions': bool(summary['regressions']),
          'scope': 'Complete actual native inventory sweep. Raw failures/regressions are preserved; '
                   'this audit does not waive the later zero-3341-prior-pass and exact-review-PASS publication gates.',
          'verifier': str(Path(__file__)), 'verifier_sha256': e.sha(__file__),
          'created_utc': datetime.now(timezone.utc).isoformat()}
with (HERE / 'full-stage1-terminal-audit.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({k: v for k, v in result.items()
                  if k not in ('artifact_manifest', 'source_snapshot', 'external_inputs')}, indent=2))
