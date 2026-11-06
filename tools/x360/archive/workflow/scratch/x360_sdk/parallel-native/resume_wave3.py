"""Resume an explicitly resolved intake, stopping on the next real conflict."""
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2] / 'b5-decomp'
receipt_path = HERE / 'wave3-receipts.json'
receipts = json.loads(receipt_path.read_text())
plan = json.loads((HERE / 'wave3-intake-plan.json').read_text())


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args], text=True).strip()


def record(task, source, resolution=None):
    head = git('rev-parse', 'HEAD')
    row = {'task': task, 'source_commit': source, 'integrated_commit': head,
           'paths': git('diff-tree', '--no-commit-id', '--name-only', '-r', head).splitlines()}
    if resolution:
        row['resolution'] = resolution
    receipts['receipts'].append(row)
    receipts['head'] = head
    receipts.pop('pending_conflict', None)
    receipt_path.write_text(json.dumps(receipts, indent=2) + '\n')


assert git('branch', '--show-current') == plan['branch']
if receipts.get('pending_conflict'):
    pending = receipts['pending_conflict']
    assert git('rev-parse', 'CHERRY_PICK_HEAD') == pending['source_commit']
    assert not git('diff', '--name-only', '--diff-filter=U')
    subprocess.run(['git', '-C', str(ROOT), '-c', 'core.editor=true', 'cherry-pick', '--continue'], check=True)
    record(pending['task'], pending['source_commit'], sys.argv[1] if len(sys.argv) > 1 else 'Explicit reviewed resolution')
else:
    assert not git('status', '--porcelain')

done = {r['source_commit'] for r in receipts['receipts']}
for item in plan['commits']:
    source = item['source_commit']
    if source in done:
        continue
    print('INTAKE', item['task'], source, flush=True)
    status = subprocess.run(['git', '-C', str(ROOT), 'cherry-pick', source])
    if status.returncode:
        receipts['pending_conflict'] = {'task': item['task'], 'source_commit': source}
        receipt_path.write_text(json.dumps(receipts, indent=2) + '\n')
        print('STOP_CONFLICT', source, flush=True)
        sys.exit(status.returncode)
    record(item['task'], source)

print('INTEGRATED', len(receipts['receipts']), 'HEAD', receipts['head'],
      'STATUS', git('status', '--porcelain'), flush=True)
