from pathlib import Path
import hashlib,json,os,subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\gui_events_cpp03_luna_r10")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\gui_events_cpp03_luna_r10")
sources=["src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.cpp","src/GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.cpp","src/GameSource/Game/GameBridgeControllerToX.cpp","src/GameSource/Game/GameBridgeGameStateToX_EventFlowGuiEvents.cpp","src/GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.cpp"]
def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()
branch=subprocess.check_output(["git","branch","--show-current"],cwd=repo,text=True).strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).strip()
if head!="1598404ea16b7e16cbe00396bbd051e38b0d59f4" or status:
 raise SystemExit(f"preflight requires clean frozen R10 intake: {head} {status!r}")
header_paths=set()
for raw in subprocess.check_output(["git","ls-files","-z","--","*.h","*.hpp","*.inl","*.ipp"],cwd=repo).decode().split("\0"):
 if raw:
  p=repo/raw
  if p.is_file():header_paths.add(p.resolve())
roots=[canonical/"vendor/EABase/include/Common",canonical/"vendor/EASTL/include",canonical/"vendor/EAThread/include",canonical/"vendor/renderware/include",canonical/"vendor/PPMalloc/include",canonical/"vendor/coreallocator/include",canonical/"vendor/dirtysdk/include",canonical/"vendor/zlib/include",canonical/"vendor/lua/src",sdk/"include/xbox"]
exts={".h",".hpp",".inl",".ipp"}
for root in roots:
 if not root.is_dir():raise SystemExit(f"missing compiler include root: {root}")
 for dirpath,dirs,files in os.walk(root):
  for name in files:
   p=Path(dirpath)/name
   if ".git" in p.parts and p.suffix.lower() not in exts:continue
   if p.suffix.lower() in exts or root==sdk/"include/xbox":header_paths.add(p.resolve())
source_rows={}
for rel in sources:
 p=repo/rel
 if not p.is_file():raise SystemExit(f"missing source: {rel}")
 blob=subprocess.check_output(["git","rev-parse",f"HEAD:{rel}"],cwd=repo,text=True).strip()
 source_rows[rel]={"git_blob":blob,"sha256":sha(p)}
headers={str(p):sha(p) for p in sorted(header_paths,key=lambda x:str(x).lower())}
manifest={"head":head,"branch":branch,"source_count":len(source_rows),"sources":source_rows,
          "header_count":len(headers),"headers":headers,"header_roots":[str(p.resolve()) for p in roots],
          "sdk":str(sdk.resolve())}
out=art/"preflight-input-binding.json";out.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"branch":branch,"sources":len(source_rows),"headers":len(headers),"manifest":str(out)},indent=2))