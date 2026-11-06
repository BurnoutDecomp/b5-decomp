from pathlib import Path
import ast,hashlib,json,os,re,struct,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
pdir=d.parent/'r28-root-preflight'
base='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
branch='codex/experimental-x360-r28-combined-preflight'
fix='src/GameShared/GameClasses/Gui/CgsSaveLoadX360_wB_04.cpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
m=json.loads((pdir/'PROJECTION.json').read_text())
p=json.loads((d/'projection.json').read_text())
patch=pdir/'r28-preflight.patch'
assert sha(patch)==m['patch']['sha256'] and p['base']==m['base']==base
assert git('rev-parse','HEAD')==base and git('branch','--show-current')==branch
assert not git('diff','--name-only') and not any(fix in v for v in p['projected_snapshot']['dependencies'].values())
expected={r['path']:r['new'].split()[-1] for r in m['final_paths']}
def check_blob(path,blob):
    assert git('rev-parse',':'+path)==blob
    assert blob in (git('hash-object','--path='+path,'--',path),git('hash-object','--no-filters','--',path))
assert set(git('diff','--cached','--name-only').splitlines())==set(expected)|{fix}
for path,blob in expected.items():check_blob(path,blob)
first=d/'native/compile-results.json'; retry=d/'realmc-caller-repair/compile-results.json'
r=json.loads(first.read_text()); q=json.loads(retry.read_text())
assert r['requested']==141 and r['passed']==140 and r['failed']==1
assert [x['source'] for x in r['results'] if not x['passed']]==[fix]
assert q['requested']==q['passed']==1 and q['failed']==0
new=q['results'][0]
assert new['source']==fix and new['machine']==0x1f2
asm=Path(new['assembly']).read_text(errors='replace')
owner='?EnableAutosave@SaveLoadSystem@CgsGui@@QAAXH@Z'
body=asm.split('Begin code for function: '+owner,1)[1].split('End code for function:',1)[0]
assert '30h(' in body and '38 80 00 01'.replace(' ','') in body.lower() and 'bctrl' in body
for row in r['results']:
    if row['source']==fix:continue
    assert row['passed'] and row['machine']==0x1f2
    traces=[line.lower() for line in Path(row['log']).read_text(errors='replace').splitlines() if 'including file:' in line.lower()]
    assert not any('cgssaveloadx360_wb_04.cpp' in line for line in traces)
r['results']=[new if row['source']==fix else row for row in r['results']]
r['passed']=141;r['failed']=0
r['origin_reports']=[{'path':str(first),'sha256':sha(first)},{'path':str(retry),'sha256':sha(retry)}]
verified=d/'verified';assert verified.is_dir()
report=verified/'compile-results.json'
assert json.loads(report.read_text())==r
assert (d/'realmc-caller-repair.patch').read_bytes()==subprocess.check_output(['git','diff','--cached','--binary','--',fix],cwd=w)
expected[fix]=git('rev-parse',':'+fix)
assert set(git('diff','--cached','--name-only').splitlines())==set(expected)
final_patch=d/'r28-combined-repaired.patch'
assert final_patch.read_bytes()==subprocess.check_output(['git','diff','--cached','--binary',base],cwd=w)
env=dict(os.environ,TEMP=str(d/'repair-temp'),TMP=str(d/'repair-temp'))
src=(d/'compile.py').read_text()
tail=src[src.index("# Replay's traffic layout header"):]
tail=tail.replace("'patch_sha256':sha(patch)","'patch_sha256':sha(final_patch),'initial_patch_sha256':sha(patch),'caller_repair_patch_sha256':sha(d/'realmc-caller-repair.patch')")
tail=tail.replace("gdir.mkdir()","gdir.mkdir(exist_ok=True)").replace("env['SystemRoot']","os.environ['SystemRoot']")
exec(compile(tail,str(d/'finish_repair.py'),'exec'))
