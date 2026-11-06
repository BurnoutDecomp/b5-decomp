import hashlib
import json
import subprocess
from pathlib import Path

repo=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a")
art=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave27/connapi-lifecycle-luna-abi-successor")
base="361747525a075f8d744aac4a47aacdc1a078fc15"
original_base="f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331"
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo).decode().strip()
branch=subprocess.check_output(["git","branch","--show-current"],cwd=repo).decode().strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo).decode().strip()
changed=["vendor/dirtysdk/include/connapi.h","vendor/dirtysdk/src/x360/connapi_x360.cpp","vendor/dirtysdk/src/x360/gamemanager.cpp"]
pre_dir=art/"preimages"; pre_dir.mkdir(parents=True,exist_ok=True)
preimages=[]
for rel in changed:
    blob=subprocess.check_output(["git","show",base+":"+rel],cwd=repo)
    dst=pre_dir/(Path(rel).name+".ce55acf0-base")
    dst.write_bytes(blob)
    preimages.append({"path":rel,"artifact":str(dst),"sha256":hashlib.sha256(blob).hexdigest(),
                      "git_blob_sha1":subprocess.check_output(["git","rev-parse",base+":"+rel],cwd=repo).decode().strip(),
                      "bytes":len(blob)})
source_hashes=[]
for rel in changed:
    data=(repo/rel).read_bytes()
    source_hashes.append({"path":rel,"worktree_sha256":hashlib.sha256(data).hexdigest(),
                          "git_blob_sha1":subprocess.check_output(["git","rev-parse",head+":"+rel],cwd=repo).decode().strip(),
                          "bytes":len(data)})
patch=subprocess.check_output(["git","show","--format=","--binary",head],cwd=repo)
(art/"R27_ABI_SELECTOR_SUCCESSOR.patch").write_bytes(patch)
compile_report=json.loads((art/"native1"/"compile-results.json").read_text(encoding="utf-8"))
include_roots=[
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EABase\include\Common",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EASTL\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EAThread\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\renderware\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\PPMalloc\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\coreallocator\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\dirtysdk\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\zlib\include",
 r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\lua\src"]
driver=repo/"tools/build_x360.py"
gate={"stage":"compile-only","state":compile_report["state"],"parent_commit":base,"candidate_commit":head,
      "branch":branch,"driver":str(driver),"driver_sha256":hashlib.sha256(driver.read_bytes()).hexdigest(),
      "sdk":compile_report["sdk"],"jobs":1,"timeout_seconds":180,"fallback_include_roots":include_roots,
      "fallback_include_root_count":len(include_roots),"showIncludes":True,
      "out_dir":str(art/"native1"),"TEMP":str(art/"native1-temp"),"TMP":str(art/"native1-temp"),
      "requested":compile_report["requested"],"passed":compile_report["passed"],"failed":compile_report["failed"],
      "results":compile_report["results"]}
(art/"R27_ABI_SELECTOR_GATE.json").write_text(json.dumps(gate,indent=2)+"\n",encoding="utf-8")
prev=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave27/connapi-lifecycle-luna")
previous_paths=[prev/"R27_RECEIPT.md",prev/"R27_SEALED_MANIFEST.json",prev/"R27.patch",prev/"native3"/"compile-results.json"]
previous_evidence=[{"path":str(p),"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in previous_paths]
sealed={"original_base_commit":original_base,"parent_candidate_commit":base,"candidate_commit":head,
        "branch":branch,"working_tree_clean":not bool(status),"changed_paths":changed,
        "source_hashes":source_hashes,"preimages":preimages,
        "patch":{"path":str(art/"R27_ABI_SELECTOR_SUCCESSOR.patch"),"sha256":hashlib.sha256(patch).hexdigest(),"bytes":len(patch)},
        "native_gate":str(art/"R27_ABI_SELECTOR_GATE.json"),"previous_candidate_evidence_unchanged":previous_evidence,
        "link_performed":False,"integrated":False}
(art/"R27_ABI_SELECTOR_MANIFEST.json").write_text(json.dumps(sealed,indent=2)+"\n",encoding="utf-8")
