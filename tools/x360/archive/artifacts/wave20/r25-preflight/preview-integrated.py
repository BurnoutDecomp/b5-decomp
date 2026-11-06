import hashlib, json, os, pathlib, shutil, subprocess

OUT=pathlib.Path(__file__).resolve().parent
BASE=OUT.parent
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
ORIGINAL='c7376f3fed4105792eabdb9622d3ffefd7535009'
LAYER='src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp'
G=[
    ('xam-schema','015c30af','56799b4f'),
    ('gui-queue','56799b4f','7af1b432'),
    ('state-factories','7af1b432','3733740b'),
    ('ice-action-tables','3733740b','65300876'),
    ('device-access','c2c4ab51','53be630e'),
    ('mesh-context','a551dbc8','701c766f'),
    ('full-render','388e0721','4dacf56e'),
    ('particle-jobs','388e0721','7fac4b01'),
    ('im3d','4dacf56e','f3c8b699'),
    ('effects-vtable','4dacf56e','7d261ec7'),
    ('xnfriends','5d1cbec3','713e7ac8'),
]
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W,env=env)
def oid(rev,path):
    ls=git('ls-tree',rev,'--',path).decode().split()
    return ls[2] if ls else None
def idx(env,path):
    ls=git('ls-files','--stage','--',path,env=env).decode().split()
    return ls[1] if ls else None
assert git('rev-parse','HEAD').decode().strip()==ORIGINAL
assert not git('status','--porcelain=v1').strip()
index=OUT/'preview-integrated.index'
assert not index.exists()
env=dict(os.environ,GIT_INDEX_FILE=str(index))
git('read-tree',ORIGINAL,env=env)
r24=git('write-tree',env=env).decode().strip()
rows=[]
for name,parent,rev in G:
    paths=git('diff','--name-only',parent,rev).decode().splitlines()
    prior={p:idx(env,p) for p in paths}
    expected={p:oid(rev,p) for p in paths}
    for p in paths:
        if p==LAYER and name=='full-render':
            assert prior[p]==oid('701c766f',p)
        elif name=='effects-vtable':
            prior_blob=git('cat-file','blob',prior[p]); original_blob=git('cat-file','blob',oid(parent,p))
            assert original_blob.replace(b'class IOBufferStack;',b'struct IOBufferStack;')==prior_blob
        else:
            assert prior[p]==oid(parent,p),(name,p,prior[p],oid(parent,p))
    patch=OUT/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,rev))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    projected={p:idx(env,p) for p in paths}
    for p in paths:
        if p==LAYER and name=='full-render':
            evidence=(BASE.parent/'wave19/renderer-render-sol/dispatcher-provider-combined.cpp').read_bytes()
            assert git('cat-file','blob',projected[p])==evidence
        elif name=='effects-vtable':
            exact=git('cat-file','blob',expected[p]).replace(b'class IOBufferStack;',b'struct IOBufferStack;')
            assert git('cat-file','blob',projected[p])==exact
        else:
            assert projected[p]==expected[p],(name,p)
    rows.append({'name':name,'parent':parent,'commit':rev,'preimages':prior,'postimages':projected,
                 'patch_sha256':hashlib.sha256(patch.read_bytes()).hexdigest()})
    print(name,len(paths),flush=True)
paths=git('diff','--cached','--name-only',r24,env=env).decode().splitlines()
assert git('rev-parse','HEAD').decode().strip()==ORIGINAL and not git('status','--porcelain=v1').strip()
result={'note':'Dry projection against the exact integrated R24 source commit. No release or native gate.',
        'starting_tree':r24,'projected_tree':git('write-tree',env=env).decode().strip(),
        'groups':rows,'paths':paths,'new_sources':[p for p in paths if oid(r24,p) is None]}
(OUT/'PROJECTION_INTEGRATED.json').write_text(json.dumps(result,indent=2)+'\n')
print('paths',len(paths),'new',result['new_sources'],flush=True)


