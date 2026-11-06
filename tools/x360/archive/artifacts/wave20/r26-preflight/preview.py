import hashlib,json,os,pathlib,shutil,subprocess
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='c7376f3fed4105792eabdb9622d3ffefd7535009'
R25=D.parent/'r25-batch'
GROUPS=[
 ('codec-corona','c7376f3f','50600bd0'),
 ('picture','c7376f3f','0ac326e4'),
 ('game-data','d49dc118','2691fcf7'),
 ('netconn','a551dbc8','8d9cc135'),
 ('dirty-sdk-diagnostics','8d9cc135','34265fbc'),
 ('socket-addresses','8d9cc135','bde3d525')]
def git(*a,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*a],cwd=W,env=env)
def item(rev,p):
    return git('ls-tree',rev,'--',p).decode().split('\t')[0].strip() or None
def index_item(env,p):
    row=git('ls-files','--stage','--',p,env=env).decode().split()
    return f'{row[0]} blob {row[1]}' if row else None
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
old=json.loads((R25/'MANIFEST.json').read_text())
index=D/'preview.index'
assert not index.exists()
shutil.copyfile(R25/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
assert all(index_item(env,x['path'])==x['new'] for x in old['final_paths'])
pre=git('write-tree',env=env).decode().strip()
out=[]
for name,start,end in GROUPS:
    start,end=(git('rev-parse',ref).decode().strip() for ref in (start,end))
    files=git('diff','--name-only',start,end).decode().splitlines()
    rows=[{'path':p,'old':item(start,p),'new':item(end,p)} for p in files]
    assert all(index_item(env,r['path'])==r['old'] for r in rows),(name,rows)
    patch=D/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',start,end))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    assert all(index_item(env,r['path'])==r['new'] for r in rows),name
    out.append({'name':name,'base':start,'commit':end,'paths':rows,'patch_sha256':hashlib.sha256(patch.read_bytes()).hexdigest()})
    print(name,len(files),flush=True)
files=git('diff','--cached','--name-only',pre,env=env).decode().splitlines()
assert all(p not in files for p in ('vendor/dirtysdk/src/x360/gamemanager.cpp','vendor/dirtysdk/src/x360/hlbudapi.cpp','vendor/dirtysdk/src/x360/xnfriends.cpp'))
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
result={'scope':'Dry preimage check against original R25 projection. Requires final reviewed R25 baseline and new review.',
        'original_r25_projected_tree':pre,'projected_tree':git('write-tree',env=env).decode().strip(),
        'groups':out,'changed_paths':files}
(D/'PROJECTION.json').write_text(json.dumps(result,indent=2)+'\n')
print('paths',len(files),'new cpp',sum(p.endswith('.cpp') and item(pre,p) is None for p in files))
