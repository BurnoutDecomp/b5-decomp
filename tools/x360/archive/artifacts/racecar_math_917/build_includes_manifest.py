from pathlib import Path
import subprocess,json,os,re,hashlib
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/racecar_math_917');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');sdk=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK');env=dict(os.environ);env.update(XEDK=str(sdk),TEMP=str(out/'airram_provider/temp'),TMP=str(out/'airram_provider/temp'),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
for k in ['CL','_CL_','LINK','_LINK_']:env.pop(k,None)
report={}
for unit in ['physics','airram_provider']:
 cmd=json.loads((out/unit/'command.json').read_text());cmd=[('/Fo'+str(out/unit/'includes-probe.obj') if x.startswith('/Fo') else '/Fa'+str(out/unit/'includes-probe.asm') if x.startswith('/Fa') else x) for x in cmd];cmd.insert(1,'/showIncludes');r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace',timeout=180);log=r.stdout+r.stderr;(out/unit/'includes.log').write_text(log,encoding='utf8');assert r.returncode==0,(unit,log[-1000:]);files=[]
 for line in log.splitlines():
  marker='Note: including file:'
  if marker in line:
   path=Path(line.split(marker,1)[1].strip())
   if path.is_file():files.append(path.resolve())
 files=sorted(set(files),key=lambda p:str(p).lower());report[unit]={'count':len(files),'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},'command':cmd,'object_sha256':hashlib.sha256((out/unit/'includes-probe.obj').read_bytes()).hexdigest()};print(unit,len(files))
(out/'actual-includes-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
