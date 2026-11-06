from pathlib import Path
import os,json,subprocess,importlib.util,concurrent.futures
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/racecar_vecfloat');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/caller_checks');out.mkdir(parents=True,exist_ok=True);can=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp');sdk=can/'build/x360/sdk-6534.4/portable/XDK';sp=importlib.util.spec_from_file_location('builder',root/'tools/build_x360.py');b=importlib.util.module_from_spec(sp);sp.loader.exec_module(b)
sources=['src/GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.cpp','src/GameSource/World/ShadowMap/BrnShadowMap.cpp']
def run(item):
 p=root/item;d=out/p.stem;d.mkdir(exist_ok=True);tmp=d/'temp';tmp.mkdir(exist_ok=True);env=dict(os.environ);env.update(XEDK=str(sdk),TEMP=str(tmp),TMP=str(tmp),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'))
 for k in ['CL','_CL_','LINK','_LINK_']:env.pop(k,None)
 cmd=[str(sdk/'bin/win32/cl.exe')]+b.FLAGS+['/I'+str(root/n) for n in b.INCLUDES if (root/n).is_dir()]+['/I'+str(can/n) for n in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include']]+['/I'+str(sdk/'include/xbox'),'/Fo'+str(d/'obj.obj'),str(p)];r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace',timeout=180);(d/'compile.log').write_text(r.stdout+r.stderr,encoding='utf8');(d/'command.json').write_text(json.dumps(cmd,indent=2));return {'source':item,'exit':r.returncode,'first_errors':(r.stdout+r.stderr).splitlines()[:8]}
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as ex: results=list(ex.map(run,sources))
(out/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
