"""Refresh only current operational files after exact remote-verified publication."""
from datetime import datetime, timezone
import hashlib
import json
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


def write_current(path, text):
    path = Path(path)
    assert path.resolve().is_relative_to(SCRATCH.resolve()) or path == HERE / 'post-terminal-regression-and-deferred-queue.json'
    temporary = path.with_name(path.name + '.wave6-publish.tmp')
    with temporary.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(text)
    temporary.replace(path)


receipt_path = HERE / 'wave6-final2-publish-receipt.json'
receipt = read(receipt_path)
head = receipt['head']
assert head == '9388e2a58178bd4403a2b34353105b608a8feb08'
assert git(CANONICAL, 'rev-parse', 'HEAD') == git(WORK, 'rev-parse', 'HEAD') == head
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(CANONICAL, 'branch', '--show-current') == 'codex/experimental-x360-byte-match'
assert git(CANONICAL, 'ls-remote', '--heads', 'origin', receipt['branch']).split()[0] == head
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
review_path = HERE / 'native-parallel-wave6-final2-review-result.json'
review = read(review_path)
assert review['verdict'] == 'pass' and review['head'] == head
assert sha(review_path) == receipt['review_result_sha256']
meta_path = HERE / 'native-parallel-wave6-final2-patch.json'
meta = read(meta_path)
assert meta['head'] == head and sha(meta_path) == receipt['metadata_sha256']
historical = {
    HERE / 'native-parallel-wave6-full-stage1-review-result.json': '5125feb91fe864feb01aac3fe0bb2583c7777a7121b6f61022758ba9c80755e9',
    HERE / 'native-parallel-wave6-final-review-result.json': '8efd23edd3262deb28bd432a895d7f745d0940a83db338ebd8cab5fa2724f9ff'}
for path, digest in historical.items():
    assert sha(path) == digest and read(path)['verdict'] == 'fail'
owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
checkpoint_path = SCRATCH / 'current-native-checkpoint.json'
operation_path = SCRATCH / 'parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md'
coordination_path = SCRATCH / 'parallel-native/COORDINATION.md'
queue_path = HERE / 'post-terminal-regression-and-deferred-queue.json'
paths = [owner_path, checkpoint_path, operation_path, coordination_path, queue_path]
for path in paths:
    with (HERE / ('before-final2-publication-bookkeeping-' + path.name)).open('xb') as stream:
        stream.write(path.read_bytes())
stamp = datetime.now(timezone.utc).isoformat()
owner = read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(phase='wave6_published_checkpoint', published_head=head, frozen_head=head, canonical_head=head,
    live_session=None, live_process_id=None, live_session_kind=None, live_report=None,
    effective_report=receipt['effective_report'], effective_report_sha256=receipt['effective_report_sha256'],
    effective_passed=receipt['passed'], effective_failed=receipt['failed'],
    source_metadata=str(meta_path), source_metadata_sha256=sha(meta_path),
    publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
    review_result=str(review_path), review_result_sha256=sha(review_path),
    published_review_result=str(review_path), published_review_result_sha256=sha(review_path),
    terminal_audit=str(HERE / 'final2-repair-terminal-audit.json'), terminal_audit_sha256=sha(HERE / 'final2-repair-terminal-audit.json'),
    last_actual_full_head=receipt['last_actual_full_head'], last_actual_full_report=receipt['last_actual_full_report'],
    last_actual_full_report_sha256=receipt['last_actual_full_report_sha256'],
    repair_passed=3, repair_failed=0, pending_source_finding=None, pending_source_commit=None,
    goal='active', updated_utc=stamp,
    instruction='Wave6 exact checkpoint published and remote verified; canonical/intake clean9388. No jobs. Full d3fc3374/3613 plus3actualCPPs; all3341published passes retained. Both oldFAILs remain immutable. Native goal active:239failures/strictlink/image/launch remain. New cutoff required for all later registry candidates; do not replay59 receipts.')
