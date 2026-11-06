import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
out=Path(__file__).resolve().parent
wave=out.parent
artifact=wave.parent
old=wave/'r27-batch'
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='3792b7d0f752405cbf160c530832492df09da639'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):
    return {'path':str(p.resolve()),'sha256':sha(p)}
def read(p):
    return json.loads(p.read_text())
def parse(data,staged=False):
    rows={}
    for line in data.decode().splitlines():
        info,path=line.split('\t',1)
        if staged:
            mode,blob,stage=info.split()
            assert stage=='0'
            info=f'{mode} blob {blob}'
        rows[path]=info
    return rows
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
oldmanifest=old/'MANIFEST.json'
oldpatch=old/'r27.patch'
oldreview=old/'REVIEW.json'
assert sha(oldmanifest)=='67c7caab7b22698804663456e4347c3bfa4528775493e4e5b2089bae153a3410'
assert sha(oldpatch)=='9d9c6f3e9398b40d789e06aab87b804b86f4e46080f9d8edc1f3827980b9ed8b'
review=read(oldreview)
assert review['verdict']=='fail' and review['manifest_sha256']==sha(oldmanifest) and review['patch_sha256']==sha(oldpatch)
m=read(oldmanifest)
index=out/'projection.index'
assert not index.exists()
shutil.copyfile(old/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
active=parse(git('ls-files','--stage',env=env),True)
before=parse(git('ls-tree','-r',base))
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==oldpatch.read_bytes()
assert all(active.get(r['path'])==r['new'] and before.get(r['path'])==r['old'] for r in m['final_paths'])
groups=m['groups'][:]
socket=artifact/'wave22/socket-io-terra'
ssl=artifact/'wave30/protossl-native'
deltas=[
    {'name':'protossl-disconnect','base':'3885ddf70a1cc41436d403ecfebcae39d11ab7aa',
     'head':'4770dc5184c3c1b8989cf4957a076782cbe17bf5','paths':['vendor/dirtysdk/src/x360/protossl_x360.cpp'],
     'receipts':[ssl/'DISCONNECT_FIX_RECEIPT.txt']},
    {'name':'native-socket-io','base':base,'head':'324395c597622151467fe69c156224227a5dadb4',
     'paths':['vendor/dirtysdk/src/x360/'+name for name in ('dirtynet_io_x360.cpp','dirtynet_lookup_x360.cpp','dirtynet_x360_internal.h')],
     'receipts':[socket/'evidence.txt',socket/'native1/compile-results.json']},
    {'name':'netgamelink-lifetime','base':base,'head':'87aa75ffbb2540b38631d053427b43917052c27e',
     'paths':['vendor/dirtysdk/include/netgamelink.h','vendor/dirtysdk/src/x360/dirtylib_x360_internal.h','vendor/dirtysdk/src/x360/netgamelink_x360.cpp'],
     'receipts':[wave/'netgamelink-lifetime-root/DELIVERY.json']},
]
if len(sys.argv)==2:
    deltas+=read(Path(sys.argv[1]))
touched=set()
for delta in deltas:
    name,parent,head=delta['name'],delta['base'],delta['head']
    assert git('rev-parse',head+'^').decode().strip()==parent
    paths=git('diff','--name-only',parent,head).decode().splitlines()
    assert set(paths)==set(delta['paths'])
    a=parse(git('ls-tree','-r',parent,'--',*paths))
    b=parse(git('ls-tree','-r',head,'--',*paths))
    assert all(active.get(p)==a.get(p) for p in paths),(name,paths)
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,head))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    active=parse(git('ls-files','--stage',env=env),True)
    assert all(active.get(p)==b.get(p) for p in paths)
    groups.append({'name':name,'base':parent,'head':head,'paths':paths,'patch':pin(patch),
                   'receipts':[pin(Path(p)) for p in delta['receipts']]})
    touched.update(paths)
paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
final=[{'path':p,'old':before.get(p),'new':active.get(p)} for p in paths]
assert all(r in final for r in m['final_paths'] if r['path'] not in touched)
assert set(paths)=={r['path'] for r in m['final_paths']}|touched
new=[p for p in paths if p not in before]
if len(deltas)==3:
    assert len(final)==34 and len(groups)==9
    assert sum(p.endswith('.cpp') for p in new)==7 and sum(p.endswith('.h') for p in new)==4
patch=out/'r27.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
manifest={**m,'schema':'r27-native-intake-amended-v1','groups':groups,'patch':pin(patch),'final_paths':final,
          'original_manifest':pin(oldmanifest),'original_review':pin(oldreview),'new_files':new,
          'expected_total_native':3672+sum(Path(p).suffix.lower() in ('.c','.cpp') for p in new)}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R27 AMENDED',len(final),manifest['expected_total_native'],'MANIFEST',sha(out/'MANIFEST.json'),'PATCH',sha(patch))
