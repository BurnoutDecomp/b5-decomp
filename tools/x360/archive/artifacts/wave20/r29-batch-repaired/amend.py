import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
out=Path(__file__).resolve().parent
old=out.parent/'r29-batch'
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='fa84cd2a4423d5258a88eff385c5ad61e1707a88'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def pin(p):return {'path':str(p.resolve()),'sha256':sha(p)}
def entries(data,staged=False):
    result={}
    for line in data.decode().splitlines():
        info,path=line.split('\t',1)
        if staged:
            mode,oid,stage=info.split()
            assert stage=='0'
            info=f'{mode} blob {oid}'
        result[path]=info
    return result
assert len(sys.argv)==2
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
m=read(old/'MANIFEST.json');r=read(old/'REVIEW.json')
assert r['verdict']=='fail' and r['manifest_sha256']==sha(old/'MANIFEST.json')=='365267ec4acaeb8b8441e10fc32f1565363c09faf7bf509cdb552014880f7992'
assert r['patch_sha256']==sha(old/'r29.patch')=='3085a5875905f2b3bdc6145a7153155cc0e826afb0cf8c838a180414cc9bf14d'
repairs=read(Path(sys.argv[1]))
assert repairs
index=out/'projection.index'
assert not index.exists()
shutil.copyfile(old/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==(old/'r29.patch').read_bytes()
current=entries(git('ls-files','--stage',env=env),True)
preimage=entries(git('ls-tree','-r',base))
assert all(current.get(x['path'])==x['new'] and preimage.get(x['path'])==x['old'] for x in m['final_paths'])
touched=set()
groups=m['groups'][:]
for item in repairs:
    parent,head,name=item['parent'],item['head'],item['name']
    assert git('rev-parse',head+'^').decode().strip()==parent
    paths=git('diff','--name-only',parent,head).decode().splitlines()
    assert set(paths)==set(item['paths'])
    start=entries(git('ls-tree','-r',parent,'--',*paths))
    end=entries(git('ls-tree','-r',head,'--',*paths))
    assert all(current.get(p)==start.get(p) for p in paths),(name,paths)
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,head))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    current=entries(git('ls-files','--stage',env=env),True)
    assert all(current.get(p)==end.get(p) for p in paths)
    touched.update(paths)
    groups.append({'name':name,'base':parent,'head':head,'paths':paths,'patch':pin(patch),
                   'receipts':[pin(Path(p)) for p in item['receipts']]})
paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
final=[{'path':p,'old':preimage.get(p),'new':current.get(p)} for p in paths]
assert all(row in final for row in m['final_paths'] if row['path'] not in touched)
assert set(paths)=={x['path'] for x in m['final_paths']}|touched
new_files=[x['path'] for x in final if x['old'] is None]
patch=out/'r29.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
manifest={**m,'schema':'r29-native-intake-amended-v1','groups':groups,'patch':pin(patch),'final_paths':final,
          'original_manifest':pin(old/'MANIFEST.json'),'original_review':pin(old/'REVIEW.json'),
          'new_files':new_files,'expected_total_native':m['base_compiled']+sum(Path(p).suffix.lower() in ('.c','.cpp') for p in new_files)}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R29 AMENDED',len(final),manifest['expected_total_native'],'MANIFEST',sha(out/'MANIFEST.json'),'PATCH',sha(patch))
