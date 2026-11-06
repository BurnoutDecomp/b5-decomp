from pathlib import Path
import json,hashlib,subprocess,os,ctypes,shutil,time
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/main-layout-526adc');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/world-containment-526adc');out.mkdir(exist_ok=True)
root=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
expected=json.loads((base/'frozen-source-before.json').read_text())['hashes']; snap=lambda:{p:sha(root/p) for p in expected}
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()=='526adc27662c0c2aedf417980dcf824c989bb035'
assert not subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).strip()
assert snap()==expected
src=out/'world-layout-probe.cpp';shutil.copyfile('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/world_layout_sol/world-layout-probe.cpp',src)
record=json.loads((base/'command.json').read_text());cmd=[x for x in record['argv'] if not x.startswith(('/Fo','/Fa'))][:-1];obj=out/'world-layout-probe.obj';asm=out/'world-layout-probe.asm';cmd+=['/Fo'+str(obj),'/Fa'+str(asm),str(src)]
tmp=out/'temp';tmp.mkdir(exist_ok=True);env=dict(os.environ);env.update(record['environment']);env.update(TMP=str(tmp),TEMP=str(tmp))
for k in ('CL','_CL_','LINK','_LINK_'):env.pop(k,None)
ctypes.windll.kernel32.SetErrorMode(0x8003)
r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace',timeout=180,creationflags=subprocess.CREATE_NO_WINDOW);(out/'world-layout-probe.log').write_text(r.stdout+r.stderr,encoding='utf8');print('exit',r.returncode,'unchanged',snap()==expected)
assert r.returncode==0 and snap()==expected
report={'head':'526adc27662c0c2aedf417980dcf824c989bb035','requested':1,'passed':1,'results':[{'source':str(src),'object':str(obj),'assembly':str(asm)}]}
(out/'compile-results.json').write_text(json.dumps(report,indent=2));(out/'manifest.json').write_text(json.dumps({'probe_sha256':sha(src),'object_sha256':sha(obj),'assembly_sha256':sha(asm),'boot_snapshot_sha256':sha(base/'frozen-source-before.json'),'boot_inputs_sha256':sha(base/'actual-inputs.json'),'command':cmd,'snapshot_verified_before_after':True},indent=2))
