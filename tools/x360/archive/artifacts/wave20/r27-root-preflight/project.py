import hashlib,json,os,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
r26='3792b7d0f752405cbf160c530832492df09da639'
r25='f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331'
crypto='9506386e60273f39758ca96c34d78d1894f784c3'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):
    return {'path':str(p.resolve()),'sha256':sha(p)}
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
assert git('rev-parse','HEAD').decode().strip()==r26 and not git('status','--porcelain=v1').strip()
index=out/'projection.index'
assert not index.exists()
env=dict(os.environ,GIT_INDEX_FILE=str(index))
git('read-tree',r26,env=env)
active=parse(git('ls-files','--stage',env=env),True)
before=active.copy()
groups=[]
root=out.parent.parent
for name,base,head,receipts in (
    ('movie-and-dlc-release',r26,'f7591afcc13f4a79c084ca3a041118d58ce8f00b',
     [out.parent/'movie-release-root/DELIVERY.json',out.parent/'dlc-release-root/DELIVERY.json']),
    ('ping-audio',r25,'e63414959746c2a32b2a42594b6d21693795d0ad',
     [out.parent/'ping-server-root/DELIVERY.json',out.parent/'mpeg-tables-root/DELIVERY.json',out.parent/'aems-pause-root/DELIVERY.json']),
    ('protossl-record-crypto-protoaries',r25,'342e8cdcf954be5cbc07d2b7c6d52fe509a23b78',
     [out.parent/'native-crypto-root/DELIVERY.json',root/'wave21/stp1-native-terra/native1/compile-results.json']),
    ('protossl-connect',crypto,'3885ddf70a1cc41436d403ecfebcae39d11ab7aa',
     [root/'wave30/protossl-native/CONNECT_RECEIPT.txt']),
):
    assert subprocess.run(['git','merge-base','--is-ancestor',base,head],cwd=w,check=False).returncode==0
    paths=git('diff','--name-only',base,head).decode().splitlines()
    b=parse(git('ls-tree','-r',base,'--',*paths))
    a=parse(git('ls-tree','-r',head,'--',*paths))
    assert paths and all(active.get(p)==b.get(p) for p in paths),(name,[p for p in paths if active.get(p)!=b.get(p)])
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',base,head))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    active=parse(git('ls-files','--stage',env=env),True)
    assert all(active.get(p)==a.get(p) for p in paths)
    groups.append({'name':name,'base':base,'head':head,'paths':paths,'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
final=git('diff','--cached','--name-only',r26,env=env).decode().splitlines()
patch=out/'r27-preflight.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',r26,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==r26 and not git('status','--porcelain=v1').strip()
new=[p for p in final if p not in before]
result={'schema':'r27-source-preflight-v1','base':r26,'groups':groups,'patch':pin(patch),
        'final_paths':[{'path':p,'old':before.get(p),'new':active.get(p)} for p in final],
        'new_cpp':[p for p in new if p.endswith('.cpp')],'new_headers':[p for p in new if p.endswith('.h')],
        'scope':'Disposable index preflight only. Additional candidates and real transport dependencies remain in progress.'}
(out/'PROJECTION.json').write_text(json.dumps(result,indent=2)+'\n')
print('R27 PREFLIGHT',len(final),len(result['new_cpp']),len(result['new_headers']),sha(out/'PROJECTION.json'))
