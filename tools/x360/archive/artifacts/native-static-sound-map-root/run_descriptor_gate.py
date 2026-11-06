from pathlib import Path
import hashlib, json, os, subprocess, sys
ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
CANON=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
HERE=Path(__file__).resolve().parent
sources=['vendor/renderware/src/rw/BaseResourceDescriptor.cpp']
stage=HERE/sys.argv[1]
assert not stage.exists(),stage
stage.mkdir()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
inputs={s:sha(ROOT/s) for s in sources}
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
command=[sys.executable,'-X','utf8','-B',str(ROOT/'tools/build_x360.py'),'--sdk',str(CANON/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(stage),'--jobs','3','--compiler-arg=/showIncludes']
for s in sources:command.extend(['--source',s])
for s in ('EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src'):
 command.append('--compiler-arg=/I'+str(CANON/'vendor'/s))
env=dict(os.environ); tmp=HERE/'tmp';tmp.mkdir(exist_ok=True);env['TEMP']=env['TMP']=str(tmp)
r=subprocess.run(command,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding='utf-8',errors='replace')
(stage/'driver.log').write_text(r.stdout+r.stderr,encoding='utf-8')
assert inputs=={s:sha(ROOT/s) for s in sources}
report=json.loads((stage/'compile-results.json').read_text(encoding='utf-8'))
bind={'head':head,'sources':inputs,'report_sha256':sha(stage/'compile-results.json'),'driver_sha256':sha(__file__),'actual_headers':{}}
for row in report['results']:
 log=Path(row['log']).read_text(encoding='utf-8',errors='replace')
 errs=[s for s in log.splitlines() if 'error ' in s]
 print(row['passed'],row['source'],'diagnostics',len(errs),*errs[:3],sep='\n')
 for line in log.splitlines():
  if line.startswith('Note: including file:'):
   q=Path(line.split('Note: including file:',1)[1].strip()); assert q.is_file(),q;bind['actual_headers'][str(q.resolve())]=sha(q)
(stage/'source-object-binding.json').write_text(json.dumps(bind,indent=2)+'\n',encoding='utf-8')
print('headers',len(bind['actual_headers']),'passed',report['passed'],'failed',report['failed'])
sys.exit(r.returncode)
