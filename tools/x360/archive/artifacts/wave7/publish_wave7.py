"""Publish only the exact approved fresh Wave7 native checkpoint."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('immutable_native_evidence', HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
config = e.read(HERE / 'campaign.json')
e.BASE = config['base']
NAME = 'native-parallel-wave7-final'
REF = 'refs/heads/codex/experimental-x360-byte-match'


def git(cwd, *args):
    r = subprocess.run(['git', '--no-optional-locks', *args], cwd=cwd, capture_output=True,
                       text=True, timeout=180)
    if r.returncode:
        raise RuntimeError((args, r.returncode, r.stdout, r.stderr))
    return r.stdout.strip()


def ownership():
    assert e.read(e.SCRATCH / 'parallel-native/INTEGRATION_OWNER.json')['owner'] == '/root/wave5_integrator_sol'


ownership()
meta_path = HERE / (NAME + '-patch.json')
meta = e.read(meta_path)
e.check_current(meta)
assert meta['base'] == config['base'] and meta['selection'] == 'full'
review_path = HERE / (NAME + '-review-result.json')
review = e.read(review_path)
assert review['verdict'] == 'pass' and review['head'] == meta['head']
assert review['base'] == meta['base'] and review['sha256'] == meta['sha256']
assert Path(review['workspace']) == e.WORK
assert e.sha(review['report']) == review['report_sha256']
assert 'VERDICT: pass' in Path(review['report']).read_text(encoding='utf-8')
comparison_path = HERE / (NAME + '-sweep-comparison.json')
comparison = e.read(comparison_path)
assert comparison['patch_sha256'] == meta['sha256']
assert comparison['prior_passes_retained'] == config['baseline_passed']
assert not comparison['regressions'] and not comparison['timeouts'] and not comparison['removed_sources']
completion_path = HERE / 'final-binding-completion.json'
completion = e.read(completion_path)
assert completion['head'] == meta['head'] and completion['metadata_sha256'] == e.sha(meta_path)
assert not completion['summary']['old_full_regressions']
old_audit_path = HERE / 'full-stage1-terminal-audit.json'
old_audit = e.read(old_audit_path)
assert e.sha(old_audit_path) == 'c5cc40080d41f5e10ca7f31843a22073455b9a2446bda731186ac364cbce7921'
assert completion['summary']['retained_old_full_passes'] == old_audit['summary']['passed']
old_review_path = HERE / 'native-parallel-wave7-full-stage1-review-result.json'
assert e.sha(old_review_path) == '88941a261c6c62fd598f27271e4b1706bdde589ce33edf5152a927666c33cf62'
assert e.read(old_review_path)['verdict'] == 'fail'
assert e.sha(old_audit['report']) == old_audit['report_sha256']
effective_path = HERE / (NAME + '-effective.json')
manifest_path = HERE / (NAME + '-artifact-manifest.json')
effective, manifest = e.read(effective_path), e.read(manifest_path)
assert e.sha(manifest_path) == completion['artifact_manifest_sha256']
assert e.sha(effective_path) == manifest['effective_report_sha256'] == completion['effective_report_sha256']
assert effective['provenance']['not_a_new_full_sweep'] is False
assert all(row['build_head'] == meta['head'] for row in effective['results'])
actual_path = HERE / 'full-successor1/compile-results.json'
actual = e.read(actual_path)
assert actual['state'] == 'complete'
assert set(meta['selected_sources']) == set(meta['native_inventory']) == {r['source'] for r in actual['results']}
assert actual['requested'] == comparison['requested'] == 3615
assert actual['passed'] == effective['passed'] == comparison['passed']
assert actual['failed'] == effective['failed'] == comparison['failed']
audit_path = HERE / 'full-successor1-terminal-audit.json'
audit = e.read(audit_path)
assert audit['head'] == meta['head'] and audit['metadata_sha256'] == e.sha(meta_path)
assert audit['report_sha256'] == e.sha(actual_path)
proof_path = HERE / 'four-final-native-provider-proof.json'
proof = e.read(proof_path)
assert proof['head'] == meta['head'] and proof['report_sha256'] == e.sha(effective_path)
assert proof['artifact_manifest_sha256'] == e.sha(manifest_path)
assert proof['native_objects_scanned'] == actual['passed']
assert len(proof['providers']) == 4 and all(len(rows) == 1 for rows in proof['providers'].values())
receipts_path = HERE / 'final-wave7-receipts.json'
receipts = e.read(receipts_path)
assert receipts['head'] == meta['head'] and len(receipts['receipts']) == 34
assert e.sha(receipts_path) == meta['receipt_sha256'] == e.sha(HERE / 'wave7-receipts.json')
assert receipts['receipts'][:31] == e.read(HERE / 'frozen-stage1-31-receipts.json')['receipts']
assert [r['source_commit'] for r in receipts['receipts'][31:]] == e.read(HERE / 'intake-plan-04.json')['new_commits']
assert len(git(e.WORK, 'rev-list', meta['base'] + '..' + meta['head']).splitlines()) == 34
assert e.external_inputs(actual) == audit['external_inputs'] == old_audit['external_inputs']
parent_index = git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp')
assert parent_index == old_audit['parent_gitlink_index']
assert git(e.CANONICAL, 'branch', '--show-current') == REF.removeprefix('refs/heads/')
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['base']
assert not git(e.CANONICAL, 'status', '--porcelain=v1') and not git(e.WORK, 'status', '--porcelain=v1')
remote_before = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_before.split()[0] == meta['base']
assert set(git(e.WORK, 'diff', '--name-only', meta['base']).splitlines()) == set(meta['paths'])
assert all('pc' not in [p.lower() for p in Path(path).parts] for path in meta['paths'])
ownership()
preflight = {'head': meta['head'], 'base': meta['base'], 'branch': REF,
    'canonical': str(e.CANONICAL), 'workspace': str(e.WORK), 'patch_sha256': meta['sha256'],
    'metadata_sha256': e.sha(meta_path), 'review_result_sha256': e.sha(review_path),
    'review_report_sha256': review['report_sha256'], 'audit_comparison_sha256': e.sha(comparison_path),
    'full_report_sha256': e.sha(actual_path), 'full_terminal_audit_sha256': e.sha(audit_path),
    'effective_report_sha256': e.sha(effective_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'binding_completion_sha256': e.sha(completion_path), 'provider_proof_sha256': e.sha(proof_path),
    'remote_before': remote_before, 'parent_gitlink_index_before': parent_index,
    'publisher_sha256': e.sha(__file__), 'created_utc': e.now()}
e.save_new(HERE / 'publication-preflight.json', preflight)
print('PREFLIGHT PASS', meta['head'], 'experimental-only FF/push', flush=True)
merge_output = git(e.CANONICAL, 'merge', '--ff-only', '--no-edit', meta['head'])
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['head']
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
aggregate = hashlib.sha256()
for path, expected in meta['source_snapshot']['hashes'].items():
    actual_hash = e.sha(e.CANONICAL / path)
    assert actual_hash == expected, path
    aggregate.update((path + '\0' + actual_hash + '\n').encode())
assert git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == parent_index
print('CANONICAL FF +', len(meta['source_snapshot']['hashes']), 'ACTUAL INPUT HASHES PASS', flush=True)
ownership()
push_output = git(e.CANONICAL, 'push', 'origin', 'HEAD:' + REF)
remote_after = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_after.split()[0] == meta['head']
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['head']
assert git(e.CANONICAL, 'branch', '--show-current') == REF.removeprefix('refs/heads/')
assert not git(e.CANONICAL, 'status', '--porcelain=v1') and not git(e.WORK, 'status', '--porcelain=v1')
assert git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == parent_index
e.save_new(HERE / 'canonical-input-observation-after-publication.json', {
    'head': meta['head'], 'source_snapshot_hashes_verified': len(meta['source_snapshot']['hashes']),
    'source_input_aggregate_sha256': aggregate.hexdigest(), 'external_inputs': e.external_inputs(actual),
    'scope': 'Post-publication canonical observation; actual compiler inputs remain in terminal audits, no additional rebuild implied.'})
receipt = dict(preflight, remote_verified=remote_after, canonical_clean=True, staging_clean=True,
    source_paths=len(meta['paths']), source_input_hashes_verified=len(meta['source_snapshot']['hashes']),
    canonical_source_input_aggregate_sha256=aggregate.hexdigest(), parent_gitlink_index_unchanged=True,
    intake_commits=34, effective_report=str(effective_path), artifact_manifest=str(manifest_path),
    passed=actual['passed'], failed=actual['failed'], requested=actual['requested'],
    source_passed=comparison['source_passed'], source_requested=comparison['source_requested'],
    retained_prior_passes=comparison['prior_passes_retained'], retained_raw_full_passes=old_audit['summary']['passed'],
    new_passes=len(comparison['new_passes']), regressions=[], timeouts=[], removed_sources=[],
    last_actual_full_head=meta['head'], last_actual_full_report=str(actual_path),
    last_actual_full_report_sha256=e.sha(actual_path), last_actual_full_passed=actual['passed'], last_actual_full_failed=actual['failed'],
    fresh_full_successor_sweep=True, not_a_new_full_sweep=False,
    historical_failed_full_head=old_audit['head'], historical_failed_full_report_sha256=old_audit['report_sha256'],
    historical_failed_review_result_sha256=e.sha(old_review_path), native_image_linked=False, native_image_launched=False,
    goal_status='active', published_utc=e.now())
e.save_new(HERE / 'wave7-final-publish-receipt.json', receipt)
e.save_new(HERE / 'publication-git-output.json', {'merge': merge_output, 'push': push_output})
print('PUBLISHED AND REMOTE VERIFIED', remote_after, flush=True)
print('RECEIPT', e.sha(HERE / 'wave7-final-publish-receipt.json'), flush=True)
