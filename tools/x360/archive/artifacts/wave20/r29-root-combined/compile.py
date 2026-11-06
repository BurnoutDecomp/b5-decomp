from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys
d=Path(__file__).resolve().parent
wave=d.parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
pre=wave/'r29-root-preflight'
base='fa84cd2a4423d5258a88eff385c5ad61e1707a88'
prior='c69edc410f6e2e1dc6ba6efd707f9136019d8aec'
branch='codex/experimental-x360-r29-combined-preflight'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
m=json.loads((pre/'PROJECTION.json').read_text())
p=json.loads((d/'projection.json').read_text())
patch=pre/'r29-preflight.patch'
assert p['manifest_sha256']==sha(pre/'PROJECTION.json')=='e5262ccb19d47824da0eaf72f012c2af317182ae545cd880463f14e556d06f14'
assert sha(patch)==m['patch']['sha256'] and p['base']==m['base']==base
assert p['native_count']==3691 and len(p['selected_sources'])==60
assert git('rev-parse','HEAD')==prior and not git('status','--porcelain=v1')
git('switch','-c',branch,base)
git('apply','--index','--check',str(patch));git('apply','--index',str(patch))
expected={r['path']:r['new'].split()[-1] for r in m['final_paths']}
assert set(git('diff','--cached','--name-only').splitlines())==set(expected)
for path,blob in expected.items():
    assert git('rev-parse',':'+path)==blob
    assert blob in (git('hash-object','--path='+path,'--',path),git('hash-object','--no-filters','--',path))
msg=d/'commit-message.txt'
msg.write_text('Compose initial R29 Xbox 360 candidates for native verification\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('commit','--file',str(msg))
head=git('rev-parse','HEAD')
assert git('rev-parse','HEAD^')==base and not git('status','--porcelain=v1')
loading=d/'LOADING_INPUT.json'
stage=wave/'r29-provisional-preflight/stage_loading_input.py'
subprocess.run([sys.executable,'-X','utf8','-B',str(stage),'--worktree',str(w),'--head',head,
                '--receipt',str(loading)],check=True)
args=json.loads((wave/'r26-stream-lock-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args[args.index('--jobs')+1]='4'
args+=['--timeout','180','--quiet']
for src in p['selected_sources']:args+=['--source',src]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
report=d/'native/compile-results.json'; r=json.loads(report.read_text())
assert r['passed']==r['requested']==60 and r['failed']==0
assert all(row['machine']==0x1f2 for row in r['results'])
row=next(row for row in r['results'] if row['source']=='src/GameSource/Game/BrnLoadingScreenTextures.cpp')
generated=w/'build/x360/generated'
obj=Path(row['object'])
assert any(str(generated).lower() in line.lower() and '.generated.inc' in line for line in Path(row['log']).read_text(errors='replace').splitlines())
original=wave/'r27-loading-terra-relative/verify-coff.py'
v=original.read_text()
out=StringIO()
with redirect_stdout(out):
    exec(compile(v[v.index('b=obj.read_bytes();'):],str(original),'exec'),
         {'generated':generated,'obj':obj,'struct':struct,'hashlib':hashlib,'json':json})
facts=json.loads(out.getvalue())
reference=json.loads((wave/'r27-loading-terra-relative/coff-evidence.json').read_text())
assert facts['arrays']==reference['arrays'] and facts['include_sha256']==reference['include_sha256']
coff=d/'LOADING_COFF.json';coff.write_text(json.dumps(facts,indent=2)+'\n')
assert not git('status','--porcelain=v1')
(d/'DELIVERY.json').write_text(json.dumps({'base':base,'head':head,'branch':branch,
    'projection_sha256':sha(d/'projection.json'),'patch_sha256':sha(patch),'compile_sha256':sha(report),
    'compiled':60,'generated_input_sha256':sha(loading),'loading_coff_sha256':sha(coff),
    'scope':'Initial affected-source compile and actual generated loading COFF. Integrated R29 link/launch pending.'},indent=2)+'\n')
print('R29_COMBINED_PASS',head,sha(report),flush=True)
