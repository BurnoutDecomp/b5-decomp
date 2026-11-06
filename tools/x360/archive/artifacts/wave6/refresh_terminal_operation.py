"""Refresh operational state without changing published or frozen source evidence."""
from datetime import datetime, timezone
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRATCH = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
HEAD = '526adc27662c0c2aedf417980dcf824c989bb035'
REPORT_SHA = '003c01b2f078eae910c9cb9396bf259f15fc09392065581b131d2858fe08e845'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def save(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


owner_path = SCRATCH / 'parallel-native/INTEGRATION_OWNER.json'
owner = read(owner_path)
assert owner['owner'] == '/root/wave5_integrator_sol'
owner.update(live_session=63271, phase='wave6_full_terminal_artifact_audit',
             live_process_id=None, live_session_kind='Read-only artifact/input audit, not compiler',
             frozen_head=HEAD, updated_utc=datetime.now(timezone.utc).isoformat())
save(owner_path, owner)

checkpoint_path = SCRATCH / 'current-native-checkpoint.json'
checkpoint = read(checkpoint_path)
assert checkpoint['head'] == '23445986c12b059f881bfaeb5081b127061b5aaa'
assert checkpoint['passed'] == 3341 and checkpoint['failed'] == 269
backup = HERE / 'checkpoint-at-wave6-terminal-before-staging-refresh.json'
if not backup.exists():
    with backup.open('xb') as stream:
        stream.write(checkpoint_path.read_bytes())
checkpoint.update(active_wave='wave6', staging_head=HEAD, staging_base=checkpoint['head'],
                  staging_receipts=55, staging_commits=55, staging_reviewed=False,
                  staging_receipts_file=str(HERE / 'wave6-receipts.json'),
                  staging_phase='actual full terminal; read-only artifact/input binding and exact review pending',
                  staging_full_report=str(HERE / 'full-stage1/compile-results.json'),
                  staging_full_head=HEAD, staging_full_passed=3371, staging_full_failed=242,
                  staging_full_requested=3613, staging_full_exit=1,
                  staging_full_report_sha256=REPORT_SHA,
                  staging_patch_sha256='2193d1ec8f379575942fd4603eeb7eb828990fc5f99e53b35603cb26f4082f7a',
                  wip_full_report=str(HERE / 'full-stage1/compile-results.json'),
                  wip_full_head=HEAD, wip_full_exit=1,
                  wip_patch_sha256='2193d1ec8f379575942fd4603eeb7eb828990fc5f99e53b35603cb26f4082f7a',
                  unreviewed_passed=3371, unreviewed_failed=242, unreviewed_requested=3613,
                  pending_review=str(HERE / 'WAVE6_FULL_STAGE1_REVIEW.md'),
                  staging_terminal_audit=None, staging_terminal_audit_sha256=None,
                  staging_effective_report=None, staging_effective_passed=None,
                  staging_effective_failed=None, staging_regressions=2, jobs_running=[],
                  updated_utc=datetime.now(timezone.utc).isoformat())
save(checkpoint_path, checkpoint)

queue_path = HERE / 'post-terminal-regression-and-deferred-queue.json'
queue = read(queue_path)
queue['regressions'][0].update(commit='7df5b752d32c919d58b78c72a6ec9aa35b0687b3',
    status='ready independent repair; not collected until frozen binding/verdict',
    evidence='D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-material-526adc/material-checkpoint.json')
queue['regressions'][1]['status'] = ('Genuine shared RGBA8 definition home and native packing being proved; '
    'root authorized this exact dependency. No obsolete PS3 compatibility type or local duplicate.')
save(queue_path, queue)
print('Operational staging refreshed; published3341 baseline and frozen526adc unchanged.')
