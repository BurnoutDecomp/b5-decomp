from pathlib import Path
import ast,hashlib,json,os,struct,subprocess,sys
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
pdir=d.parent/'r28-root-preflight'
base='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
prior_head='6d9418d40a9f00d92e3cc804b7d939008faa8d77'
branch='codex/experimental-x360-r28-combined-preflight'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
m=json.loads((pdir/'PROJECTION.json').read_text())
p=json.loads((d/'projection.json').read_text())
patch=pdir/'r28-preflight.patch'
assert p['manifest_sha256']==sha(pdir/'PROJECTION.json')=='8771342bc2b9cdc0ae86cf09e87ffbfde08c6a5a0a40359f9230a301170b8f18'
assert sha(patch)==m['patch']['sha256'] and p['base']==m['base']==base
assert p['native_count']==3685 and len(p['selected_sources'])==141
assert git('rev-parse','HEAD')==prior_head and not git('status','--porcelain=v1')
git('switch','-c',branch,base)
git('apply','--index','--check',str(patch))
git('apply','--index',str(patch))
expected={r['path']:r['new'].split()[-1] for r in m['final_paths']}
def check_blob(path,blob):
    assert git('rev-parse',':'+path)==blob
    assert blob in (git('hash-object','--path='+path,'--',path),git('hash-object','--no-filters','--',path))
assert set(git('diff','--cached','--name-only').splitlines())==set(expected)
for path,blob in expected.items():check_blob(path,blob)
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args[args.index('--jobs')+1]='4'
args+=['--timeout','180','--quiet']
for src in p['selected_sources']:args+=['--source',src]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp))
subprocess.run(args,cwd=w,env=env,check=True)
report=d/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==141 and r['failed']==0
assert all(row['machine']==0x1f2 for row in r['results'])

# Replay's traffic layout header affects the named CombinedLayouts probe.
guard=json.loads((d/'guard-baseline.json').read_text())
g=next(x for x in guard['rows'] if x['name']=='CombinedLayouts')
assert sha(Path(g['source']))==g['source_sha256']
gdir=d/'named-guards';gdir.mkdir()
def map_arg(a):
    return a.replace('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake',w.as_posix()).replace(
                     r'C:\Users\adri1\.codex\worktrees\bp360\wave4-intake',str(w))
gargs=[('/Fo'+str(gdir/'CombinedLayouts.obj')) if a.startswith('/Fo') else
       ('/Fa'+str(gdir/'CombinedLayouts.asm')) if a.startswith('/Fa') else map_arg(a) for a in g['command']]
sdk=Path(r['sdk'])
genv=dict(env,XEDK=str(sdk),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(env['SystemRoot'])/'System32'),
          INCLUDE=str(sdk/'include/xbox'),LIB=str(sdk/'lib/xbox'))
for key in ('CL','_CL_','LINK','_LINK_'):genv.pop(key,None)
with (gdir/'CombinedLayouts.log').open('wb') as log:
    subprocess.run(gargs,cwd=w,env=genv,stdout=log,stderr=subprocess.STDOUT,check=True)
fn=d.parent.parent/'wave8/repair-r4/compile_header_probes.py'
nodes=[n for n in ast.parse(fn.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='symbol_words']
ns={'Path':Path,'struct':struct}
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fn),'exec'),ns)
facts=ns['symbol_words'](gdir/'CombinedLayouts.obj','R4LayoutFacts',25)
assert facts==guard['emitted_facts']['CombinedLayouts']
(gdir/'RESULT.json').write_text(json.dumps({'name':'CombinedLayouts','passed':True,'facts':facts,'command':gargs},indent=2)+'\n')

# Replay the exact entropy checks against the combined objects and listing.
v=d.parent.parent/'wave30/protossl-native/entropy-final/verify_frame.py'
content=v.read_text()
old="root = pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/entropy-final')"
assert content.count(old)==1 and content.count("report['passed'] == 3")==1
content=content.replace(old,"root = pathlib.Path("+repr(str(report.parent))+")").replace("report['passed'] == 3","report['passed'] == 141")
exec(compile(content,str(v),'exec'),{})
entropy=report.parent/'frame-verification.json'
assert json.loads(entropy.read_text())['status']=='pass'

assert not git('diff','--name-only')
for path,blob in expected.items():check_blob(path,blob)
msg=d/'commit-message.txt'
msg.write_text('Compose initial R28 Xbox 360 candidates for native verification\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('commit','--file',str(msg))
head=git('rev-parse','HEAD')
assert not git('status','--porcelain=v1') and git('rev-parse','HEAD^')==base
(d/'DELIVERY.json').write_text(json.dumps({'base':base,'head':head,'branch':branch,
    'projection_sha256':sha(d/'projection.json'),'patch_sha256':sha(patch),'compile_sha256':sha(report),
    'compiled':141,'layout_guard_sha256':sha(gdir/'RESULT.json'),'entropy_frame_sha256':sha(entropy),
    'scope':'Initial affected native source gate, layout guard and exact entropy frame. Full R28 integration/link pending.'},indent=2)+'\n')
print('R28_COMBINED_PASS',head,sha(report),flush=True)
