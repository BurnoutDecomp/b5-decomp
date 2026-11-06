"""One-job actual-header measurement against immutable 526adc; no source mutation."""
from pathlib import Path
from datetime import datetime,timezone
import ctypes,hashlib,json,os,re,struct,subprocess,time
OUT=Path(__file__).resolve().parent
ROOT=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
CANON=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
NOTES=CANON.parent/'scratch/parallel-native/native_boot_entry'
FROZEN='526adc27662c0c2aedf417980dcf824c989bb035'
BASE=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6')
SDK=CANON/'build/x360/sdk-6534.4/portable/XDK'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(name,value): (OUT/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def git(*args): return subprocess.check_output(['git','--no-optional-locks',*args],cwd=ROOT).decode().strip()
assert git('rev-parse','HEAD')==FROZEN
assert not git('status','--porcelain=v1')
meta_path=BASE/'native-parallel-wave6-full-stage1-patch.json'
meta=json.loads(meta_path.read_text())
assert meta['head']==FROZEN
frozen=meta['source_snapshot']['hashes']
def snapshot(): return {p:sha(ROOT/p) for p in frozen}
started=datetime.now(timezone.utc).isoformat()
before=snapshot()
assert before==frozen, [(p,before[p],v) for p,v in frozen.items() if before[p]!=v]
save('frozen-source-before.json',{'head':FROZEN,'time_utc':started,'hashes':before,'metadata_sha256':sha(meta_path),'patch_sha256':meta['sha256'],'tracked_status':git('status','--porcelain=v1')})
source=Path('C:/Users/adri1/.codex/worktrees/bp360/boot-e102-diag/scratch/native-game-layout-probe.cpp')
probe=OUT/'native-game-layout-probe.cpp'
probe.write_bytes(source.read_bytes())
assert not re.search(r'#\s*(?:define|undef)\s+_XBOX',probe.read_text())
report=json.loads((BASE/'full-stage1/compile-results.json').read_text())
basecmd=report['results'][0]['command']
cmd=[x for x in basecmd[:-1] if not x.startswith(('/Fo','/Fa'))]
assert cmd[0].lower().endswith('cl.exe')
obj,asm,log=OUT/'native-game-layout-probe.obj',OUT/'native-game-layout-probe.asm',OUT/'native-game-layout-probe.log'
cmd+=['/showIncludes','/Fo'+str(obj),'/Fa'+str(asm),str(probe)]
assert all('/src/pc' not in x.replace('\\','/').lower() for x in cmd)
assert all('boot-e102-diag' not in x for x in cmd)
tmp=OUT/'compiler-temp';tmp.mkdir()
env=dict(os.environ)
env.update(XEDK=str(SDK),PATH=str(SDK/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(SDK/'include/xbox'),LIB=str(SDK/'lib/xbox'),TEMP=str(tmp),TMP=str(tmp))
for k in ('CL','_CL_','LINK','_LINK_'): env.pop(k,None)
ctypes.windll.kernel32.SetErrorMode(0x0001|0x0002|0x8000)
command_record={'head':FROZEN,'argv':cmd,'cwd':str(ROOT),'jobs':1,'timeout_seconds':180,'private_temp':str(tmp),'source_probe_sha256':sha(probe),'inherited_probe':str(source),'inherited_probe_sha256':sha(source),'driver_sha256':sha(ROOT/'tools/build_x360.py'),'runner_sha256':sha(__file__),'source_manifest_sha256':sha(OUT/'frozen-source-before.json'),'environment':{k:env[k] for k in ('XEDK','PATH','INCLUDE','LIB','TEMP','TMP')},'compiler_input_origin':'Exact frozen full-stage1 first-row options; only output paths/source changed and /showIncludes added.'}
save('command.json',command_record)
print('FROZEN',FROZEN,'source files',len(before),'one compiler',flush=True)
t0=time.monotonic()
r=subprocess.run(cmd,cwd=ROOT,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
log.write_text(r.stdout+r.stderr,encoding='utf-8')
includes=[]
for line in (r.stdout+r.stderr).splitlines():
 m=re.match(r'\s*Note: including file:\s*(.+?)\s*$',line)
 if m: includes.append(Path(m.group(1)))
unique=sorted({str(p.resolve()):p.resolve() for p in includes}.values(),key=lambda p:str(p).lower())
inputs={str(p):sha(p) for p in [probe,*unique]}
assert unique,'No actual include trace captured'
for p in unique:
 ps=p.as_posix().lower()
 assert '/src/pc/' not in ps,ps
 assert 'boot-e102-diag' not in ps and 'privatevpu' not in ps,ps
 if '/src/' in ps and not ps.startswith(ROOT.as_posix().lower()+'/'):
  assert ps.startswith((CANON/'vendor').as_posix().lower()+'/') or ps.startswith(SDK.as_posix().lower()+'/'),ps
sdk_tools={str(p):sha(p) for p in (SDK/'bin/win32').iterdir() if p.name.lower() in ('cl.exe','c1.dll','c1xx.dll','c2.dll','mspdb80.dll','msvcr80.dll')}
save('actual-inputs.json',{'probe_and_unique_includes':inputs,'header_count':len(unique),'include_trace_count':len(includes),'sdk_tools':sdk_tools,'note':'Actual /showIncludes closure; source snapshot separately binds all frozen6277 files.'})
post=snapshot()
assert post==before
assert git('rev-parse','HEAD')==FROZEN and not git('status','--porcelain=v1')
save('frozen-source-after.json',{'head':FROZEN,'time_utc':datetime.now(timezone.utc).isoformat(),'hashes':post,'tracked_status':git('status','--porcelain=v1'),'unchanged':post==before})
machine=struct.unpack_from('<H',obj.read_bytes())[0] if obj.exists() else None
passed=r.returncode==0 and machine==0x1f2 and asm.is_file()
save('compile-results.json',{'state':'complete','stage':'actual-header-layout-probe','head':FROZEN,'requested':1,'passed':int(passed),'failed':int(not passed),'whole_main_compiled':False,'production_guards_modified':False,'result':{'returncode':r.returncode,'machine':machine,'elapsed_seconds':time.monotonic()-t0,'object':str(obj),'assembly':str(asm),'log':str(log),'command_sha256':sha(OUT/'command.json'),'actual_inputs_sha256':sha(OUT/'actual-inputs.json'),'object_sha256':sha(obj) if obj.exists() else None,'assembly_sha256':sha(asm) if asm.exists() else None,'log_sha256':sha(log)}})
print('PASS' if passed else 'FAIL','rc',r.returncode,'machine',machine,'unique headers',len(unique),'elapsed',round(time.monotonic()-t0,2),flush=True)
if not passed:
 print('\n'.join(x for x in (r.stdout+r.stderr).splitlines() if 'including file:' not in x)[-10000:])
 raise SystemExit(1)
