from pathlib import Path
import hashlib, json, subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\successor-origin-rightup")
rel="src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp"
previous=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\five-failure-native-gate-final\source-object-header-binding.json")
prev=json.loads(previous.read_text(encoding="utf-8"))
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""): h.update(block)
    return h.hexdigest()
def git(*args): return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD")
status=git("status","--porcelain")
if head!="e52679267f7c4b1e0403b9f240aa5f3a1758bec1" or status:
    raise SystemExit(f"unexpected successor preflight state: {head} {status!r}")
headers=[]
for row in prev["actual_headers"]:
    path=Path(row["path"])
    digest=sha(path)
    if digest != row["sha256"]:
        raise SystemExit(f"header changed since 5-source gate: {path}")
    headers.append({"path":str(path.resolve()),"sha256":digest})
p=repo/rel
manifest={"parent":head,"branch":git("branch","--show-current"),
          "source":{"path":rel,"parent_blob":git("rev-parse",f"HEAD:{rel}"),"pre_sha256":sha(p)},
          "prior_header_binding":str(previous.resolve()),"prior_header_binding_sha256":sha(previous),
          "header_count":len(headers),"headers":headers}
(art/"successor-preflight.json").write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"source":manifest["source"],"header_count":len(headers),"headers_all_match_prior_gate":True,"manifest":str(art/"successor-preflight.json")},indent=2))