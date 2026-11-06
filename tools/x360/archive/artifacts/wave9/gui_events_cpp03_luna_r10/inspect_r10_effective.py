import hashlib,json,subprocess
from pathlib import Path
repo=Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
effective=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r10\native-parallel-wave8-r10-composite-effective.json")
source_commit="1598404ea16b7e16cbe00396bbd051e38b0d59f4"
targets=[
"src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.cpp",
"src/GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.cpp",
"src/GameSource/Game/GameBridgeControllerToX.cpp",
"src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp",
"src/GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.cpp",
]
def sha(p):
 h=hashlib.sha256()
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
report=json.loads(effective.read_text(encoding="utf-8"))
found={t:[] for t in targets}
def walk(obj):
 if isinstance(obj,dict):
  s=obj.get("source")
  if s in found:
   keys=[k for k in obj if any(x in k.lower() for x in ("pass","fail","error","log","machine","sha","object","return","diagnostic"))]
   found[s].append({k:obj[k] for k in ["source"]+keys if k in obj})
  for v in obj.values():walk(v)
 elif isinstance(obj,list):
  for v in obj:walk(v)
walk(report)
out=[]
for rel in targets:
 blob=git("rev-parse",f"{source_commit}:{rel}")
 current_blob=git("rev-parse",f"HEAD:{rel}")
 rows=found[rel]
 out.append({"path":rel,"source_blob_r10":blob,"current_r10_blob":current_blob,"current_sha256":sha(repo/rel),"rows":rows})
print(json.dumps({"r10_report":str(effective),"report_sha256":sha(effective),"r10_head":source_commit,"frozen_worktree_head":git("rev-parse","HEAD"),"rows":out},indent=2))