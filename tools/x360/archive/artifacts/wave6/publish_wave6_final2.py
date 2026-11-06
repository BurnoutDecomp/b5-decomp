"""Publish the exact reviewed full sweep plus one-CPP fidelity correction."""
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
NAME = 'native-parallel-wave6-final2'
BRANCH = 'codex/experimental-x360-byte-match'
REF = 'refs/heads/' + BRANCH
SPLICE = 'src/GameShared/GameClasses/Sound/Playback/Splicer/internal/SpliceObjects.cpp'


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
assert meta['base'] == config['base'] and meta['selection'] == 'closure'
assert meta['repair_paths'] == meta['affected_sources'] == [SPLICE]
assert set(meta['selected_sources']) == {SPLICE,
    'src/GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.cpp',
    'src/GameSource/Sound/BrnDebugComponent.cpp'}
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
completion_path = HERE / 'final2-binding-completion.json'
completion = e.read(completion_path)
assert completion['head'] == meta['head'] and completion['metadata_sha256'] == e.sha(meta_path)
raw_path = Path(meta['raw_report'])
raw = e.read(raw_path)
assert raw['state'] == 'complete' and set(row['source'] for row in raw['results']) == set(meta['native_inventory'])
assert completion['summary']['retained_raw_full_passes'] == raw['passed']
assert not completion['summary']['regressions_from_raw']
effective_path = HERE / (NAME + '-effective.json')
manifest_path = HERE / (NAME + '-artifact-manifest.json')
effective, manifest = e.read(effective_path), e.read(manifest_path)
assert e.sha(manifest_path) == completion['artifact_manifest_sha256']
assert e.sha(effective_path) == manifest['effective_report_sha256'] == completion['effective_report_sha256']
assert effective['provenance']['not_a_new_full_sweep'] is True
provider_proof = e.read(HERE / 'four-final2-native-provider-proof.json')
assert provider_proof['head'] == meta['head'] and provider_proof['report_sha256'] == e.sha(effective_path)
assert provider_proof['artifact_manifest_sha256'] == e.sha(manifest_path)
assert provider_proof['native_objects_scanned'] == effective['passed']
assert len(provider_proof['providers']) == 4 and all(len(rows) == 1 for rows in provider_proof['providers'].values())
repair_path = HERE / 'final2-repair/compile-results.json'
repair = e.read(repair_path)
assert repair['state'] == 'complete' and repair['passed'] == repair['requested'] == 3 and repair['failed'] == 0
repairs = {row['source']: row for row in repair['results']}
assert set(repairs) == set(meta['selected_sources'])
expected_rows = [dict(repairs.get(row['source'], row),
                     build_head=meta['head'] if row['source'] in repairs else meta['full_sweep_head'])
                 for row in raw['results']]
