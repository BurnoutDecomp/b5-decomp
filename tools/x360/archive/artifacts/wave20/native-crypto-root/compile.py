from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
paths=('vendor/dirtysdk/src/x360/cryptarc4.cpp','vendor/dirtysdk/src/x360/cryptsha1.cpp',
       'vendor/dirtysdk/src/x360/protossl_x360.cpp','vendor/dirtysdk/src/x360/protohttp.cpp')
assert all((w/p).is_file() for p in paths),[p for p in paths if not (w/p).is_file()]
for p in paths:args+=['--source',p]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
