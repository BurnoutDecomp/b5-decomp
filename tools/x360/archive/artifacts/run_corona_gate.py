import json,os,subprocess
from pathlib import Path
r=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave11\corona-native-terra');r.mkdir(parents=True,exist_ok=True); (r/'temp').mkdir(exist_ok=True)
base=json.load(open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r11\full-stage1\compile-results.json'))['results'][0]['command']; inc=[x for x in base if x.startswith('/IC:/Users/adri1/.codex/worktrees/fbaf/')]; sdk=r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
cmd=['python','tools/build_x360.py','--sdk',sdk,'--out',str(r/'stage1'),'--jobs','1','--compiler-arg=/showIncludes','--source','src/SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronarenderer.cpp']+['--compiler-arg='+x for x in inc]
(r/'command.json').write_text(json.dumps(cmd,indent=2)); env=os.environ.copy();env['TEMP']=env['TMP']=str(r/'temp');subprocess.run(cmd,env=env)