assert effective['results'] == expected_rows
assert raw['passed'] == effective['passed'] == comparison['passed']
assert raw['failed'] == effective['failed'] == comparison['failed']
assert raw['requested'] == comparison['requested'] == len(meta['native_inventory'])
full_audit = e.read(meta['terminal_audit'])
repair_audit_path = HERE / 'final2-repair-terminal-audit.json'
repair_audit = e.read(repair_audit_path)
assert e.sha(raw_path) == full_audit['report_sha256'] == meta['raw_report_sha256']
assert e.sha(meta['terminal_audit']) == meta['terminal_audit_sha256']
assert full_audit['head'] == meta['full_sweep_head']
assert e.sha(meta['previous_full_metadata']) == full_audit['metadata_sha256'] == meta['previous_full_metadata_sha256']
assert repair_audit['head'] == meta['head'] and repair_audit['metadata_sha256'] == e.sha(meta_path)
assert e.sha(repair_path) == repair_audit['report_sha256']
assert e.sha(meta['prior_review_result']) == meta['prior_review_result_sha256']
assert e.read(meta['prior_review_result'])['verdict'] == 'fail'
initial = e.read(meta['previous_full_metadata'])
assert e.sha(initial['prior_review_result']) == initial['prior_review_result_sha256']
assert e.read(initial['prior_review_result'])['verdict'] == 'fail'
receipts_path = HERE / 'final2-wave6-receipts.json'
receipts = e.read(receipts_path)
assert e.sha(receipts_path) == meta['receipt_sha256'] == e.sha(HERE / 'wave6-receipts.json')
assert receipts['head'] == meta['head'] and len(receipts['receipts']) == 59
assert receipts['receipts'][:58] == e.read(HERE / 'final-wave6-receipts.json')['receipts']
assert receipts['receipts'][-1]['source_commit'] == '66c859286a4f7bd51069eed775eeefc747caee8d'
assert git(e.WORK, 'rev-parse', 'HEAD:' + SPLICE) == 'eabadb71ad2ea5cc9959d21fedcb1bca3492cb55'
assert len(git(e.WORK, 'rev-list', meta['base'] + '..' + meta['head']).splitlines()) == 59
assert e.external_inputs(repair) == repair_audit['external_inputs'] == meta['external_inputs'] == full_audit['external_inputs']
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
    'full_report_sha256': e.sha(raw_path), 'full_terminal_audit_sha256': e.sha(meta['terminal_audit']),
    'actual_repair_report_sha256': e.sha(repair_path), 'actual_repair_terminal_audit_sha256': e.sha(repair_audit_path),
    'effective_report_sha256': e.sha(effective_path), 'artifact_manifest_sha256': e.sha(manifest_path),
    'binding_completion_sha256': e.sha(completion_path),
    'remote_before': remote_before, 'parent_gitlink_index_before': parent_index,
    'publisher_sha256': e.sha(__file__), 'created_utc': e.now()}
e.save_new(HERE / 'publication-final2-preflight.json', preflight)
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
e.save_new(HERE / 'canonical-final2-input-observation-after-publication.json', {
    'head': meta['head'], 'source_snapshot_hashes_verified': len(meta['source_snapshot']['hashes']),
    'source_input_aggregate_sha256': aggregate.hexdigest(), 'external_inputs': e.external_inputs(repair),
    'scope': 'Post-publication canonical observation; immutable actual compiler inputs remain in terminal audits. No additional rebuild is implied.'})
receipt = dict(preflight, remote_verified=remote_after, canonical_clean=True, staging_clean=True,
    source_paths=len(meta['paths']), source_input_hashes_verified=len(meta['source_snapshot']['hashes']),
    canonical_source_input_aggregate_sha256=aggregate.hexdigest(), parent_gitlink_index_unchanged=True,
    intake_commits=59, effective_report=str(effective_path), artifact_manifest=str(manifest_path),
    passed=effective['passed'], failed=effective['failed'], requested=effective['requested'],
    source_passed=comparison['source_passed'], source_requested=comparison['source_requested'],
    retained_prior_passes=comparison['prior_passes_retained'], retained_raw_full_passes=raw['passed'],
    new_passes=len(comparison['new_passes']), regressions=[], timeouts=[], removed_sources=[],
    last_actual_full_head=meta['full_sweep_head'], last_actual_full_report=str(raw_path),
    last_actual_full_report_sha256=e.sha(raw_path), last_actual_full_passed=raw['passed'], last_actual_full_failed=raw['failed'],
    actual_repair_head=meta['head'], actual_repair_report=str(repair_path), actual_repair_requested=3,
    actual_repair_passed=3, actual_repair_failed=0, fresh_full_successor_sweep=False,
    not_a_new_full_sweep=True, historical_failed_review_result_sha256=meta['prior_review_result_sha256'],
    initial_failed_review_result_sha256=initial['prior_review_result_sha256'],
    native_image_linked=False, native_image_launched=False, goal_status='active', published_utc=e.now())
e.save_new(HERE / 'wave6-final2-publish-receipt.json', receipt)
e.save_new(HERE / 'publication-final2-git-output.json', {'merge': merge_output, 'push': push_output})
print('PUBLISHED AND REMOTE VERIFIED', remote_after, flush=True)
print('RECEIPT', e.sha(HERE / 'wave6-final2-publish-receipt.json'), flush=True)
