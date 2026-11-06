import hashlib, json, os, pathlib, subprocess

OUT = pathlib.Path(__file__).resolve().parent
REPO = pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
EVIDENCE = OUT.parent.parent
BASE = 'a551dbc82a97d127f978750d891fafe92178147f'
GROUPS = (
    ('sku-decimal', 'fe57f5a7', '909c84f7', ['wave19/native-sku-and-decimal/DELIVERY.json']),
    ('resource-pool', '909c84f7', 'd49dc118', ['wave19/resource-pool-report/DELIVERY.json']),
    ('hlb', '6be513e1', '13906f42', ['wave19/hlb-finish-terra/RECEIPT.md']),
    ('world-host-removal', 'de34beaf', '479eb57f', ['wave18/world-host-cleanup-luna/RECEIPT.md']),
    ('render-device', '2f87015e', 'c2c4ab51', ['wave19/renderdevice-native-terra/native3/compile-results.json']),
    ('shadow-frame', '2f87015e', '62a858b6', ['wave19/shadow-frame-terra/native1/compile-results.json']),
    ('renderer-helpers', '49d8c133', '6f671737', ['wave19/renderer-render-sol/screenshot-gate/compile-results.json', 'wave19/renderer-render-sol/object-to-mesh-gate2/compile-results.json']),
)

def git(*args, env=None, check=True):
    p = subprocess.run(['git', '--no-optional-locks', *args], cwd=REPO, env=env,
                       capture_output=True, check=False)
    if check and p.returncode:
        raise RuntimeError(f'{args!r}: {p.stderr.decode(errors="replace")}')
    return p.stdout

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def tree_entry(tree, path):
    return git('ls-tree', tree, '--', path).decode().split('\t')[0].strip() or None

def projected_entry(env, path):
    line = git('ls-files', '--stage', '--', path, env=env).decode().split('\t')[0].strip()
    if not line:
        return None
    mode, oid, stage = line.split()
    assert stage == '0'
    return f'{mode} blob {oid}'

def main():
    assert git('rev-parse', 'HEAD').decode().strip() == BASE
    assert not git('status', '--porcelain=v1').strip()
    index = OUT / 'r24-probe.index'
    assert not index.exists()
    env = dict(os.environ, GIT_INDEX_FILE=str(index))
    git('read-tree', BASE, env=env)
    for name, start, end, _ in GROUPS:
        start, end = [git('rev-parse', x).decode().strip() for x in (start, end)]
        paths = git('diff', '--name-only', start, end).decode().splitlines()
        before = [(p, tree_entry(start, p), projected_entry(env, p)) for p in paths]
        bad = [x for x in before if x[1] != x[2]]
        if bad:
            for item in bad: print('PREIMAGE', name, *item)
            continue
        patch = OUT / (name + '.patch')
        patch.write_bytes(git('diff', '--no-ext-diff', '--binary', start, end))
        git('apply', '--cached', '--check', str(patch), env=env)
        git('apply', '--cached', str(patch), env=env)
        for p in paths:
            assert projected_entry(env,p) == tree_entry(end,p), (name,p)
        print('EXACT', name, len(paths))
    print('PROJECTED', len(git('diff','--cached','--name-only',BASE,env=env).splitlines()))
    print('R23 HEAD unchanged', git('rev-parse','HEAD').decode().strip())

if __name__ == '__main__':
    main()
