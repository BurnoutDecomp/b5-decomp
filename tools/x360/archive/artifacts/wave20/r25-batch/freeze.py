import argparse,hashlib,json,os,pathlib,shutil,subprocess

OUT=pathlib.Path(__file__).resolve().parent
ART=OUT.parent.parent
CHECK=OUT.parent/'r25-preflight-combined'
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='c7376f3fed4105792eabdb9622d3ffefd7535009'
RECEIPTS={
    'xam-schema':['wave20/xam-schema-root/DELIVERY.json'],
    'gui-queue':['wave20/gui-queue-length-root/DELIVERY.json'],
    'state-factories':['wave20/state-factory-vtables-root/DELIVERY.json'],
    'ice-action-tables':['wave20/ice-action-data-root/DELIVERY.json'],
    'device-access':['wave19/renderer-render-sol/device-present-interval-access.patch',
                     'wave19/renderer-render-sol/render-frame-candidate.json'],
    'mesh-context':['wave19/dispatch-context-terra/objects/compile-results.json',
                    'wave19/renderer-render-sol/dispatcher-provider-combined.cpp'],
    'full-render':['wave19/renderer-render-sol/render-frame-candidate.json',
                   'wave19/renderer-render-sol/render-reference-inventory-with-providers.json'],
    'particle-jobs':['wave18/renderer-entry-terra/particle-final/compile-results.json'],
    'im3d':['wave19/im3d-dispatch-terra/native1/compile-results.json'],
    'effects-vtable':['wave19/effects-vtable-terra/evidence.json',
                      'wave19/effects-vtable-terra/renderer-compile.log'],
    'xnfriends':['wave25/xnfriends-terra/RECEIPT.md'],
    'lobby-gamemanager':['wave19/lobby-finish-luna/POSTCOMMIT_CHECK.json',
                         'wave19/lobby-finish-luna/gate-ppc-native-1/compile-results.json']
}
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W,env=env)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def pinned(path):
    return {'path':str(path.resolve()),'sha256':sha(path)}
def tree_tuple(rev,path):
    return git('ls-tree',rev,'--',path).decode().split('\t')[0].strip() or None

parser=argparse.ArgumentParser()
parser.add_argument('--object-to-mesh')
parser.add_argument('--job-receipt')
args=parser.parse_args()
assert bool(args.object_to_mesh)==bool(args.job_receipt)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
preview=CHECK/'PROJECTION_INTEGRATED.json'
m=json.loads(preview.read_text())
assert m['starting_tree']==git('rev-parse',BASE+'^{tree}').decode().strip()
env=dict(os.environ,GIT_INDEX_FILE=str(CHECK/'preview-integrated.index'))
assert git('write-tree',env=env).decode().strip()==m['projected_tree']
paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
assert paths==m['paths'] and len(paths)==33
groups=[]
for group in m['groups']:
    name=group['name']
    patch=pinned(CHECK/(name+'.patch'))
    assert patch['sha256']==group['patch_sha256']
    groups.append({'name':name,'base':git('rev-parse',group['parent']).decode().strip(),
                   'commit':git('rev-parse',group['commit']).decode().strip(),
                   'patch':patch,'preimages':group['preimages'],'postimages':group['postimages'],
                   'receipts':[pinned(ART/x) for x in RECEIPTS[name]]})
assert len(groups)==len(RECEIPTS)==12
projected=m['projected_tree']
if args.object_to_mesh:
    job='src/GameShared/Jobs/ObjectToMesh/ObjectToMesh.cpp'
    end=git('rev-parse',args.object_to_mesh).decode().strip()
    parent=git('rev-parse',end+'^').decode().strip()
    assert parent==BASE
    assert git('diff','--name-only',parent,end).decode().splitlines()==[job]
    assert job not in paths and tree_tuple(projected,job)==tree_tuple(parent,job)
    index=OUT/'projection.index'
    assert not index.exists()
    shutil.copyfile(CHECK/'preview-integrated.index',index)
    env=dict(os.environ,GIT_INDEX_FILE=str(index))
    jobpatch=OUT/'object-to-mesh.patch'
    jobpatch.write_bytes(git('diff','--binary','--no-ext-diff',parent,end))
    git('apply','--cached','--check',str(jobpatch),env=env)
    git('apply','--cached',str(jobpatch),env=env)
    projected=git('write-tree',env=env).decode().strip()
    assert tree_tuple(projected,job)==tree_tuple(end,job)
    groups.append({'name':'object-to-mesh','base':parent,'commit':end,'patch':pinned(jobpatch),
                   'preimages':{job:tree_tuple(parent,job).split()[-1]},
                   'postimages':{job:tree_tuple(end,job).split()[-1]},
                   'receipts':[pinned(pathlib.Path(args.job_receipt))]})
    paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
    assert set(paths)==set(m['paths'])|{job} and len(paths)==34
final=[{'path':p,'old':tree_tuple(BASE,p),'new':tree_tuple(projected,p)} for p in paths]
new_files=[r['path'] for r in final if r['old'] is None]
assert sum(p.endswith('.cpp') for p in new_files)==4
assert [p for p in new_files if p.endswith('.h')]==['vendor/dirtysdk/src/x360/lobbyapi_x360_internal.h']
tracked=git('ls-tree','-r','--name-only',BASE).decode().splitlines()
assert all(pathlib.PurePosixPath(p).name.lower()!='lobbyapi_x360_internal.h' for p in tracked)
patch=OUT/'r25.patch'
assert not patch.exists()
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',BASE,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
manifest={'schema':'r25-native-intake-v1','base':BASE,'source_root':str(W),
          'projection_proof':pinned(preview),'groups':groups,'final_paths':final,'patch':pinned(patch),
          'new_files':new_files,'base_compiled':3647,'expected_total_native':3651}
out=OUT/'MANIFEST.json'
out.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('MANIFEST',sha(out),'PATCH',sha(patch))