write_current(owner_path, json.dumps(owner, indent=2) + '\n')
checkpoint = read(checkpoint_path)
checkpoint.update(head=head, branch='codex/experimental-x360-byte-match', remote_verified=head,
    effective_report=receipt['effective_report'], passed=receipt['passed'], failed=receipt['failed'], requested=receipt['requested'],
    source_passed=receipt['source_passed'], source_requested=receipt['source_requested'], source_clean=True,
    last_full_head=receipt['last_actual_full_head'], last_full_report=receipt['last_actual_full_report'],
    last_full_requested=receipt['requested'], last_full_passed=receipt['last_actual_full_passed'],
    last_full_failed=receipt['last_actual_full_failed'], last_full_exit=1,
    last_full_report_sha256=receipt['last_actual_full_report_sha256'],
    jobs_running=[], goal='active', active_wave='wave6', pending_review=None,
    patch_sha256=meta['sha256'], source_metadata=str(meta_path), source_metadata_sha256=sha(meta_path),
    final_review=review['report'], final_review_result=str(review_path),
    publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
    remaining_errors=str(HERE / 'remaining-native-parallel-wave6-final2-errors.json'),
    artifact_manifest=receipt['artifact_manifest'], artifact_manifest_sha256=receipt['artifact_manifest_sha256'],
    effective_artifact_layers=2, effective_not_a_new_full_sweep=True,
    retained_prior_passes=receipt['retained_prior_passes'], retained_raw_full_passes=receipt['retained_raw_full_passes'],
    new_passes=receipt['new_passes'], regressions=[], timeouts=[], native_image_linked=False, native_image_launched=False,
    staging_head=head, staging_patch_sha256=meta['sha256'], staging_receipts=59, staging_commits=59, staging_reviewed=True,
    staging_phase='published and remote verified; native goal active', staging_base=meta['base'],
    staging_receipts_file=str(HERE / 'final2-wave6-receipts.json'),
    staging_effective_report=receipt['effective_report'], staging_effective_passed=receipt['passed'], staging_effective_failed=receipt['failed'],
    staging_source_metadata=str(meta_path), staging_source_metadata_sha256=sha(meta_path),
    staging_live_session=None, staging_live_kind=None, staging_live_report=None, staging_pending_review_findings=[],
    staging_terminal_audit=str(HERE / 'final2-repair-terminal-audit.json'),
    staging_terminal_audit_sha256=sha(HERE / 'final2-repair-terminal-audit.json'),
    unreviewed_requested=None, unreviewed_passed=None, unreviewed_failed=None, effective_unreviewed_report=None,
    wip_full_head=None, wip_full_report=None, wip_full_exit=None, wip_patch_sha256=None,
    primary_snapshot_fresh=False, byte_match_report_is_historical=True,
    native_comparisons_scope='Historical helper count carried forward; current actual-object comparisons and all limits are in final_review.',
    sdk_layout_comparisons_scope='Historical helper count, not whole-game native ABI proof.',
    bounded_provider_proof=str(HERE / 'four-final2-native-provider-proof.json'),
    raw_full_source_review='Two historical FAILs preserved; W6-F1/F2/F3 genuinely repaired and approved only in exact final2 successor.',
    updated_utc=stamp)
write_current(checkpoint_path, json.dumps(checkpoint, indent=2) + '\n')
queue = read(queue_path)
queue.update(status='current closed cutoff collected and published', applied_successor_head=head,
    publication_receipt=str(receipt_path), do_not_apply_until_actual_terminal=False,
    next_batch_registry=str(HERE / 'NEXT_BATCH_CANDIDATES_ROOT.md'), goal='active')
for row in queue['regressions']:
    row['status'] = 'collected, genuinely repaired and exact successor reviewed/published'
