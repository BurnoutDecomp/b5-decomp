import hashlib
import json
import subprocess
from pathlib import Path

repo=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a")
art=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave27/connapi-lifecycle-luna")
base="f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331"
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo).decode().strip()
branch=subprocess.check_output(["git","branch","--show-current"],cwd=repo).decode().strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo).decode().strip()
changed=["vendor/dirtysdk/src/x360/connapi_x360.cpp","vendor/dirtysdk/include/connapi.h","vendor/dirtysdk/src/x360/gamemanager.cpp"]
pre_dir=art/"preimages"
pre_dir.mkdir(parents=True,exist_ok=True)
preimages=[]
for rel in changed:
    blob=subprocess.check_output(["git","show",base+":"+rel],cwd=repo)
    dst=pre_dir/(Path(rel).name+".f3cb84e9")
    if not dst.exists(): dst.write_bytes(blob)
    snapshot=dst.read_bytes()
    normalize=lambda b:b.replace(b"\r\n",b"\n")
    if normalize(snapshot)!=normalize(blob): raise SystemExit("preimage content mismatch: "+rel)
    blob_id=subprocess.check_output(["git","rev-parse",base+":"+rel],cwd=repo).decode().strip()
    preimages.append({"path":rel,"artifact":str(dst),"snapshot_sha256":hashlib.sha256(snapshot).hexdigest(),
                      "snapshot_bytes":len(snapshot),"git_blob_sha1":blob_id,
                      "git_blob_sha256":hashlib.sha256(blob).hexdigest(),
                      "normalized_contents_match":True})
source_hashes=[]
for rel in changed:
    data=(repo/rel).read_bytes()
    blob_id=subprocess.check_output(["git","rev-parse",head+":"+rel],cwd=repo).decode().strip()
    source_hashes.append({"path":rel,"worktree_sha256":hashlib.sha256(data).hexdigest(),
                          "git_blob_sha1":blob_id,"bytes":len(data)})
patch=subprocess.check_output(["git","show","--format=","--binary",head],cwd=repo)
(art/"R27.patch").write_bytes(patch)
artist=art/"ARTIST_82B14608.ppcdis.txt"
callers=art/"ida-copy"/"callers.log"
data_log=art/"ida-copy"/"data.log"
compile_report=json.loads((art/"native3"/"compile-results.json").read_text(encoding="utf-8"))
deps=json.loads((art/"native3"/"object-dependencies.json").read_text(encoding="utf-8"))
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
gate={"stage":"compile-only","state":compile_report["state"],"base_commit":base,"candidate_commit":head,
      "branch":branch,"driver":str(driver),"driver_sha256":hashlib.sha256(driver.read_bytes()).hexdigest(),
      "sdk":compile_report["sdk"],"jobs":1,"timeout_seconds":180,"fallback_include_roots":include_roots,
      "fallback_include_root_count":len(include_roots),"showIncludes":True,
      "out_dir":str(art/"native3"),"TEMP":str(art/"native3-temp"),"TMP":str(art/"native3-temp"),
      "requested":compile_report["requested"],"passed":compile_report["passed"],"failed":compile_report["failed"],
      "results":compile_report["results"]}
(art/"R27_NATIVE_GATE.json").write_text(json.dumps(gate,indent=2)+"\n",encoding="utf-8")
initial=json.loads((art/"R27_INPUT_MANIFEST.json").read_text(encoding="utf-8"))
sealed={"base_commit":base,"candidate_commit":head,"branch":branch,"working_tree_clean":not bool(status),
        "changed_paths":changed,"source_hashes":source_hashes,"preimages":preimages,
        "patch":{"path":str(art/"R27.patch"),"sha256":hashlib.sha256(patch).hexdigest(),"bytes":len(patch)},
        "frozen_r25_link":{"path":initial["frozen_link_source"],"sha256":initial["frozen_link_sha256"],
                            "unique_unresolved":initial["frozen_link_unique_symbols"],
                            "unresolved_messages":initial["frozen_link_unresolved_messages"],
                            "duplicates":initial["frozen_link_duplicates"]},
        "native_gate":str(art/"R27_NATIVE_GATE.json"),
        "object_dependencies":str(art/"native3"/"object-dependencies.json"),
        "connapi_object_undefined_count":deps[0]["undefined_count"],
        "connapi_object_internal_sub_refs":deps[0]["unresolved_internal_sub_82B_names"],
        "defined_connapi_public":deps[0]["defined_connapi_public"],
        "artist_game_client_update":{"path":str(artist),"sha256":hashlib.sha256(artist.read_bytes()).hexdigest(),"instructions":277},
        "artist_native_callers":{"path":str(callers),"sha256":hashlib.sha256(callers.read_bytes()).hexdigest()},
        "artist_tunnel_key_bytes":{"path":str(data_log),"sha256":hashlib.sha256(data_log.read_bytes()).hexdigest()},
        "link_performed":False,"integrated":False}
(art/"R27_SEALED_MANIFEST.json").write_text(json.dumps(sealed,indent=2)+"\n",encoding="utf-8")
