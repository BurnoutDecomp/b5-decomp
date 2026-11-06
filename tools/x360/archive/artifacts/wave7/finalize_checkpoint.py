"""Reconcile current native metadata after the exact Wave7 publication."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
CANONICAL = PARENT / 'b5-decomp'
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(cwd, *args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=cwd, text=True).strip()


def write_current(path, value):
    path = Path(path)
    assert path.resolve().is_relative_to(SCRATCH.resolve()) or path == HERE / 'post-terminal-mandatory-regressions.json'
    temporary = path.with_name(path.name + '.wave7-publish.tmp')
    with temporary.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(value)
    temporary.replace(path)


receipt_path = HERE / 'wave7-final-publish-receipt.json'
receipt = read(receipt_path)
head = receipt['head']
assert head == '3534f7f16103a58ff904bbd1604e18e9934868f4'
assert git(CANONICAL, 'rev-parse', 'HEAD') == git(WORK, 'rev-parse', 'HEAD') == head
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(CANONICAL, 'branch', '--show-current') == 'codex/experimental-x360-byte-match'
assert git(CANONICAL, 'ls-remote', '--heads', 'origin', receipt['branch']).split()[0] == head
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
review_path = HERE / 'native-parallel-wave7-final-review-result.json'
review = read(review_path)
assert review['verdict'] == 'pass' and review['head'] == head and sha(review_path) == receipt['review_result_sha256']
meta_path = HERE / 'native-parallel-wave7-final-patch.json'
meta = read(meta_path)
assert meta['head'] == head and sha(meta_path) == receipt['metadata_sha256']
old_review_path = HERE / 'native-parallel-wave7-full-stage1-review-result.json'
assert read(old_review_path)['verdict'] == 'fail'
assert sha(old_review_path) == '88941a261c6c62fd598f27271e4b1706bdde589ce33edf5152a927666c33cf62'
owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
checkpoint_path = SCRATCH / 'current-native-checkpoint.json'
operation_path = SCRATCH / 'parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md'
coordination_path = SCRATCH / 'parallel-native/COORDINATION.md'
queue_path = HERE / 'post-terminal-mandatory-regressions.json'
paths = [owner_path, checkpoint_path, operation_path, coordination_path, queue_path]
for path in paths:
    with (HERE / ('before-publication-bookkeeping-' + path.name)).open('xb') as stream:
        stream.write(path.read_bytes())
stamp = datetime.now(timezone.utc).isoformat()
owner = read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave7_published_checkpoint', published_head=head, canonical_head=head, frozen_head=head,
    live_session=None, live_process_id=None, live_session_kind=None, live_report=None,
    effective_report=receipt['effective_report'], effective_report_sha256=receipt['effective_report_sha256'],
    effective_passed=receipt['passed'], effective_failed=receipt['failed'],
    source_metadata=str(meta_path), source_metadata_sha256=sha(meta_path),
    review_result=str(review_path), review_result_sha256=sha(review_path),
    published_review_result=str(review_path), published_review_result_sha256=sha(review_path),
    publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
    terminal_audit=str(HERE / 'full-successor1-terminal-audit.json'),
    terminal_audit_sha256=sha(HERE / 'full-successor1-terminal-audit.json'),
    pending_source_finding=None, pending_source_commit=None, repair_passed=None, repair_failed=None,
    goal='active', updated_utc=stamp,
    instruction='Wave7 exact3534 checkpoint published and remote verified; canonical/intake clean. No jobs. Freshfull3615 source/artifacts and same-reviewer PASS/originalaudit bound; all3374published+3397oldraw passes retained. Old765FAIL/negatives immutable. Whole native goal active; later candidates require a new cutoff. No queue replay.')
write_current(owner_path, json.dumps(owner, indent=2) + '\n')
checkpoint = read(checkpoint_path)
checkpoint.update(head=head, branch='codex/experimental-x360-byte-match', remote_verified=head,
    effective_report=receipt['effective_report'], passed=receipt['passed'], failed=receipt['failed'], requested=receipt['requested'],
    source_passed=receipt['source_passed'], source_requested=receipt['source_requested'], source_clean=True,
    last_full_head=head, last_full_report=receipt['last_actual_full_report'], last_full_requested=receipt['requested'],
    last_full_passed=receipt['passed'], last_full_failed=receipt['failed'], last_full_exit=1,
    last_full_report_sha256=receipt['last_actual_full_report_sha256'], jobs_running=[], goal='active', active_wave='wave7',
    pending_review=None, patch_sha256=meta['sha256'], source_metadata=str(meta_path), source_metadata_sha256=sha(meta_path),
    final_review=review['report'], final_review_result=str(review_path),
    publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
    remaining_errors=str(HERE / 'remaining-native-parallel-wave7-final-errors.json'),
    artifact_manifest=receipt['artifact_manifest'], artifact_manifest_sha256=receipt['artifact_manifest_sha256'],
    effective_artifact_layers=1, effective_not_a_new_full_sweep=False,
    retained_prior_passes=receipt['retained_prior_passes'], retained_raw_full_passes=receipt['retained_raw_full_passes'],
    new_passes=receipt['new_passes'], regressions=[], timeouts=[], native_image_linked=False, native_image_launched=False,
    staging_head=head, staging_base=meta['base'], staging_patch_sha256=meta['sha256'], staging_receipts=34, staging_commits=34,
    staging_receipts_file=str(HERE / 'final-wave7-receipts.json'), staging_reviewed=True,
    staging_phase='published and remote verified; native goal active', staging_live_session=None, staging_live_kind=None,
    staging_live_report=None, staging_pending_review_findings=[], staging_effective_report=receipt['effective_report'],
    staging_effective_passed=receipt['passed'], staging_effective_failed=receipt['failed'],
    staging_source_metadata=str(meta_path), staging_source_metadata_sha256=sha(meta_path),
    staging_terminal_audit=str(HERE / 'full-successor1-terminal-audit.json'),
    staging_terminal_audit_sha256=sha(HERE / 'full-successor1-terminal-audit.json'),
    unreviewed_requested=None, unreviewed_passed=None, unreviewed_failed=None, effective_unreviewed_report=None,
    wip_full_head=None, wip_full_report=None, wip_full_exit=None, wip_patch_sha256=None,
    primary_snapshot_fresh=False, byte_match_report_is_historical=True,
    native_comparisons_scope='Historical helper count; current fresh full-object witnesses and limits are in exact final_review.',
    sdk_layout_comparisons_scope='Historical helper count; not whole-image native ABI proof.',
    bounded_provider_proof=str(HERE / 'four-final-native-provider-proof.json'),
    raw_full_source_review='Old765 four-finding FAIL immutable; only exact3534 mandatory successor approved.', updated_utc=stamp)
write_current(checkpoint_path, json.dumps(checkpoint, indent=2) + '\n')
queue = read(queue_path)
queue.update(status='mandatory fixes collected, exact successor reviewed/published', applied_successor_head=head,
    publication_receipt=str(receipt_path), not_terminal=False, goal='active')
for item in queue.get('requires_postterminal_successor', []) + queue.get('source_review_findings', []):
    item['status'] = 'genuinely repaired and published only in exact3534 successor; old evidence remains immutable'
write_current(queue_path, json.dumps(queue, indent=2) + '\n')
summary = f'''# Current native Wave7 checkpoint

Published and remote-verified `{head}` on `codex/experimental-x360-byte-match`. Canonical and detached intake are clean. Exclusive owner remains `/root/wave5_integrator_sol` until root transfers. No jobs are running. Main/dev, parent gitlink index and production ledger are unchanged.

- Fresh full native result: **{receipt['passed']}/{receipt['requested']} pass, {receipt['failed']} fail**. All {receipt['retained_prior_passes']} published and {receipt['retained_raw_full_passes']} earlier raw passes retained; {receipt['new_passes']} baseline gains, zero regressions/timeouts. Source subset {receipt['source_passed']}/{receipt['source_requested']}; no source removed.
- Exact source: 34 new Wave7 receipts, 52 combined paths, 6280 input hashes. Patch `{meta['sha256']}`. This result is a NEW complete actual sweep at3534, not an assumed subset or overlay. Actual canonical input hashes and exact remote head were verified after FF.
- Same sole reviewer PASS: `{review['report']}`. Repaired GUI payload/caller types, genuine octree containment/result provider, and original IIR arithmetic boundaries pass bounded actual-object comparisons. Effective member128 alignment is inferred from two native contexts; original new16 request remains and real allocator alignment is unproved. IIR private noinline primitive adds one call/sample, so byte identity/performance are not claimed. Hardware/whole frame/query/audio claims remain bounded.
- Old765 FAIL, raw3397/217/3614, all four findings and negative witnesses remain immutable. Later candidates were not added. All Wave6 raw/FAIL/PASS/publication evidence is preserved.
- Receipt: `{receipt_path}` SHA256 `{sha(receipt_path)}`. Exact metadata, actual full report/audit, final artifact manifest, known-provider scan, original mandatory audit and remaining errors are in `{HERE}`.

**Whole native goal remains ACTIVE.** {receipt['failed']} files still fail. Main hard layout/entry guards, whole vector ABI, storage/providers, strict native link, image and launch remain open. No native image has linked or launched; software PPC tests do not prove physical CPU/cache/GPU/audio/worker behavior.

The Wave7 cutoff is closed. Do not replay34 receipts or automatically collect later registry/Scene375/Semaphore/Director/Math/VPU/GPU/Sun/contact/particle/formatter/Apt/StreamPool/OSGlobal/System work. Preserve private WIP. ARTIST PPC32 big-endian controls all behavior/layout/signatures including Apt, with DWARF corroboration; no PC/XB1 authority/srcpc build, fake padding/vslots/APIs/stubs, disabled guards or forced unresolved linking.
'''
write_current(operation_path, summary)
previous = (HERE / 'before-publication-bookkeeping-COORDINATION.md').read_text(encoding='utf-8-sig')
write_current(coordination_path, summary + '\n---\n\n# Historical coordination snapshots\n\nEarlier source/live-job claims below are historical; the checkpoint above is current.\n\n' + previous)
assert sha(old_review_path) == '88941a261c6c62fd598f27271e4b1706bdde589ce33edf5152a927666c33cf62'
assert read(checkpoint_path)['head'] == read(owner_path)['published_head'] == head
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
record = {'head': head, 'publication_receipt': str(receipt_path), 'publication_receipt_sha256': sha(receipt_path),
    'current_files': {str(p): sha(p) for p in paths}, 'canonical_clean': True, 'intake_clean': True,
    'parent_gitlink_index_unchanged': True, 'old_fail_unchanged': True, 'jobs_running': [], 'goal': 'active', 'created_utc': stamp}
with (HERE / 'publication-bookkeeping-receipt.json').open('x', encoding='utf-8') as stream:
    json.dump(record, stream, indent=2)
print('Published metadata reconciled; native goal ACTIVE; receipt', sha(HERE / 'publication-bookkeeping-receipt.json'))
