from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path('D:/codex-native/game-module-sol');CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp');OUT=Path(__file__).resolve().parent
stage=OUT/sys.argv[1];stage.mkdir(exist_ok=False);paths=sys.argv[2:]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pins={str(p.relative_to(ROOT)):sha(p)for p in (ROOT/'src').rglob('*.h')}
for p in (ROOT/'src').rglob('*.hpp'):pins[str(p.relative_to(ROOT))]=sha(p)
sourcepins={p:sha(ROOT/p)for p in paths}
cmd=[sys.executable,'-X','utf8','-B',str(ROOT/'tools/build_x360.py'),'--sdk',str(CANON/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(stage),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes','--compiler-arg=/Z7']
for p in paths:cmd+=['--source',p]
for p in ['EABase/include/Common','EASTL/include','EAThread/include']:cmd+=['--compiler-arg=/I'+str(CANON/'vendor'/p)]
env=dict(os.environ);temp=stage/'temp';temp.mkdir();env['TEMP']=env['TMP']=str(temp)
r=subprocess.run(cmd,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding='utf-8',errors='replace');(stage/'driver.log').write_text(r.stdout+r.stderr)
report=json.loads((stage/'compile-results.json').read_text());includes={}
for row in report['results']:
 for line in Path(row['log']).read_text().splitlines():
  if 'including file:'in line:
   p=Path(line.split('including file:',1)[1].strip());includes[str(p.resolve())]=sha(p)
 for p,d in includes.items():
  pp=Path(p)
  if pp.is_relative_to(ROOT/'src'):assert pins[str(pp.relative_to(ROOT))]==d
 print('TERMINAL',row['passed'],row['source']);print('\n'.join(l for l in Path(row['log']).read_text().splitlines()if 'error 'in l))
assert sourcepins=={p:sha(ROOT/p)for p in paths}
(stage/'source-object-binding.json').write_text(json.dumps({'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),'source_before_after':sourcepins,'actual_includes_before_after':includes,'command':cmd,'driver_sha256':sha(__file__),'report_sha256':sha(stage/'compile-results.json'),'objects':{row['source']:sha(row['object'])for row in report['results']if row['passed']}},indent=2)+'\n')
