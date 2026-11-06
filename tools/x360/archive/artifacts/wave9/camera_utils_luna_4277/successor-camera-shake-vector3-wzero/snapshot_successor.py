from pathlib import Path
import hashlib,json,subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\successor-camera-shake-vector3-wzero")
rel="src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"
prior=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\camera-shake-ice-one-file-rerun\source-object-header-binding.json")
prev=json.loads(prior.read_text(encoding="utf-8"))
def sha(p):
 h=hashlib.sha256()
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD");status=git("status","--porcelain")
if head!="038e40e50efa4c3736dc5cf1bb45cbf7bde6f17d" or status:raise SystemExit(f"expected clean source candidate: {head} {status!r}")
headers=[]
for h in prev["actual_headers"]:
 p=Path(h["path"]);d=sha(p)
 if d!=h["sha256"]:raise SystemExit(f"header drift before successor: {p}")
 headers.append({"path":str(p.resolve()),"sha256":d})
source=repo/rel
vector_header=repo/"src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h"
manifest={"parent":head,"branch":git("branch","--show-current"),"source":{"path":rel,"parent_blob":git("rev-parse",f"HEAD:{rel}"),"pre_sha256":sha(source)},"preflight_headers_from_prior_camera_gate":str(prior.resolve()),"prior_binding_sha256":sha(prior),"header_count":len(headers),"headers":headers,"vector3_constructor_header":{"path":str(vector_header.resolve()),"git_blob":git("rev-parse",f"HEAD:src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h"),"sha256":sha(vector_header)}}
out=art/"successor-preflight.json";out.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"source":manifest["source"],"actual_header_count":len(headers),"vector3_ctor_header_sha256":manifest["vector3_constructor_header"]["sha256"],"manifest":str(out)},indent=2))