queue['authorized_direct_caller_completion']['status'] = 'd014 collected, then source-equivalence defect repaired by66 and published'
queue['confirmed_successor_source_equivalence_fix']['status'] = '66 collected as9388; actual3CPP and mutation gates pass; same reviewer PASS; published'
write_current(queue_path, json.dumps(queue, indent=2) + '\n')
summary = f'''# Current native Wave6 checkpoint

Published and remote-verified `{head}` on `codex/experimental-x360-byte-match`. Canonical and detached intake are clean. Exclusive owner remains `/root/wave5_integrator_sol` until root explicitly transfers. No jobs are running. Main/dev, the parent gitlink index and production ledger are unchanged.

- Effective native result: **{receipt['passed']}/{receipt['requested']} pass, {receipt['failed']} fail**; all {receipt['retained_prior_passes']} published passes and all {receipt['retained_raw_full_passes']} current-full passes retained, {receipt['new_passes']} gains, zero regressions/timeouts. Source subset {receipt['source_passed']}/{receipt['source_requested']}; no source removed.
- Actual provenance: a complete 3613-source sweep at d3fc (3374/239), plus the actual three-CPP stable-input correction at9388 (3/3). Headers/other inputs are invariant. This is **not a fresh full sweep at9388**.
- Exact source: 59 new Wave6 receipts, 116 combined paths, 6278 input hashes, patch `{meta['sha256']}`. Canonical input hashes and remote were verified after FF. Both 526adc and d3fc FAILs, raw objects/logs/listings and mutation witnesses remain immutable.
- Same sole reviewer exact PASS: `{review['report']}`. Splice retains the original pointer across callbacks/Allocate and re-reads pointee contents. The actual control and mutation comparisons are bounded source-equivalence to the unchanged delegate, not a located ARTIST constructor. Material/Lion checks retain their finite/model/worker-attribution limits. All effective artifacts and four unique decorated provider signatures are bound; this is not a successful link.
- Publication receipt: `{receipt_path}` SHA256 `{sha(receipt_path)}`. Full metadata, effective artifact manifest, exact review, original mandatory audit and remaining failures are in `{HERE}`. Current compiler/include/submodule bindings are preserved there.

**Whole native goal remains ACTIVE.**239 source files still fail. Main hard layout/entry guards, genuine storage/bodies/global vector ABI, strict native link, image and launch remain open. No native image has linked or launched; software PPC comparisons do not prove physical Xenon/GPU/audio behavior.

The Wave6 cutoff is closed. Root next-batch registry is routing only; do not replay59 receipts or automatically collect any late candidate. Preserve private WIP. Native ARTIST PPC32 big-endian governs all behavior/layout/signatures including Apt; DWARF corroborates. No PC/XB1 authority, src/pc compilation, invented padding/vslots/APIs/stubs, disabled guards or forced unresolved linking.
'''
write_current(operation_path, summary)
previous_coord = (HERE / 'before-final2-publication-bookkeeping-COORDINATION.md').read_text(encoding='utf-8-sig')
write_current(coordination_path, summary + '\n---\n\n# Historical coordination snapshots\n\nEarlier source/live-job claims below are historical; the checkpoint above is current.\n\n' + previous_coord)
for path, digest in historical.items():
    assert sha(path) == digest and read(path)['verdict'] == 'fail'
assert read(checkpoint_path)['head'] == read(owner_path)['published_head'] == head
assert read(checkpoint_path)['goal'] == 'active'
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
record = {'head': head, 'publication_receipt': str(receipt_path), 'publication_receipt_sha256': sha(receipt_path),
    'current_files': {str(path): sha(path) for path in paths}, 'canonical_clean': True, 'intake_clean': True,
    'parent_gitlink_index_unchanged': True, 'historical_fails_unchanged': True, 'jobs_running': [], 'goal': 'active', 'created_utc': stamp}
with (HERE / 'publication-final2-bookkeeping-receipt.json').open('x', encoding='utf-8') as stream:
    json.dump(record, stream, indent=2)
print('Published checkpoint metadata reconciled; goal ACTIVE; receipt', sha(HERE / 'publication-final2-bookkeeping-receipt.json'))
