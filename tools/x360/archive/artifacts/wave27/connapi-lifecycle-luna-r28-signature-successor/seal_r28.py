import hashlib
import json
import subprocess
from pathlib import Path

repo=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a")
art=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave27/connapi-lifecycle-luna-r28-signature-successor")
base="ce55acf0f72b3b4a91e7f7f2e6824d49eacf2eec"
original="f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331"
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo).decode().strip()
branch=subprocess.check_output(["git","branch","--show-current"],cwd=repo).decode().strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo).decode().strip()
rel="vendor/dirtysdk/src/x360/connapi_x360.cpp"
base_blob=subprocess.check_output(["git","show",base+":"+rel],cwd=repo)
pre=art/"preimages"/(Path(rel).name+".ce55acf0")
if pre.read_bytes()!=base_blob: raise SystemExit("base preimage mismatch")
source=(repo/rel).read_bytes()
source_blob=subprocess.check_output(["git","show",head+":"+rel],cwd=repo)
patch=subprocess.check_output(["git","show","--format=","--binary",head],cwd=repo)
(art/"R28_SIGNATURE_REPAIR.patch").write_bytes(patch)
compile_report=json.loads((art/"native3"/"compile-results.json").read_text(encoding="utf-8"))
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
gate={"stage":"compile-only","state":compile_report["state"],"parent_commit":base,
      "candidate_commit":head,"branch":branch,"driver":str(driver),
      "driver_sha256":hashlib.sha256(driver.read_bytes()).hexdigest(),"sdk":compile_report["sdk"],
      "jobs":1,"timeout_seconds":180,"fallback_include_roots":include_roots,
      "fallback_include_root_count":len(include_roots),"showIncludes":True,
      "out_dir":str(art/"native3"),"TEMP":str(art/"native3-temp"),"TMP":str(art/"native3-temp"),
      "requested":compile_report["requested"],"passed":compile_report["passed"],
      "failed":compile_report["failed"],"results":compile_report["results"]}
(art/"R28_NATIVE_GATE.json").write_text(json.dumps(gate,indent=2)+"\n",encoding="utf-8")
lanes=["artist-protomanglecomplete-caller.txt","artist-voipconnect4-caller.txt",
       "artist-voipremove-caller.txt","artist-netgameutilcontrol-caller.txt",
       "artist-connapi-online-voip-user.txt"]
lane_hashes=[{"path":str(art/name),"sha256":hashlib.sha256((art/name).read_bytes()).hexdigest()} for name in lanes]
decfigs_names=["0xE92D44.json","0xE927CC.json","0xE9416C.json","0xE94080.json","0xE943B0.json","0xE9E690.json"]
decfigs_root=Path(r"D:/Reverse/BP-Decomp_Workflow/.ida-exports/DecFIGS_Burnout_Internal_PS3.ELF")
decfigs=[{"path":str(decfigs_root/name),"sha256":hashlib.sha256((decfigs_root/name).read_bytes()).hexdigest()} for name in decfigs_names]
previous_root=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave27")
previous_paths=[
 previous_root/"connapi-lifecycle-luna"/"R27_RECEIPT.md",
 previous_root/"connapi-lifecycle-luna"/"R27_SEALED_MANIFEST.json",
 previous_root/"connapi-lifecycle-luna"/"R27.patch",
 previous_root/"connapi-lifecycle-luna"/"native3"/"compile-results.json",
 previous_root/"connapi-lifecycle-luna-abi-successor"/"R27_ABI_SELECTOR_RECEIPT.md",
 previous_root/"connapi-lifecycle-luna-abi-successor"/"R27_ABI_SELECTOR_MANIFEST.json",
 previous_root/"connapi-lifecycle-luna-abi-successor"/"R27_ABI_SELECTOR_SUCCESSOR.patch",
 previous_root/"connapi-lifecycle-luna-abi-successor"/"native1"/"compile-results.json"]
previous=[{"path":str(p),"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in previous_paths]
audit=art/"R28_EXTERN_AUDIT.md"
sealed={"original_base_commit":original,"parent_commit":base,"candidate_commit":head,
        "branch":branch,"working_tree_clean":not bool(status),"changed_paths":[rel],
        "preimage":{"path":str(pre),"sha256":hashlib.sha256(base_blob).hexdigest(),
                    "git_blob_sha1":subprocess.check_output(["git","rev-parse",base+":"+rel],cwd=repo).decode().strip(),"bytes":len(base_blob)},
        "source":{"path":rel,"worktree_sha256":hashlib.sha256(source).hexdigest(),
                  "git_blob_sha1":subprocess.check_output(["git","rev-parse",head+":"+rel],cwd=repo).decode().strip(),
                  "git_blob_sha256":hashlib.sha256(source_blob).hexdigest(),"bytes":len(source)},
        "patch":{"path":str(art/"R28_SIGNATURE_REPAIR.patch"),"sha256":hashlib.sha256(patch).hexdigest(),"bytes":len(patch)},
        "native_gate":str(art/"R28_NATIVE_GATE.json"),"extern_audit":{"path":str(audit),"sha256":hashlib.sha256(audit.read_bytes()).hexdigest()},
        "artist_call_lanes":lane_hashes,"decfigs_prototypes":decfigs,
        "previous_frozen_evidence_unchanged":previous,
        "link_performed":False,"integrated":False}
(art/"R28_SIGNATURE_REPAIR_MANIFEST.json").write_text(json.dumps(sealed,indent=2)+"\n",encoding="utf-8")
