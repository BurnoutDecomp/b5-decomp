import hashlib,json,os,shutil,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
wave=out.parent; artifacts=wave.parent
r28=wave/'r28-batch-final'
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
r27='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
integrated='fa84cd2a4423d5258a88eff385c5ad61e1707a88'
def git(*args,env=None):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):return {'path':str(p.resolve()),'sha256':sha(p)}
def read(p):return json.loads(p.read_text())
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
assert sha(r28/'MANIFEST.json')=='f4671fba40fc70fe940cf762c3ddee064d851456f9bb575984a9817095084a9b'
assert sha(r28/'r28.patch')=='ab78f6fc9855919e7d1e173e2cc99cc8f6ed0a98a5acb95a1ff892f728115e63'
assert git('rev-parse','HEAD').decode().strip()==integrated and not git('status','--porcelain=v1').strip()
index=out/'projection.index'
assert not index.exists()
shutil.copyfile(r28/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
assert git('diff','--cached','--binary','--no-ext-diff',r27,env=env)==(r28/'r28.patch').read_bytes()
base_tree=git('write-tree',env=env).decode().strip()
assert base_tree==git('rev-parse',integrated+'^{tree}').decode().strip()
initial=parse(git('ls-files','--stage',env=env),True)
current=initial.copy()
groups=[]
ssl=artifacts/'wave30/protossl-native'
lobby=artifacts/'wave30/lobby-async-luna'
advt=artifacts/'wave22/protoadvt-native-terra'
comm=artifacts/'wave22/commudp-native-terra'
specs=[
 ('movie-prepare','6d9418d40a9f00d92e3cc804b7d939008faa8d77','e7676229c200e6a0779356e692460c3f7a76a4c7',
  None,[wave/'movie-player-prepare-root/DELIVERY.json']),
 ('native-discovery-commudp',r27,'c2fa0edb38b56d46b246428c0670e020ba99b83a',
  None,[advt/'evidence.txt',advt/'header/native1/compile-results.json',comm/'evidence.txt',comm/'native1/compile-results.json']),
 ('netgameutil-completion','ebb096a8b842bd5f49bffa51235d3d5d0f7c4f41','1b65752c5f58f181327a75bc83a600297ff51d0f',
  ['vendor/dirtysdk/include/netgameutil.h','vendor/dirtysdk/src/x360/netgameutil_x360.cpp'],
  [wave/'netgameutil-selectors-root/DELIVERY.json',wave/'netgameutil-connect-root/DELIVERY.json']),
 ('lobby-native-async','f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331','0b92a376628bdab192d0b01a3fd2095cf5174bc4',
  None,[lobby/'receipt.json',lobby/'RECEIPT.md']),
 ('protossl-update','4b75f3030dcdec0e8fa911463969c469e7cb8bd5','ff382871c8d29763c50b50eeec102fc1f707f050',
  None,[ssl/'UPDATE_RECEIPT.txt']),
 ('draw-renderable-native',r27,'0c0a72dff56949c63dac1d0a8e7b2ba24b9a67c0',
  None,[wave/'draw-renderable-interpret-sol/candidate.json']),
 ('protofilter-native','0b92a376628bdab192d0b01a3fd2095cf5174bc4','e10772d277c7bddcecff741354d2fbee0d9dc332',
  None,[artifacts/'wave30/protofilter-native-luna/receipt.json',artifacts/'wave30/protofilter-native-luna/RECEIPT.md']),
 ('protomangle-lifecycle',r27,'7e222eea826d81d82fc57add862a586f17a3d2bb',
  None,[Path('D:/Reverse/BP-X360-Experimental-Worktrees/protomangle-native-terra/receipt.json')]),
 ('loading-textures',r27,'179dd217726975da2e99bc1cc715bcd59d42abe4',
  None,[wave/'r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json']),
]
for name,base,head,paths,receipts in specs:
    if paths is None:paths=git('diff','--name-only',base,head).decode().splitlines()
    pre=parse(git('ls-tree','-r',base,'--',*paths));post=parse(git('ls-tree','-r',head,'--',*paths))
    assert paths and all(current.get(p)==pre.get(p) for p in paths),(name,paths)
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',base,head,'--',*paths))
    git('apply','--cached','--check',str(patch),env=env);git('apply','--cached',str(patch),env=env)
    current=parse(git('ls-files','--stage',env=env),True)
    assert all(current.get(p)==post.get(p) for p in paths)
    groups.append({'name':name,'base':base,'head':head,'paths':paths,'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
paths=git('diff','--cached','--name-only',base_tree,env=env).decode().splitlines()
patch=out/'r29-preflight.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base_tree,env=env))
assert git('rev-parse','HEAD').decode().strip()==integrated and not git('status','--porcelain=v1').strip()
new=[p for p in paths if p not in initial]
result={'schema':'r29-source-preflight-v1','base':integrated,'base_tree':base_tree,'base_manifest':pin(r28/'MANIFEST.json'),
        'base_patch':pin(r28/'r28.patch'),'groups':groups,'patch':pin(patch),
        'final_paths':[{'path':p,'old':initial.get(p),'new':current.get(p)} for p in paths],
        'new_cpp':[p for p in new if p.endswith('.cpp')],'new_headers':[p for p in new if p.endswith('.h')],
        'scope':'Dry composition against actual integrated R28, byte-identical to the final reviewed R28 tree. No combined R29 compile or link.'}
(out/'PROJECTION.json').write_text(json.dumps(result,indent=2)+'\n')
print('R29 PREFLIGHT',len(paths),len(result['new_cpp']),len(result['new_headers']),sha(out/'PROJECTION.json'))
