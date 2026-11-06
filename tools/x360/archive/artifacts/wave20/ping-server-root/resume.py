import hashlib,json,os,pathlib,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
START='8a41fda10605a9e780610d6331a6cabd8c6ab000'
BASE='f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331'
C='vendor/dirtysdk/src/x360/pingmanager.cpp'
H='vendor/dirtysdk/include/pingmanager.h'
A='vendor/dirtysdk/src/x360/lobbyapi.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def replace(p,old,new):
    f=W/p
    b=f.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    s=b.decode().replace('\r\n','\n')
    assert s.count(old)==1
    f.write_bytes(s.replace(old,new).replace('\n',eol).encode())

assert git('rev-parse','HEAD')==BASE
assert git('branch','--show-current')=='codex/experimental-x360-r27-root-network'
assert set(git('diff','--name-only').splitlines())=={C,H}
assert not git('diff','--cached','--name-only')
git('-c','core.whitespace=cr-at-eol','diff','--check','--',C,H)
args=json.loads((D.parent/'r26-root-providers/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for p in (C,A):args+=['--source',p]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==2 and r['failed']==0
rows={row['source']:row for row in r['results']}
assert all(row['machine']==498 for row in rows.values())
asm=pathlib.Path(rows[C]['assembly']).read_text(errors='replace')
body=asm.split('Begin code for function: PingManagerPingServer\n',1)[1].split('End code for function:',1)[0]
assert '38800000' in body and 'PingManagerPingServer2' in body
caller=pathlib.Path(rows[A]['assembly']).read_text(errors='replace')
assert any(' bl ' in l and 'PingManagerPingServer' in l for l in caller.splitlines())
msg=D/'message.txt'
msg.write_text('Restore the native default-server ping entry\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',C,H)
assert set(git('diff','--cached','--name-only').splitlines())=={C,H}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':BASE,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in (C,H)},
        'artist_provider':'0x82B27BF0 clears r4 and tail-forwards r3/r5/r6',
        'artist_lobby_caller':'0x82B1C558',
        'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])
