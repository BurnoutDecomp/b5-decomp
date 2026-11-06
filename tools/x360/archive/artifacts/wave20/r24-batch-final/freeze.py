import hashlib, json, os, pathlib, subprocess

OUT = pathlib.Path(__file__).resolve().parent
REPO = pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
EVIDENCE = OUT.parent.parent
BASE = 'a551dbc82a97d127f978750d891fafe92178147f'
GROUPS = (
    ('sku-decimal', 'fe57f5a7', '909c84f7', ['wave19/native-sku-and-decimal/DELIVERY.json'], []),
    ('resource-pool', '909c84f7', 'd49dc118', ['wave19/resource-pool-report/DELIVERY.json'], []),
    ('xam-endian', 'd49dc118', '015c30af', ['wave20/xam-endian-root/DELIVERY.json'], []),
    ('hlb', '6be513e1', '13906f42', ['wave19/hlb-finish-terra/RECEIPT.md'], []),
    ('connapi', '49467e93', '765ed52', ['wave17/network-platform-terra/connapi-control-receipt.json',
                                        'wave17/network-platform-terra/connapi-lifecycle-receipt.json'],
                                      ['vendor/dirtysdk/src/x360/connapi_x360.cpp', 'vendor/dirtysdk/src/x360/commudp_tick_x360.cpp']),
    ('world-host-removal', 'de34beaf', '479eb57f', ['wave18/world-host-cleanup-luna/RECEIPT.md'], []),
    ('render-device', '2f87015e', 'c2c4ab51', ['wave19/renderdevice-native-terra/native3/compile-results.json'], []),
    ('shadow-frame', '2f87015e', '62a858b6', ['wave19/shadow-frame-terra/objects/compile-results.json'], []),
    ('renderer-helpers', '49d8c133', '388e0721', ['wave19/renderer-render-sol/monitor-colour-gate/compile-results.json',
                                               'wave19/renderer-render-sol/object-to-mesh-gate2/compile-results.json'], []),
)

def git(*args, env=None):
    p = subprocess.run(['git', '--no-optional-locks', *args], cwd=REPO, env=env,
                       capture_output=True, check=False)
    if p.returncode:
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

def pinned(path):
    return {'path':str(path), 'sha256':digest(path)}

def main():
    assert git('rev-parse', 'HEAD').decode().strip() == BASE
    assert not git('status', '--porcelain=v1').strip()
    index = OUT / 'projection.index'
    assert not index.exists()
    env = dict(os.environ, GIT_INDEX_FILE=str(index))
    git('read-tree', BASE, env=env)
    groups = []
    for name, start, end, receipts, scope in GROUPS:
        start, end = [git('rev-parse', x).decode().strip() for x in (start, end)]
        paths = git('diff', '--name-only', start, end, '--', *scope).decode().splitlines()
        assert not scope or set(paths) == set(scope), (name,scope,paths)
        entries = [{'path':p, 'old':tree_entry(start,p), 'new':tree_entry(end,p)} for p in paths]
        for row in entries:
            assert projected_entry(env,row['path']) == row['old'], (name,row)
        patch = OUT / (name + '.patch')
        patch.write_bytes(git('diff', '--no-ext-diff', '--binary', start, end, '--', *scope))
        git('apply', '--cached', '--check', str(patch), env=env)
        git('apply', '--cached', str(patch), env=env)
        for row in entries:
            assert projected_entry(env,row['path']) == row['new'], (name,row)
        groups.append({'name':name,'base':start,'commit':end,'patch':pinned(patch),'paths':entries,
                       'receipts':[pinned(EVIDENCE/p) for p in receipts]})
        print('EXACT', name, len(paths), flush=True)
    paths = git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
    final_paths = [{'path':p,'old':tree_entry(BASE,p),'new':projected_entry(env,p)} for p in paths]
    assert len(final_paths)==23 and all(p['new'] for p in final_paths)
    patch = OUT / 'r24.patch'
    patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',BASE,env=env))
    git('apply','--index','--check',str(patch))
    assert git('rev-parse','HEAD').decode().strip() == BASE
    assert not git('status','--porcelain=v1').strip()
    manifest = {'schema':'r24-native-intake-v1','base':BASE,'source_root':str(REPO),
                'groups':groups,'final_paths':final_paths,'patch':pinned(patch)}
    output = OUT/'MANIFEST.json'
    output.write_text(json.dumps(manifest,indent=2)+'\n', encoding='utf-8')
    print('MANIFEST', digest(output),'PATCH',digest(patch),'NATIVE_ADDS',
          [p['path'] for p in final_paths if p['old'] is None])

if __name__ == '__main__':
    main()

