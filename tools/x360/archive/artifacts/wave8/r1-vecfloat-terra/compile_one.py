from pathlib import Path
import os,json,subprocess,sys
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');sdk=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK')
for name in sys.argv[1:]:
 unit=out/name;cmd=json.loads((unit/'command.json').read_text());temp=unit/'temp';env=dict(os.environ);env.update(XEDK=str(sdk),TEMP=str(temp),TMP=str(temp),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
 for k in ['CL','_CL_','LINK','_LINK_']:env.pop(k,None)
 r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace',timeout=180);log=r.stdout+r.stderr;(unit/'compile.log').write_text(log,encoding='utf8');print(name,'exit',r.returncode,'errors',sum('error C' in x for x in log.splitlines()),flush=True)
