from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

out = Path(__file__).resolve().parent
wave = out.parent
previous = wave / 'r30-root-realmc-preflight'
worktree = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base = '145fce281539ce509c4dd1974f1e61d099650378'

def git(*args, env=None):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=worktree, env=env)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def pin(path):
    return {'path': str(path.resolve()), 'sha256': sha(path)}

def entries(data, staged=False):
    result = {}
    for line in data.decode().splitlines():
        value, path = line.split('\t', 1)
        if staged:
            mode, blob, stage = value.split()
            assert stage == '0'
            value = f'{mode} blob {blob}'
        result[path] = value
    return result

def clean():
    return git('rev-parse', 'HEAD').decode().strip() == base and not git('status', '--porcelain=v1').strip()

assert len(sys.argv) == 2 and clean()
prior = previous / 'PROJECTION.json'
assert sha(prior) == '8a9f5033886077742a017d1221b1a80fa48f84341b07c37f50a014563c8f64ee'
manifest = json.loads(prior.read_text())
specs = json.loads(Path(sys.argv[1]).read_text())
assert 2 <= len(specs) <= 3
index = out / 'projection.index'
assert not index.exists()
shutil.copyfile(previous / 'projection.index', index)
env = dict(os.environ, GIT_INDEX_FILE=str(index))
assert git('write-tree', env=env).decode().strip() == manifest['projected_tree']
current = entries(git('ls-files', '--stage', env=env), True)
initial = entries(git('ls-tree', '-r', base))
assert all(current.get(row['path']) == row['new'] for row in manifest['final_paths'])
groups = manifest['groups'][:]
for item in specs:
    name, parent, tip = item['name'], item['parent'], item['tip']
    assert git('rev-parse', tip + '^').decode().strip() == parent
    paths = git('diff', '--name-only', parent, tip).decode().splitlines()
    assert paths == item['paths'], (name, paths)
    pre = entries(git('ls-tree', '-r', parent, '--', *paths))
    post = entries(git('ls-tree', '-r', tip, '--', *paths))
    assert all(current.get(path) == pre.get(path) for path in paths), (name, paths)
    patch = out / (name + '.patch')
    patch.write_bytes(git('diff', '--binary', '--no-ext-diff', parent, tip, '--', *paths))
    git('apply', '--cached', '--check', str(patch), env=env)
    git('apply', '--cached', str(patch), env=env)
    current = entries(git('ls-files', '--stage', env=env), True)
    assert all(current.get(path) == post.get(path) for path in paths)
    groups.append({
        'name': name, 'base': parent, 'head': tip, 'paths': paths,
        'patch': pin(patch), 'receipts': [pin(Path(p)) for p in item['receipts']]
    })

paths = git('diff', '--cached', '--name-only', base, env=env).decode().splitlines()
final = [{'path': p, 'old': initial.get(p), 'new': current.get(p)} for p in paths]
new = [p for p in paths if p not in initial]
assert len(final) == 56 and len(new) == 9 and sum(p.endswith('.cpp') for p in new) == 4
patch = out / 'r30.patch'
patch.write_bytes(git('diff', '--cached', '--binary', '--no-ext-diff', base, env=env))
git('apply', '--index', '--check', str(patch))
assert clean()
m = {
    **manifest, 'schema': 'r30-native-intake-v1', 'scope': 'Frozen R30 source candidate against the exact R29 checkpoint. No R30 integrated link.',
    'realm_projection': pin(prior), 'projected_tree': git('write-tree', env=env).decode().strip(),
    'groups': groups, 'patch': pin(patch), 'final_paths': final, 'new_files': new,
    'expected_total_native': 3695,
    'ginsu_exact_function': pin(wave / 'ginsu-rtti-native-root/ATTACH_CONTROLLER_COFF_MATCH.json'),
    'generated_loading_contract': pin(wave / 'r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json'),
    'base_loading_coff': pin(wave / 'r29-intake-repaired/loading-coff-verification.json')
}
with (out / 'MANIFEST.json').open('x', encoding='utf-8') as stream:
    json.dump(m, stream, indent=2)
    stream.write('\n')
print('R30 BATCH', len(final), len(groups), sha(out / 'MANIFEST.json'), sha(patch), flush=True)
