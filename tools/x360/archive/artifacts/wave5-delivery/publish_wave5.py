"""Publish the exact approved native checkpoint to its experimental branch only."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('native_final_binding', HERE / 'wave5_successor.py')
e = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e)
BRANCH = 'codex/experimental-x360-byte-match'
REF = 'refs/heads/' + BRANCH


def git(cwd, *args):
    result = subprocess.run(['git', '--no-optional-locks', *args], cwd=cwd,
                            capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise RuntimeError((args, result.returncode, result.stdout, result.stderr))
    return result.stdout.strip()


def ownership():
    current = e.read(e.SCRATCH / 'parallel-native/INTEGRATION_OWNER.json')
    assert current['owner'] == '/root/wave5_integrator_sol', current


ownership()
meta = e.read(e.META)
e.check_current(meta)
review_path = HERE / (e.NAME + '-review-result.json')
review = e.read(review_path)
assert review['verdict'] == 'pass'
assert review['head'] == meta['head'] and review['base'] == meta['base']
assert review['sha256'] == meta['sha256'] and Path(review['workspace']) == e.WORK
assert e.sha(review['report']) == review['report_sha256']
assert 'VERDICT: pass' in Path(review['report']).read_text(encoding='utf-8')
comparison_path = HERE / (e.NAME + '-sweep-comparison.json')
comparison = e.read(comparison_path)
assert comparison['patch_sha256'] == meta['sha256']
assert comparison['passed'] == 3341 and comparison['failed'] == 269
assert comparison['requested'] == 3610 and comparison['prior_passes_retained'] == 3233
assert len(comparison['new_passes']) == 108
assert not comparison['regressions'] and not comparison['timeouts']
effective_path = HERE / (e.NAME + '-effective.json')
manifest_path = HERE / (e.NAME + '-artifact-manifest.json')
manifest = e.read(manifest_path)
assert manifest['head'] == meta['head'] and manifest['patch_sha256'] == meta['sha256']
assert e.sha(effective_path) == manifest['effective_report_sha256']
assert e.sha(e.RAW) == meta['raw_report_sha256']
context = e.read(HERE / 'terminal-binding-context.json')
parent_index = git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp')
assert parent_index == context['parent_gitlink_index']
assert git(e.CANONICAL, 'branch', '--show-current') == BRANCH
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['base']
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
remote_before = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_before.split()[0] == meta['base']
assert set(git(e.WORK, 'diff', '--name-only', meta['base']).splitlines()) == set(meta['paths'])
assert all('pc' not in [part.lower() for part in Path(p).parts] for p in meta['paths'])
ownership()
preflight = {
    'head': meta['head'], 'base': meta['base'], 'branch': REF,
    'canonical': str(e.CANONICAL), 'workspace': str(e.WORK),
    'patch_sha256': meta['sha256'], 'metadata_sha256': e.sha(e.META),
    'review_result_sha256': e.sha(review_path), 'review_report_sha256': review['report_sha256'],
    'audit_comparison_sha256': e.sha(comparison_path),
    'effective_report_sha256': e.sha(effective_path),
    'artifact_manifest_sha256': e.sha(manifest_path),
    'remote_before': remote_before, 'parent_gitlink_index_before': parent_index,
    'publisher_sha256': e.sha(__file__), 'created_utc': datetime.now(timezone.utc).isoformat(),
}
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
print('CANONICAL FF +6274 ACTUAL INPUT HASHES PASS', flush=True)
ownership()
push_output = git(e.CANONICAL, 'push', 'origin', 'HEAD:' + REF)
remote_after = git(e.CANONICAL, 'ls-remote', '--heads', 'origin', REF)
assert remote_after.split()[0] == meta['head']
assert git(e.CANONICAL, 'rev-parse', 'HEAD') == meta['head']
assert not git(e.CANONICAL, 'status', '--porcelain=v1')
assert not git(e.WORK, 'status', '--porcelain=v1')
assert git(e.PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == parent_index
# A canonical fallback directory may now contain the newly published headers.
# Keep this post-publication observation separate from the actual historical
# compiler-input capture; no compiler rerun is implied by the fast-forward.
post_inputs = e.external_inputs(e.read(e.RAW))
e.save_new(HERE / 'canonical-input-observation-after-publication.json', {
    'head': meta['head'], 'source_snapshot_hashes_verified': len(meta['source_snapshot']['hashes']),
    'source_input_aggregate_sha256': aggregate.hexdigest(),
    'external_inputs': post_inputs,
    'scope': 'Post-publication canonical source/header and conservative include-tree observation. '
             'Historical raw/repair compiler inputs remain in their immutable terminal audits; '
             'this observation does not claim a rebuild.'})
receipts = e.read(HERE / 'final-101-receipts.json')
assert receipts['head'] == meta['head'] and len(receipts['receipts']) == 101
objects = sum(Path(r['object']).stat().st_size + Path(r['assembly']).stat().st_size
              for r in e.read(effective_path)['results'] if r['passed'])
receipt = dict(preflight,
    remote_verified=remote_after, canonical_clean=True, staging_clean=True,
    source_paths=len(meta['paths']), source_input_hashes_verified=len(meta['source_snapshot']['hashes']),
    canonical_source_input_aggregate_sha256=aggregate.hexdigest(),
    parent_gitlink_index_unchanged=True, intake_commits=101,
    effective_report=str(effective_path), artifact_manifest=str(manifest_path),
    passed=3341, failed=269, requested=3610, source_passed=3201, source_requested=3455,
    retained_prior_passes=3233, retained_raw_full_passes=3335, new_passes=108,
    regressions=[], timeouts=[], removed_sources=[],
    last_actual_full_head=e.FROZEN, last_actual_full_report=str(e.RAW),
    last_actual_full_report_sha256=e.sha(e.RAW), last_actual_full_passed=3335,
    last_actual_full_failed=275, actual_repair_requested=8,
    actual_repair_passed=7, actual_repair_failed=1,
    not_a_new_full_sweep=True, actual_object_listing_bytes=objects,
    native_image_linked=False, native_image_launched=False, goal_status='active',
    published_utc=datetime.now(timezone.utc).isoformat())
e.save_new(HERE / 'wave5-final-publish-receipt.json', receipt)
e.save_new(HERE / 'publication-git-output.json', {'merge': merge_output, 'push': push_output})
print('PUBLISHED AND REMOTE VERIFIED', remote_after, flush=True)
print('RECEIPT', str(HERE / 'wave5-final-publish-receipt.json'),
      e.sha(HERE / 'wave5-final-publish-receipt.json'), flush=True)
