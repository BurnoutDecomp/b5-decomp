import hashlib,json,os,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
wave=out.parent
artifact=wave.parent
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
r27='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
r26='3792b7d0f752405cbf160c530832492df09da639'
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
assert git('rev-parse','HEAD').decode().strip()==r27 and not git('status','--porcelain=v1').strip()
assert read(wave/'r27-intake-final/completion.json')['head']==r27
assert read(wave/'r27-link/result.json')['all_post_checks_pass']
index=out/'projection.index'
assert not index.exists()
env=dict(os.environ,GIT_INDEX_FILE=str(index))
git('read-tree',r27,env=env)
active=parse(git('ls-files','--stage',env=env),True)
before=active.copy()
groups=[]
ssl=artifact/'wave30/protossl-native'
realm=artifact/'wave19/realmc-finish-luna'
specs=[
    ('movie-render-update',r27,'6d9418d40a9f00d92e3cc804b7d939008faa8d77',
     [wave/'movie-output-texture-root/DELIVERY.json',wave/'movie-output-texture-root/RENDER_DELIVERY.json',
      wave/'movie-player-render-root/DELIVERY.json',wave/'movie-player-update-root/DELIVERY.json']),
    ('protossl-entropy','4770dc5184c3c1b8989cf4957a076782cbe17bf5','4b75f3030dcdec0e8fa911463969c469e7cb8bd5',
     [ssl/'ENTROPY_RECEIPT.txt',ssl/'entropy-final/frame-verification.json',ssl/'entropy-final/verify_frame.py']),
    ('netgameutil-lifetime','87aa75ffbb2540b38631d053427b43917052c27e','ebb096a8b842bd5f49bffa51235d3d5d0f7c4f41',
     [wave/'netgameutil-lifetime-root/DELIVERY.json']),
    ('realmc-native-interface','44422f9bdfa8caf9a008fd08ab94f52e6dba9178','cc34d998240dbcba830675d0085f0d272b2a2beb',
     [realm/'SLICE_RECEIPT.json']),
    ('realmc-simple-tasks','cc34d998240dbcba830675d0085f0d272b2a2beb','ae1f50bdcd6a4a8e37775d61e1d7d7127bd242ae',
     [realm/'COMPOSITION_BASE_RECEIPT.json',realm/'EXPANDED_SIMPLE_RECEIPT.json']),
    ('replay-serializers',r26,'88a750b9695924d1389910c87b92922f05efb7f0',
     [Path('D:/Reverse/BP-X360-Experimental-Worktrees/replay-native-terra-evidence/receipt.json')]),
]
for name,base,head,receipts in specs:
    assert subprocess.run(['git','merge-base','--is-ancestor',base,head],cwd=w,check=False).returncode==0
    paths=git('diff','--name-only',base,head).decode().splitlines()
    b=parse(git('ls-tree','-r',base,'--',*paths)); a=parse(git('ls-tree','-r',head,'--',*paths))
    assert paths and all(active.get(p)==b.get(p) for p in paths),(name,[p for p in paths if active.get(p)!=b.get(p)])
    if name=='realmc-native-interface':
        assert 'src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp' in paths
        assert b'Native Xenon SaveLoad implementation' in git('show',r27+':src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp')
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',base,head))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    active=parse(git('ls-files','--stage',env=env),True)
    assert all(active.get(p)==a.get(p) for p in paths)
    groups.append({'name':name,'base':base,'head':head,'paths':paths,'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
final=git('diff','--cached','--name-only',r27,env=env).decode().splitlines()
patch=out/'r28-preflight.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',r27,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==r27 and not git('status','--porcelain=v1').strip()
new=[p for p in final if p not in before]
result={'schema':'r28-source-preflight-v1','base':r27,'groups':groups,'patch':pin(patch),
        'final_paths':[{'path':p,'old':before.get(p),'new':active.get(p)} for p in final],
        'new_cpp':[p for p in new if p.endswith('.cpp')],'new_headers':[p for p in new if p.endswith('.h')],
        'scope':'Disposable index source preflight. Native Xenon SaveLoad retains its historical PS3 filename. No integration or link.'}
(out/'PROJECTION.json').write_text(json.dumps(result,indent=2)+'\n')
print('R28 PREFLIGHT',len(final),len(result['new_cpp']),len(result['new_headers']),sha(out/'PROJECTION.json'))
