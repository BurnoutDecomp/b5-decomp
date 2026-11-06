from pathlib import Path
import hashlib,json,subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\successor-camera-shake-vector3-wzero")
pre=json.loads((art/"successor-preflight.json").read_text(encoding="utf-8"))
gate=art/"camera-shake-ice-native-gate"
bindpath=gate/"source-object-header-binding.json"
bind=json.loads(bindpath.read_text(encoding="utf-8"))
reportpath=gate/"compile-results.json";report=json.loads(reportpath.read_text(encoding="utf-8"))
rel="src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"
vector_rel="src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h"
vector_header=repo/vector_rel
prior_receipt=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\DELIVERY.json")
def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
commit=git("rev-parse","HEAD");parent=git("rev-parse","HEAD^");branch=git("branch","--show-current");status=git("status","--porcelain")
if commit!="9c3d2db104691d347ab41eb4967897397c06bed1" or parent!="038e40e50efa4c3736dc5cf1bb45cbf7bde6f17d" or status:
 raise SystemExit(f"expected clean W-lane successor: {commit=} {parent=} {status!r}")
paths=subprocess.check_output(["git","diff-tree","--no-commit-id","--name-only","-r","HEAD"],cwd=repo,text=True).splitlines()
if paths!=[rel]:raise SystemExit(f"unexpected successor paths: {paths}")
if report.get("requested")!=1 or report.get("passed")!=1 or report.get("failed")!=0 or len(report.get("results",[]))!=1:
 raise SystemExit("single-file XDK gate did not pass 1/1")
row=report["results"][0];obj=bind["result"]["object"]
if not row["passed"] or obj["machine"]!=0x01F2 or not obj["verified_ppc32be_coff"]:raise SystemExit(f"invalid native object {obj}")
if bind["actual_header_count"]!=124 or bind["preflight_bound_header_count"]!=124 or not bind["preflight_bound_headers_unchanged"]:raise SystemExit("include binding incomplete or changed")
pre_source=pre["source"]
pre_vector=pre["vector3_constructor_header"]
if pre_vector["path"]!=str(vector_header.resolve()) or sha(vector_header)!=pre_vector["sha256"]:raise SystemExit("Vector3 constructor header drifted")
if sha(repo/rel)!=bind["source_sha256_after_gate"]:raise SystemExit("committed source does not match compiled source")
receipt={
 "schema":"native-x360-camera-utils-vector3-w-zero-successor-v1",
 "objective":"Initialize the fourth lane of lTargetCameraAnglesRandom through the verified native Vector3(float,float,float) constructor, whose implementation writes W=0.",
 "repository":"b5-decomp","private_worktree":str(repo),"branch":branch,"commit":commit,"parent":parent,
 "base":"42772cbe0386462d1c32c8442e9f58c9e534e75b","prior_commit_preserved":True,
 "previous_delivery_receipt":str(prior_receipt.resolve()),"previous_delivery_receipt_sha256":sha(prior_receipt),
 "co_author_trailer":"Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>","published":False,"private_clean_branch":True,
 "source_change":{"path":rel,"parent_blob":pre_source["parent_blob"],"preflight_sha256":pre_source["pre_sha256"],"committed_blob":git("rev-parse",f"{commit}:{rel}"),"committed_sha256":sha(repo/rel),"changed_files":1,"changed_headers":0,"corrected_initialization":"Vector3(computedPitch, 0.0f, 0.0f);"},
 "native_constructor_evidence":{"header":str(vector_header.resolve()),"header_sha256":sha(vector_header),"header_blob":pre_vector["git_blob"],"constructor_declaration":"Vector3(float x, float y, float z);","constructor_implementation_path":str(vector_header.resolve()),"implementation_lines":{"x":"mV.v[0] = x;","y":"mV.v[1] = y;","z":"mV.v[2] = z;","w":"mV.u[3] = 0;"},"w_value":"positive float zero (0x00000000)","implementation_comment":"ARTIST821F0DD8/821F0E00: scalar inputs assembled, then one raw row copy."},
 "native_compile_gate":{"stage":str(gate.resolve()),"target":"xbox360-ppc-be","sdk":bind["sdk"],"jobs":1,"showIncludes":True,"requested":1,"passed":1,"failed":0,"returncode":bind["driver_returncode"],"machine":"0x01F2","source_object_header_binding":str(bindpath.resolve()),"source_object_header_binding_sha256":sha(bindpath),"compile_report":str(reportpath.resolve()),"compile_report_sha256":sha(reportpath),"driver_log":bind["driver_log"],"driver_log_sha256":bind["driver_log_sha256"],"object":obj,"actual_header_count":bind["actual_header_count"],"preflight_bound_header_count":bind["preflight_bound_header_count"],"preflight_bound_headers_unchanged":bind["preflight_bound_headers_unchanged"]},
 "preflight":{"path":str((art/"successor-preflight.json").resolve()),"sha256":sha(art/"successor-preflight.json"),"prior_camera_gate_binding":pre["preflight_headers_from_prior_camera_gate"],"header_snapshot_count":pre["header_count"]},
 "retained_bindings":{"other_four_camera_utility_objects":"Preserved from the preceding camera-utils delivery; only CameraShakeICEController.cpp was recompiled and replaced.","previous_receipt_path":str(prior_receipt.resolve())},
 "verification_limits":["One translation unit compile-only; no link/image/runtime launch.","No additional source, header, or test changes."]
}
out=art/"DELIVERY.json";out.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"delivery":str(out),"sha256":sha(out),"commit":commit,"parent":parent,"object_sha256":obj["sha256"],"compiled":"1/1","machine":"0x01F2","headers":bind["actual_header_count"]},indent=2))