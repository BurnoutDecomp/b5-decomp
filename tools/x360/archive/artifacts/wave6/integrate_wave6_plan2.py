"""Collect reviewed-scope worker candidates into the root-owned detached intake."""
import argparse
import json
from pathlib import Path
import re
import subprocess

PARENT = Path('C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow')
WORKSPACE = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
STATE = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave6\\wave6-receipts.json')
PLAN = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave6\\intake-plan-02.json').read_text(encoding='utf-8'))
ALLOWED = set(PLAN['collect_now'])


def git(*args, check=True):
    result = subprocess.run(['git', '-C', str(WORKSPACE), *args], capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    if check and result.returncode:
        raise RuntimeError((args, result.stdout, result.stderr))
    return result


def resolve(prefix):
    assert re.fullmatch(r'[0-9a-f]{7,40}', prefix), prefix
    candidates = git('rev-parse', '--disambiguate=' + prefix).stdout.splitlines()
    commits = [value for value in candidates if git('cat-file', '-t', value).stdout.strip() == 'commit']
    assert len(commits) == 1, (prefix, commits)
    return commits[0]


def save():
    STATE.write_text(json.dumps(state, indent=2) + '\n', encoding='utf-8')


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
    assert commit in ALLOWED, ('Not in exact current Wave6 plan', commit)
    if commit not in seen and commit not in state['todo']:
        state['todo'].append(commit)
        state['queue'].append(commit)
state['state'] = 'integrating'
save()
while state['todo']:
    commit = state['todo'][0]
    paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', commit).stdout.splitlines()
    assert paths and all(path.startswith(('src/', 'vendor/')) for path in paths), paths
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
