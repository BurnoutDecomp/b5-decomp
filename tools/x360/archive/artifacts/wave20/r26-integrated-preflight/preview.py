import hashlib,json,os,pathlib,subprocess
D=pathlib.Path(__file__).resolve().parent
A=D.parent.parent
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331'
GROUPS=[
 ('codec-corona','c7376f3f','50600bd0',['wave20/r26-root-providers/DELIVERY.json','wave20/r26-root-providers/coff-link-input-comparison.json']),
 ('ai-speed','50600bd0','19bf3f95',['wave20/ai-speed-debug-root/DELIVERY.json']),
 ('material-descriptor','19bf3f95','5c09399b',['wave20/material-descriptor-root/DELIVERY.json']),
 ('movie-dissolve','5c09399b','a888255d',['wave20/movie-dissolve-root/DELIVERY.json']),
 ('movie-player-state','a888255d','8a41fda1',['wave20/movie-player-state-root/DELIVERY.json']),
 ('picture','c7376f3f','0ac326e4',['wave20/picture-lifetime-terra/native2/compile-results.json']),
 ('game-data','d49dc118','65c6d4de',['D:/Reverse/BP-X360-Experimental-Worktrees/gamedata-native-terra/receipt.json',
                                    'D:/Reverse/BP-X360-Experimental-Worktrees/gamedata-handler-overrides/receipt.json']),
 ('netconn-and-diagnostics','a551dbc8','2f7833ae',['wave27/netconn-connection-terra/RECEIPT.md',
                                                'wave28/dirty-diagnostics-terra/RECEIPT.md',
                                                'wave28/dirty-printmem-terra/RECEIPT.md',
                                                'wave30/netconn-thread-terra/RECEIPT.md']),
 ('dirtylsp-hostent','c7376f3f','fdcbce66',['wave18/dirtylsp-native/gate1/compile-results.json',
                                         'wave18/dirtylsp-native/gate3/compile-results.json']),
 ('socket','8d9cc135','0788e4c4',['wave20/dirtynet-socket-terra/lifecycle4/compile-results.json',
                                 'wave20/dirtynet-socket-terra/control1/compile-results.json'])]
def git(*a,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*a],cwd=W,env=env)
def parsed(data,index=False):
    result={}
    for line in data.decode().splitlines():
        info,path=line.split('\t',1)
        if index:
            mode,oid,stage=info.split()
            assert stage=='0'
            info=f'{mode} blob {oid}'
        result[path]=info
    return result
def pin(x):
    p=A/x
    return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
assert not (D/'projection.index').exists()
env=dict(os.environ,GIT_INDEX_FILE=str(D/'projection.index'))
git('read-tree',BASE,env=env)
groups=[]
for name,start,end,receipts in GROUPS:
    start,end=(git('rev-parse',x).decode().strip() for x in (start,end))
    paths=git('diff','--name-only',start,end).decode().splitlines()
    before=parsed(git('ls-tree','-r',start))
    after=parsed(git('ls-tree','-r',end))
    stage=parsed(git('ls-files','--stage',env=env),True)
    rows=[{'path':p,'old':before.get(p),'new':after.get(p)} for p in paths]
    assert all(stage.get(r['path'])==r['old'] for r in rows),(name,[r for r in rows if stage.get(r['path'])!=r['old']])
    patch=D/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',start,end))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    stage=parsed(git('ls-files','--stage',env=env),True)
    assert all(stage.get(r['path'])==r['new'] for r in rows),(name,rows)
    groups.append({'name':name,'base':start,'commit':end,'paths':rows,
                   'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
    print(name,len(rows),flush=True)
paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
before=parsed(git('ls-tree','-r',BASE))
after=parsed(git('ls-files','--stage',env=env),True)
rows=[{'path':p,'old':before.get(p),'new':after.get(p)} for p in paths]
new=[r['path'] for r in rows if r['old'] is None]
assert all(r['new'] for r in rows)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
(D/'PROJECTION.json').write_text(json.dumps({'scope':'Candidate preimage composition only. No R26 review, release, compile or link.',
                                             'base':BASE,'groups':groups,'final_paths':rows,'new_files':new},indent=2)+'\n')
print('paths',len(rows),'new cpp',sum(p.endswith('.cpp') for p in new),'new headers',sum(p.endswith('.h') for p in new))
