import json, os, subprocess
from pathlib import Path
root=Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\effects_lanes_luna_4277')
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave11\logcall_lion_terra')
out.mkdir(parents=True,exist_ok=True)
(out/'temp').mkdir(exist_ok=True)
rows=json.load(open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r11\full-stage1\compile-results.json'))['results']
names=['LionParticleRender.cpp','BrnLionBlendRenderer.cpp','BrnLionBlendIm3d.cpp','ParticleRender.cpp','rwgcoronarenderer.cpp']
paths=[next(x['source'] for x in rows if x['source'].endswith('/'+n)) for n in names]
args=[x for x in rows[0]['command'] if x.startswith('/IC:/Users/adri1/.codex/worktrees/fbaf/')]
sdk=r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK'
cmd=['python','tools/build_x360.py','--sdk',sdk,'--out',str(out/'iteration1'),'--jobs','1','--compiler-arg=/showIncludes']+sum((['--source',p] for p in paths),[])+['--compiler-arg='+a for a in args]
(out/'command.json').write_text(json.dumps(cmd,indent=2))
env=os.environ.copy(); env['TEMP']=env['TMP']=str(out/'temp')
subprocess.run(cmd,cwd=root,env=env)

