from pathlib import Path
import subprocess,os,json
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/bootlegal-overlay-retention');D.mkdir(parents=True,exist_ok=True)
paths=[x.replace('\\','/') for x in subprocess.check_output(['rg','-l','GuiOverlayRequest','src','-g','*.cpp'],text=True).splitlines()]
extras=['native_bootlegal_probe.cpp','native_overlay_provider_probe.cpp','src/GameSource/Gui/BrnGuiMovieManager.cpp','src/GameSource/Gui/Flow/HUD/States/BrnBootLegalBoundary.cpp','src/GameSource/Gui/Flow/HUD/States/BrnBootVideos.cpp','src/GameSource/Gui/Flow/HUD/States/BrnPostTitleScreenLoad.cpp','src/GameSource/Gui/Flow/Screen/States/BrnVideo.cpp','src/GameSource/Gui/Flow/Screen/States/BrnIntro.cpp','src/GameSource/Resource/BrnDLCManager.cpp']
paths=list(dict.fromkeys(paths+extras));(D/'source-inventory.json').write_text(json.dumps(paths,indent=2)+'\n');sdk='C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK';vendor='C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/'
cmd=['python','-u','tools/build_x360.py','--sdk',sdk,'--out',str(D),'--jobs','1','--timeout','180']
for p in paths:cmd+=['--source',p]
for p in ['EABase/include/Common','EASTL/include','EAThread/include']:cmd+=['--compiler-arg=/I'+vendor+p]
e=os.environ.copy();e['TEMP']=e['TMP']='D:/codex-native/bootlegal-temp';print('Checking',len(paths),'actual sources',flush=True);raise SystemExit(subprocess.call(cmd,env=e))
