"""Publish only the exact approved full-native checkpoint to the experimental ref."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('immutable_native_evidence', HERE.parent / 'wave5-delivery/wave5_delivery.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
config = e.read(HERE / 'campaign.json')
e.BASE = config['base']
NAME = 'native-parallel-wave6-final'
BRANCH = 'codex/experimental-x360-byte-match'
REF = 'refs/heads/' + BRANCH


def git(cwd, *args):
    result = subprocess.run(['git', '--no-optional-locks', *args], cwd=cwd,
                            capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise RuntimeError((args, result.returncode, result.stdout, result.stderr))
    return result.stdout.strip()


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
assert not comparison['regressions'] and not comparison['timeouts']
completion_path = HERE / 'final-binding-completion.json'
completion = e.read(completion_path)
assert completion['head'] == meta['head'] and completion['metadata_sha256'] == e.sha(meta_path)
assert completion['summary']['retained_raw_full_passes'] == 3371
assert not completion['summary']['regressions_from_raw']
effective_path = HERE / (NAME + '-effective.json')
manifest_path = HERE / (NAME + '-artifact-manifest.json')
effective, manifest = e.read(effective_path), e.read(manifest_path)
assert e.sha(manifest_path) == completion['artifact_manifest_sha256']
assert e.sha(effective_path) == manifest['effective_report_sha256'] == completion['effective_report_sha256']
assert effective['provenance']['not_a_new_full_sweep'] is False
assert all(row['build_head'] == meta['head'] for row in effective['results'])
assert set(meta['selected_sources']) == set(meta['native_inventory'])
actual_full_path = HERE / 'repair-stage1/compile-results.json'
actual_full = e.read(actual_full_path)
assert set(row['source'] for row in actual_full['results']) == set(meta['native_inventory'])
assert actual_full['passed'] == effective['passed'] == comparison['passed']
assert actual_full['failed'] == effective['failed'] == comparison['failed']
assert actual_full['requested'] == comparison['requested'] == len(meta['native_inventory'])
full_audit_path = HERE / 'repair-stage1-terminal-audit.json'
full_audit = e.read(full_audit_path)
assert e.sha(actual_full_path) == full_audit['report_sha256']
assert full_audit['head'] == meta['head'] and full_audit['metadata_sha256'] == e.sha(meta_path)
assert e.sha(meta['raw_report']) == meta['raw_report_sha256']
assert e.sha(meta['prior_review_result']) == meta['prior_review_result_sha256']
assert e.read(meta['prior_review_result'])['verdict'] == 'fail'
receipts_path = HERE / 'final-wave6-receipts.json'
receipts = e.read(receipts_path)
assert e.sha(receipts_path) == meta['receipt_sha256'] == e.sha(HERE / 'wave6-receipts.json')
assert receipts['head'] == meta['head'] and len(receipts['receipts']) == 58
assert receipts['receipts'][:55] == e.read(HERE / 'frozen-stage1-55-receipts.json')['receipts']
assert e.external_inputs(actual_full) == full_audit['external_inputs'] == meta['external_inputs']
context = e.read(HERE / 'terminal-binding-context.json')
parent_index = git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp')
assert parent_index == context['parent_gitlink_index']
assert git(e.CANONICAL, 'branch', '--show-current') == BRANCH
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['base']
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
assert not git(e.WORK, 'status', '--porcelain=v1')
remote_before = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_before.split()[0] == meta['base']
assert set(git(e.WORK, 'diff', '--name-only', meta['base']).splitlines()) == set(meta['paths'])
assert all('pc' not in [part.lower() for part in Path(p).parts] for p in meta['paths'])
ownership()
preflight = {
    'head': meta['head'], 'base': meta['base'], 'branch': REF,
    'canonical': str(e.CANONICAL), 'workspace': str(e.WORK),
    'patch_sha256': meta['sha256'], 'metadata_sha256': e.sha(meta_path),
    'review_result_sha256': e.sha(review_path), 'review_report_sha256': review['report_sha256'],
    'audit_comparison_sha256': e.sha(comparison_path),
    'full_report_sha256': e.sha(actual_full_path), 'full_terminal_audit_sha256': e.sha(full_audit_path),
    'effective_report_sha256': e.sha(effective_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'binding_completion_sha256': e.sha(completion_path),
    'remote_before': remote_before, 'parent_gitlink_index_before': parent_index,
    'publisher_sha256': e.sha(__file__), 'created_utc': e.now()}
e.save_new(HERE / 'publication-preflight.json', preflight)
print('PREFLIGHT PASS', meta['head'], 'experimental-only FF/push', flush=True)
merge_output = git(e.CANONICAL, 'merge', '--ff-only', '--no-edit', meta['head'])
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['head']
assert git(e.CANONICAL, 'branch', '--show-current') == BRANCH
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
aggregate = hashlib.sha256()
for relative, expected in meta['source_snapshot']['hashes'].items():
    actual = e.sha(e.CANONICAL / relative)
    assert actual == expected, relative
    aggregate.update((relative + '\0' + actual + '\n').encode())
assert git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == parent_index
print('CANONICAL FF +', len(meta['source_snapshot']['hashes']), 'ACTUAL INPUT HASHES PASS', flush=True)
ownership()
push_output = git(e.CANONICAL, 'push', 'origin', 'HEAD:' + REF)
remote_after = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_after.split()[0] == meta['head']
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['head']
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
assert not git(e.WORK, 'status', '--porcelain=v1')
assert git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == parent_index
e.save_new(HERE / 'canonical-input-observation-after-publication.json', {
    'head': meta['head'], 'source_snapshot_hashes_verified': len(meta['source_snapshot']['hashes']),
    'source_input_aggregate_sha256': aggregate.hexdigest(), 'external_inputs': e.external_inputs(actual_full),
    'scope': 'Post-publication canonical observation; actual compiler inputs remain bound by the immutable terminal audits. No additional rebuild is implied.'})
receipt = dict(preflight, remote_verified=remote_after, canonical_clean=True, staging_clean=True,
    source_paths=len(meta['paths']), source_input_hashes_verified=len(meta['source_snapshot']['hashes']),
    canonical_source_input_aggregate_sha256=aggregate.hexdigest(), parent_gitlink_index_unchanged=True,
    intake_commits=58, effective_report=str(effective_path), artifact_manifest=str(manifest_path),
    passed=actual_full['passed'], failed=actual_full['failed'], requested=actual_full['requested'],
    source_passed=comparison['source_passed'], source_requested=comparison['source_requested'],
    retained_prior_passes=comparison['prior_passes_retained'], retained_raw_full_passes=3371,
    new_passes=len(comparison['new_passes']), regressions=[], timeouts=[], removed_sources=[],
    last_actual_full_head=meta['head'], last_actual_full_report=str(actual_full_path),
    last_actual_full_report_sha256=e.sha(actual_full_path), fresh_full_successor_sweep=True,
    not_a_new_full_sweep=False, prior_failed_full_head=meta['full_sweep_head'],
    prior_failed_full_report=meta['raw_report'], prior_failed_full_report_sha256=meta['raw_report_sha256'],
    historical_failed_review_result_sha256=meta['prior_review_result_sha256'],
    native_image_linked=False, native_image_launched=False, goal_status='active', published_utc=e.now())
e.save_new(HERE / 'wave6-final-publish-receipt.json', receipt)
e.save_new(HERE / 'publication-git-output.json', {'merge': merge_output, 'push': push_output})
print('PUBLISHED AND REMOTE VERIFIED', remote_after, flush=True)
print('RECEIPT', e.sha(HERE / 'wave6-final-publish-receipt.json'), flush=True)
