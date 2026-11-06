from pathlib import Path
import subprocess,json,os,hashlib
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/racecar_wave8_consumers');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers');sdk=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK');res={}
for unit in ['BrnLionBlendIm3d','BrnLionBlendRenderer','BrnLionBlendVertex','BrnPhysicalTrafficManager','vecfloat','ParticleModule_Lifecycle']:
 folder=out/unit;cmd=json.loads((folder/'command.json').read_text());cmd=[('/Fo'+str(folder/'includes-probe.obj') if x.startswith('/Fo') else '/Fa'+str(folder/'includes-probe.asm') if x.startswith('/Fa') else x) for x in cmd];cmd.insert(1,'/showIncludes');env=dict(os.environ);env.update(XEDK=str(sdk),TEMP=str(folder/'temp'),TMP=str(folder/'temp'),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
 for k in ['CL','_CL_','LINK','_LINK_']:env.pop(k,None)
 r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace',timeout=180);log=r.stdout+r.stderr;(folder/'includes.log').write_text(log,encoding='utf8');assert r.returncode==0,(unit,log[-500:]);paths=[]
 for line in log.splitlines():
  if 'Note: including file:' in line:
   f=Path(line.split('Note: including file:',1)[1].strip())
   if f.is_file():paths.append(f.resolve())
 paths=sorted(set(paths),key=lambda x:str(x).lower());res[unit]={'count':len(paths),'headers':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths},'command':cmd};print(unit,len(paths),flush=True)
(out/'wave8-final-includes-manifest.json').write_text(json.dumps(res,indent=2)+'\n')
