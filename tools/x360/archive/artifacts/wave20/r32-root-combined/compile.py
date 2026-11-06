from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

out = Path(__file__).resolve().parent
wave = out.parent
batch = wave / 'r32-batch'
worktree = Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
intake = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base = 'a170b802168bebf7884c744d9f81f30ec258372a'
branch = 'codex/experimental-x360-r32-combined-preflight'

def git(*args, env=None, cwd=worktree, input_bytes=None):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=cwd, env=env, input=input_bytes)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

assert len(sys.argv) == 2 and sha(batch / 'MANIFEST.json') == sys.argv[1]
manifest = json.loads((batch / 'MANIFEST.json').read_text())
assert manifest['base'] == base and manifest['expected_total_native'] == 3695
assert git('rev-parse', 'HEAD', cwd=intake).decode().strip() == base
assert not git('status', '--porcelain=v1', cwd=intake).strip()
assert git('rev-parse', 'HEAD').decode().strip() == '33d79ef9b82b944b97ca6b5f450664aabb8073cf'
assert git('branch', '--show-current').decode().strip() == 'codex/experimental-x360-r32-native-collision-streams'
assert not git('status', '--porcelain=v1').strip()
assert not git('for-each-ref', '--format=%(objectname)', 'refs/heads/' + branch).strip()

env = dict(os.environ, GIT_INDEX_FILE=str(batch / 'projection.index'))
tree = git('write-tree', env=env).decode().strip()
assert tree == manifest['projected_tree']
patch = Path(manifest['patch']['path'])
assert sha(patch) == manifest['patch']['sha256']
assert git('diff', '--cached', '--binary', '--no-ext-diff', base, env=env) == patch.read_bytes()
message = b'Build the experimental X360 R32 combined preflight\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n'
head = git('commit-tree', tree, '-p', base, input_bytes=message).decode().strip()
git('switch', '--create', branch, head)
assert git('rev-parse', 'HEAD').decode().strip() == head
assert git('diff', '--binary', '--no-ext-diff', base, head) == patch.read_bytes()
assert not git('status', '--porcelain=v1').strip()
assert all(not (worktree / p).exists() for p in manifest['deleted_files'])

sources = {item['path'] for item in manifest['final_paths'] if item['new'] and Path(item['path']).suffix in ('.c', '.cpp')}
proofs = [
    Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/collision-stream-scheduler-root/native/compile-results.json'),
    Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/traffic-worker-native-root/native-final/compile-results.json'),
    Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave19/realmc-finish-luna/native-gate/r30-task-wrappers-final2/compile-results.json'),
    Path('D:/Reverse/BP-X360-Experimental-Worktrees/protomangle-control-terra/gate-frozen/compile-results.json'),
]
for proof in proofs:
    report = json.loads(proof.read_text(encoding='utf-8-sig'))
    assert report['failed'] == 0
    sources.update(row['source'].replace('\\', '/') for row in report['results'])
assert all((worktree / source).is_file() and not Path(source).is_absolute() for source in sources)
args = json.loads((wave / 'race-progression-native-root/command.json').read_text())
args = args[:args.index('--source')]
args[args.index('--out') + 1] = str(out / 'native')
args[args.index('--jobs') + 1] = '4'
for source in sorted(sources):
    args.extend(['--source', source])
(out / 'command.json').write_text(json.dumps(args, indent=2) + '\n', encoding='utf-8')
tmp = out / 'temp'
tmp.mkdir()
subprocess.run(args, cwd=worktree, env=dict(os.environ, TEMP=str(tmp), TMP=str(tmp)), check=True)
report_path = out / 'native/compile-results.json'
report = json.loads(report_path.read_text())
assert report['passed'] == len(sources) and report['failed'] == 0
assert all(row['machine'] == 0x1f2 for row in report['results'])
assert {row['source'].replace('\\', '/') for row in report['results']} == sources
assert not git('status', '--porcelain=v1').strip()
receipt = {
    'base': base, 'head': head, 'branch': branch, 'manifest_sha256': sys.argv[1],
    'patch_sha256': sha(patch), 'compile_sha256': sha(report_path), 'compiled': len(sources),
    'scope': 'Exact private combined affected-source compile. Full integrated compile and link remain pending.'
}
(out / 'DELIVERY.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print('R32 COMBINED', head, len(sources), sha(out / 'DELIVERY.json'), flush=True)
