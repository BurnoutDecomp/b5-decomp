"""Refresh current operational metadata after the verified experimental publication."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
SCRATCH = PARENT / 'scratch/x360_sdk'
HERE = Path(__file__).resolve().parent
CANONICAL = PARENT / 'b5-decomp'
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(path, *args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=path, text=True).strip()


def current_write(path, value):
    path = Path(path).resolve()
    assert path.is_relative_to(SCRATCH.resolve())
    temp = path.with_name(path.name + '.wave5-publish.tmp')
    with temp.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(value)
    temp.replace(path)


receipt_path = HERE / 'wave5-final-publish-receipt.json'
receipt = read(receipt_path)
head = receipt['head']
assert head == '23445986c12b059f881bfaeb5081b127061b5aaa'
assert git(CANONICAL, 'rev-parse', 'HEAD') == git(WORK, 'rev-parse', 'HEAD') == head
assert git(CANONICAL, 'branch', '--show-current') == 'codex/experimental-x360-byte-match'
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
assert git(CANONICAL, 'ls-remote', '--heads', 'origin', receipt['branch']).split()[0] == head
owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
checkpoint_path = SCRATCH / 'current-native-checkpoint.json'
operation_path = SCRATCH / 'parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md'
coordination_path = SCRATCH / 'parallel-native/COORDINATION.md'
queue_path = SCRATCH / 'parallel-native/wave5_integrator_sol/post-sweep-fix-queue.json'
paths = [owner_path, checkpoint_path, operation_path, coordination_path, queue_path]
for path in paths:
    with (HERE / ('before-publication-bookkeeping-' + path.name)).open('xb') as stream:
        stream.write(path.read_bytes())
owner = read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
review_path = HERE / 'native-parallel-wave5-final-review-result.json'
review = read(review_path)
assert review['verdict'] == 'pass' and review['head'] == head
old_review_path = SCRATCH / 'native-parallel-wave5-full-stage1-review-result.json'
assert read(old_review_path)['verdict'] == 'fail'
assert sha(old_review_path) == 'fa4b655d4bc6a29827df71681a137e4cead83b35e841920c9a582c7ddf3695f9'
meta_path = HERE / 'native-parallel-wave5-final-patch.json'
meta = read(meta_path)
stamp = datetime.now(timezone.utc).isoformat()
owner.update(phase='wave5_published_checkpoint', published_head=head, frozen_head=head,
             canonical_head=head, live_session=None, live_process_id=None, live_report=None,
             effective_report=receipt['effective_report'], effective_report_sha256=receipt['effective_report_sha256'],
             publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
             review_result=str(review_path), review_result_sha256=sha(review_path),
             effective_passed=3341, effective_failed=269, goal='active', updated_utc=stamp,
             instruction='Wave5 checkpoint published and remote verified; canonical/intake clean at234459. '
                         'No jobs. Owner wave5_integrator_sol remains exclusive until parent explicitly transfers. '
                         'Do not replay collected repairs. Native goal remains active; later cohorts need a new cutoff/review.')
current_write(owner_path, json.dumps(owner, indent=2) + '\n')
checkpoint = read(checkpoint_path)
checkpoint.update(head=head, branch='codex/experimental-x360-byte-match', remote_verified=head,
                  effective_report=receipt['effective_report'], passed=3341, failed=269, requested=3610,
                  source_passed=3201, source_requested=3455, source_clean=True,
                  last_full_head=receipt['last_actual_full_head'], last_full_report=receipt['last_actual_full_report'],
                  last_full_requested=3610, last_full_passed=3335, last_full_failed=275, last_full_exit=1,
                  last_full_report_sha256=receipt['last_actual_full_report_sha256'],
                  jobs_running=[], goal='active', active_wave='wave5', pending_review=None,
                  patch_sha256=meta['sha256'], source_metadata=str(meta_path), source_metadata_sha256=sha(meta_path),
                  final_review=review['report'], final_review_result=str(review_path),
                  publication_receipt=str(receipt_path), publication_receipt_sha256=sha(receipt_path),
                  remaining_errors=str(HERE / 'remaining-native-parallel-wave5-final-errors.json'),
                  artifact_manifest=receipt['artifact_manifest'], artifact_manifest_sha256=receipt['artifact_manifest_sha256'],
                  effective_artifact_layers=2, effective_not_a_new_full_sweep=True,
                  retained_prior_passes=3233, retained_raw_full_passes=3335, new_passes=108,
                  regressions=[], timeouts=[], native_image_linked=False, native_image_launched=False,
                  staging_head=head, staging_patch_sha256=meta['sha256'], staging_receipts=101, staging_commits=101,
                  staging_reviewed=True, staging_phase='published and remote verified; checkpoint only, native goal active',
                  staging_effective_report=receipt['effective_report'], staging_effective_passed=3341, staging_effective_failed=269,
                  unreviewed_requested=None, unreviewed_passed=None, unreviewed_failed=None,
                  effective_unreviewed_report=None, primary_snapshot_fresh=False,
                  byte_match_report_is_historical=True,
                  native_comparisons_scope='Historical bounded helper count carried forward; fresh successor evidence and limits are in final_review.',
                  sdk_layout_comparisons_scope='Historical bounded layout count carried forward; not whole-game ABI proof.',
                  bounded_provider_proof=str(HERE / 'three-final-native-provider-proof.json'),
                  raw_full_source_review='fail; W5-F1/F2 fixed and separately approved by exact successor review',
                  updated_utc=stamp)
current_write(checkpoint_path, json.dumps(checkpoint, indent=2) + '\n')
queue = read(queue_path)
queue.update(status='collected and published', applied_successor_head=head,
             applied_commits=[r['source_commit'] for r in read(HERE / 'final-101-receipts.json')['receipts'][95:]],
             applied_source_paths=meta['repair_paths'], do_not_apply_until_actual_terminal=False,
             instruction='All ready_debug/traffic entries below are historical COLLECTED entries. Do not replay. '
                         'Next-batch candidates are in late-next-batch-candidates.json; parent sets the next cutoff.',
             publication_receipt=str(receipt_path), deferred_candidates=str(HERE / 'late-next-batch-candidates.json'), goal='active')
current_write(queue_path, json.dumps(queue, indent=2) + '\n')
summary = f'''# Current native integration checkpoint\n\nPublished and remote-verified `{head}` on `codex/experimental-x360-byte-match`. Canonical and detached intake are clean. Only the experimental ref was pushed; main/dev, the parent gitlink index and production ledger were not changed. Exclusive owner remains `/root/wave5_integrator_sol` until parent explicitly transfers; no jobs are running.\n\n- Effective native compilation: **3341/3610 pass,269 fail**, all3233previously published passes retained,108gains,zero regressions/timeouts. Source subset3201/3455; no native source was removed to hide failure.\n- Provenance: one actual COMPLETE3610-source sweep at9769 (3335pass/275fail) plus one actual8CPP successor build (7pass/1retained oldBridges failure). This is an effective overlay, **not a new full sweep**. All3335raw full-sweep passes are retained.\n- Exact final source:101commits,159changedpaths,6274source/header hashes; patch423ac79b28e17a72eb14e95c2208fbdf9ee8b62b3a41edb34ee6d6018b861ab1. All6274actual canonical input hashes were verified after FF, followed by exact remote verification. Parent gitlink index209fb3b94f7266506dff4ffd569e8553f1b3af93 is unchanged.\n- Same sole reviewer exact successor PASS: `{review['report']}`. Debug3D five-lane/unconditional native allocation and flag-before-raster fixes pass8original/compiled whole-caller cases; corrected Traffic passes the same5controlled cases.159+6274source hashes,101receipts,all10292effective artifact hashes and3341actual01F2objects were independently verified. Old9769 FAIL, raw reports and the checker count-assumption failure remain immutable. Historical23688positive cases are carried evidence, not fresh successor reruns.\n- Evidence and receipt: `{receipt_path}` SHA256 `{sha(receipt_path)}`. Exact final metadata/effective report/artifact manifest/audit/new review and remaining269failures live in `{HERE}`. Historical compiler inputs include10canonical fallback include roots,766files,clean exactEA submodule HEADs,and cl/c1/c1xx/c2+runtime fingerprints. The post-FF canonical observation is separate and does not imply a rebuild.\n- The eight-CPP cutoff is closed. LateGUIcdf14e8, Sound727f6ca6/Core, Shader/Sampler/StateHelper, privateglobalVector/layout/math/World/Replay/device and other later cohorts are nextbatch. Do not replay the six collected repairs. Preserve all private WIP and raw/FAIL evidence.\n\n**Whole native goal remains ACTIVE.**269sources still fail, including true Main/entry layout gates, oldAssertManager debt and genericBridges. Genuine layouts/callee ABI/globalVectorABI, missing native bodies, complete game-provider inventory, strict link, native image and actual launch remain open. No native game image has linked or launched. Successful compilation and bounded PPC software models do not prove whole gameplay or physical Xenon/cache/GPU/audio behavior.\n\n'''
current_write(operation_path, summary + '''Parent coordinates the next source ownership/cutoff. Use this published checkpoint as the new baseline; the original terminal audit, old95receipt/FAIL and all operation/checkpoint/owner histories remain immutable onD. Current `INTEGRATION_OWNER.json` is authoritative for mutation ownership.\n''')
old_coordination = (HERE / 'before-publication-bookkeeping-COORDINATION.md').read_text(encoding='utf-8')
current_write(coordination_path, summary + '\n---\n\n# Historical coordination snapshots\n\nEverything below is preserved historical context; its former current-state claims are not the current checkpoint.\n\n' + old_coordination)
assert read(checkpoint_path)['head'] == read(owner_path)['published_head'] == head
assert read(checkpoint_path)['goal'] == 'active' and not read(checkpoint_path)['jobs_running']
assert not git(CANONICAL, 'status', '--porcelain=v1') and not git(WORK, 'status', '--porcelain=v1')
assert git(PARENT, 'ls-files', '--stage', '--', 'b5-decomp') == receipt['parent_gitlink_index_before']
assert sha(SCRATCH / 'parallel-native/WAVE5_FULL_STAGE1_REVIEW_RESULT.md') == '3b03bdcbc72b3bbba4e3c0b4c034a419e4ec5974105e7d22a595f07b244e27fa'
assert sha(SCRATCH / 'native-parallel-wave5-full-stage1-review-result.json') == 'fa4b655d4bc6a29827df71681a137e4cead83b35e841920c9a582c7ddf3695f9'
record = {'head': head, 'publication_receipt': str(receipt_path), 'publication_receipt_sha256': sha(receipt_path),
          'current_files': {str(path): sha(path) for path in paths},
          'canonical_clean': True, 'intake_clean': True, 'parent_gitlink_index_unchanged': True,
          'old_fail_unchanged': True, 'jobs_running': [], 'goal': 'active', 'created_utc': stamp}
with (HERE / 'publication-bookkeeping-receipt.json').open('x', encoding='utf-8') as stream:
    json.dump(record, stream, indent=2)
print(json.dumps(record, indent=2))
