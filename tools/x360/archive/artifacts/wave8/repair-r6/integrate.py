"""Collect reviewed-scope worker candidates into the root-owned detached intake."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone

PARENT = Path('C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow')
WORKSPACE = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
STATE = Path(__file__).resolve().parent / 'repair-r6-receipts.json'
HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE / 'campaign.json').read_text(encoding='utf-8'))
PLAN = json.loads(Path(CONFIG['current_plan']).read_text(encoding='utf-8'))
ALLOWED = set(PLAN['collect_now'])


def git(*args, check=True):
    result = subprocess.run(['git', '-C', str(WORKSPACE), *args], capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    if check and result.returncode:
        raise RuntimeError((args, result.stdout, result.stderr))
    return result


def resolve(prefix):
    if len(prefix) == 40:
        assert git('cat-file', '-t', prefix).stdout.strip() == 'commit'
        return prefix
    assert re.fullmatch(r'[0-9a-f]{7,40}', prefix), prefix
    candidates = git('rev-parse', '--disambiguate=' + prefix).stdout.splitlines()
    commits = [value for value in candidates if git('cat-file', '-t', value).stdout.strip() == 'commit']
    assert len(commits) == 1, (prefix, commits)
    return commits[0]


def save():
    STATE.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')
    owner_path = PARENT / 'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json'
    owner = json.loads(owner_path.read_text(encoding='utf-8-sig'))
    assert owner['owner'] == '/root/wave5_integrator_sol'
    owner.update(phase='wave8_repair_r6_intake_' + state['state'], intake_head=state['head'],
                 frozen_head=None, source_freeze_status='not frozen; integration in progress',
                 live_process_id=None, live_session=None, live_report=None,
                 campaign_root=str(HERE), plan=CONFIG['current_plan'], receipts=str(STATE),
                 intake_receipts=len(state['receipts']), canonical_head=CONFIG['published_base'],
                 last_actual_full_head=CONFIG['base'], last_actual_full_report=CONFIG['baseline_report'],
                 last_actual_full_report_sha256=CONFIG['baseline_report_sha256'],
                 pending_source_commit=state.get('pending'), pending_source_owner=None,
                 pending_source_instruction='Resolve only the exact named R6 source union, preserving R5 and published edits. No stale file replacement.',
                 instruction='Wave8 R6 exact14-original intake,44paths, one failed Boundary removal. Source not frozen; no compiler. Preserve R5 full3105/511 and352published regressions plus published3534. No unlisted candidates/source changes/publication.',
                 updated_utc=datetime.now(timezone.utc).isoformat())
    owner_path.write_text(json.dumps(owner, indent=2) + '\n', encoding='utf-8')
    operation = PARENT / 'scratch/x360_sdk/parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md'
    operation.write_text(
        '# Current native Wave8 R6 repair integration\n\n'
        'Exclusive owner: `/root/wave5_integrator_sol`. Canonical remains published clean `' + CONFIG['published_base'] + '`.\n'
        'Detached intake: `' + state['head'] + '`; state **' + state['state'] + '**; ' + str(len(state['receipts'])) + ' NEW Wave8 receipts.\n'
        'Plan: `' + CONFIG['current_plan'] + '`. Receipt state: `' + str(STATE) + '`.\n\n'
        'No compiler is running and source is not frozen. Finish named source union and selected actual gates before ONE fresh full native diagnostic sweep.\n'
        'Collect only14 originals/44paths in the Root-reviewed R6 plan, preserving all R5/published edits. No newfiles; retire only failed BootLegalBoundary. All unlisted Volume/SAT/LineSeg/later18CPP/RootStuntPolygon/geometry work excluded.\n'
        'Current R5 full baseline remains3105/3616PASS/511FAIL/352published regressions; no new global result. Published3534 remains3400/3615. All prior raw FAIL/PASS evidence immutable.\n'
        'No canonical publication, force link, main/dev/parent-index or production-ledger mutation. Whole native goal ACTIVE; no linked image/launch.\n', encoding='utf-8')


def record(source, resolution=None, outcome='cherry-picked'):
    paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', source).stdout.splitlines()
    assert paths and all(path.startswith(('src/', 'vendor/')) for path in paths), paths
    state['head'] = git('rev-parse', 'HEAD').stdout.strip()
    row = {'source_commit': source, 'integrated_commit': state['head'],
           'paths': paths, 'outcome': outcome}
    if resolution:
        row['resolution'] = resolution
    state['receipts'].append(row)
    if state.get('todo') and state['todo'][0] == source:
        state['todo'].pop(0)
    state.pop('pending', None)
    save()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--commits', nargs='*', default=[])
parser.add_argument('--continue-resolved', action='store_true')
parser.add_argument('--resolution')
args = parser.parse_args()
state = json.loads(STATE.read_text(encoding='utf-8'))
assert str(WORKSPACE.resolve()).lower() == str(Path(state['workspace']).resolve()).lower()
assert git('branch', '--show-current').stdout.strip() == '', 'Intake must remain detached'
assert git('rev-parse', 'HEAD').stdout.strip() == state['head']

if args.continue_resolved:
    assert state['state'] == 'conflict' and args.resolution
    pending = state['pending']
    assert git('rev-parse', 'CHERRY_PICK_HEAD').stdout.strip() == pending
    assert not git('ls-files', '-u').stdout.strip(), 'Unresolved conflicts remain'
    changed = set(git('diff', '--cached', '--name-only').stdout.splitlines())
    original = set(git('diff-tree', '--no-commit-id', '--name-only', '-r', pending).stdout.splitlines())
    assert changed and changed <= original, (changed, original)
    git('-c', 'core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol',
        'diff', '--cached', '--check')
    result = git('-c', 'core.editor=true', 'cherry-pick', '--continue', check=False)
    print(result.stdout, result.stderr, flush=True)
    assert result.returncode == 0
    record(pending, args.resolution)
else:
    assert state['state'] == 'complete', state
    assert not git('status', '--porcelain=v1').stdout.strip(), 'Preserve unexpected WIP'

state.setdefault('todo', [])
seen = {row['source_commit'] for row in state['receipts']}
for prefix in args.commits:
    commit = resolve(prefix)
    assert commit in ALLOWED, ('Not in exact current Wave8 plan', commit)
    if commit not in seen and commit not in state['todo']:
        state['todo'].append(commit)
        state['queue'].append(commit)
state['state'] = 'integrating'
save()
while state['todo']:
    commit = state['todo'][0]
    paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).stdout.splitlines()
    assert paths and all(path.startswith(('src/', 'vendor/')) for path in paths), paths
    candidate = next(c for c in PLAN['candidates'] if c['commit'] == commit)
    for change in candidate['paths']:
        current = git('rev-parse', 'HEAD:' + change['path']).stdout.strip()
        assert current == change['preimage'], ('Changed preimage; stop before mutation', commit, change['path'], current, change['preimage'])
    result = git('cherry-pick', commit, check=False)
    print(result.stdout, result.stderr, flush=True)
    if result.returncode:
        state.update(state='conflict', pending=commit,
                     head=git('rev-parse', 'HEAD').stdout.strip())
        save()
        raise SystemExit(result.returncode)
    record(commit)
state['state'] = 'complete'
save()
print('Intake HEAD', state['head'], 'receipts', len(state['receipts']), flush=True)
