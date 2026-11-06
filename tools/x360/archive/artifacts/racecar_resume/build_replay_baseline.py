from pathlib import Path
import subprocess,hashlib,os,json
root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/racecar_containment');rel='src/GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.h';p=root/rel;candidate=p.read_bytes();baseline=subprocess.check_output(['git','show','HEAD:'+rel],cwd=root);out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume/typed_replay_frame_baseline');cmd=['C:/Python312/python.exe','tools/build_x360.py','--sdk','C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK','--source','src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.cpp','--out',str(out),'--jobs','1','--timeout','180'];
for s in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include']:cmd.append('--compiler-arg=/IC:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/'+s)
try:
 p.write_bytes(baseline);r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=240);print(r.stdout,r.stderr);assert r.returncode==0
finally:p.write_bytes(candidate)
assert p.read_bytes()==candidate
