from pathlib import Path
import hashlib,json,subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277")
sources=[
"src/GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp",
"src/GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp",
"src/GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp",
"src/GameSource/Director/Camera/Utils/BrnPositionLag.cpp",
"src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp",
]
def sha(p):
 h=hashlib.sha256()
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()
branch=subprocess.check_output(["git","branch","--show-current"],cwd=repo,text=True).strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).strip()
if head!="42772cbe0386462d1c32c8442e9f58c9e534e75b" or status:
 raise SystemExit(f"expected clean frozen R8 intake: head={head} status={status!r}")
headers=set()
for raw in subprocess.check_output(["git","ls-files","-z","--","*.h","*.hpp","*.inl","*.ipp"],cwd=repo).decode().split("\0"):
 if raw:
  p=repo/raw
  if p.is_file():headers.add(p.resolve())
roots=[canonical/"vendor/EABase/include/Common",canonical/"vendor/EASTL/include",canonical/"vendor/EAThread/include",canonical/"vendor/renderware/include",canonical/"vendor/PPMalloc/include",canonical/"vendor/coreallocator/include",canonical/"vendor/dirtysdk/include",canonical/"vendor/zlib/include",canonical/"vendor/lua/src",sdk/"include/xbox"]
exts={".h",".hpp",".inl",".ipp"}
for root in roots:
 if not root.is_dir():raise SystemExit(f"missing canonical include root: {root}")
 for p in root.rglob("*"):
  if p.is_file() and p.suffix.lower() in exts and ".git" not in p.parts:headers.add(p.resolve())
source_rows={}
for rel in sources:
 p=repo/rel
 if not p.is_file():raise SystemExit(f"missing candidate source {rel}")
 blob=subprocess.check_output(["git","rev-parse",f"HEAD:{rel}"],cwd=repo,text=True).strip()
 source_rows[rel]={"git_blob":blob,"sha256":sha(p)}
header_rows={str(p):sha(p) for p in sorted(headers,key=lambda x:str(x).lower())}
manifest={"head":head,"branch":branch,"source_count":len(source_rows),"sources":source_rows,
          "header_count":len(header_rows),"headers":header_rows,"header_roots":[str(p.resolve()) for p in roots],
          "sdk":str(sdk.resolve())}
out=art/"preflight-input-binding.json"
out.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"branch":branch,"sources":len(source_rows),"headers":len(header_rows),"manifest":str(out)},indent=2))