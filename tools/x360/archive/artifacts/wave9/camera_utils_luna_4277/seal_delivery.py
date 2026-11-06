from pathlib import Path
import hashlib,json,subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277")
r7dir=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\full-stage1")
r7patch=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\native-parallel-wave8-r7-full-recovered-patch.json")
pre=json.loads((art/"preflight-input-binding.json").read_text(encoding="utf-8"))
initial_dir=art/"initial-five-source-gate"; initial_report_path=initial_dir/"compile-results.json";initial_bind_path=initial_dir/"source-object-header-binding.json"
final_dir=art/"final-five-source-gate"; final_report_path=final_dir/"compile-results.json";final_bind_path=final_dir/"source-object-header-binding.json"
shake_dir=art/"camera-shake-ice-one-file-rerun";shake_report_path=shake_dir/"compile-results.json";shake_bind_path=shake_dir/"source-object-header-binding.json"
relpaths=list(pre["sources"])

def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD");parent=git("rev-parse","HEAD^");branch=git("branch","--show-current");status=git("status","--porcelain")
if head!="038e40e50efa4c3736dc5cf1bb45cbf7bde6f17d" or parent!="42772cbe0386462d1c32c8442e9f58c9e534e75b" or status:
 raise SystemExit(f"receipt requires committed clean camera branch: {head=} {parent=} {status=}")
committed_paths=subprocess.check_output(["git","diff-tree","--no-commit-id","--name-only","-r","HEAD"],cwd=repo,text=True).splitlines()
if set(committed_paths)!=set(relpaths):raise SystemExit(f"commit scope mismatch: {committed_paths}")
r7=json.loads(r7patch.read_text(encoding="utf-8"));r7report=json.loads((r7dir/"compile-results.json").read_text(encoding="utf-8"))
r8parent="190afb3b4597facbd6dc83e66996fc1c1f895fb0"
diff=subprocess.run(["git","diff","--quiet",r8parent,pre["head"],"--",*relpaths],cwd=repo)
if diff.returncode!=0:raise SystemExit("one or more target CPP paths changed in the R8 intake commit")
initial=json.loads(initial_report_path.read_text(encoding="utf-8"));initial_bind=json.loads(initial_bind_path.read_text(encoding="utf-8"))
final=json.loads(final_report_path.read_text(encoding="utf-8"));final_bind=json.loads(final_bind_path.read_text(encoding="utf-8"))
shake=json.loads(shake_report_path.read_text(encoding="utf-8"));shake_bind=json.loads(shake_bind_path.read_text(encoding="utf-8"))
if (initial.get("requested"),initial.get("passed"),initial.get("failed"))!=(5,0,5):raise SystemExit("unexpected baseline gate summary")
if (final.get("requested"),final.get("passed"),final.get("failed"))!=(5,4,1):raise SystemExit("unexpected five-source pre-trace-cleanup gate summary")
if (shake.get("requested"),shake.get("passed"),shake.get("failed"))!=(1,1,0):raise SystemExit("camera-only rerun did not pass")
final_rows={r["source"]:r for r in final_bind["results"]}
shake_row=shake_bind["result"]
shake_source=relpaths[-1]
# Confirm all four unaffected sources still match the five-file gate that passed them.
for rel in relpaths[:-1]:
 if sha(repo/rel)!=final_bind["sources_after"][rel]:raise SystemExit(f"source changed after its final object was built: {rel}")
if sha(repo/shake_source)!=shake_bind["source_sha256_after_gate"]:raise SystemExit("camera-shake final source does not match its one-file object")

# Preserve the exact R7 compiler evidence and source-snapshot match for the five candidates.
r7_rows=[]
for rel in relpaths:
 name=Path(rel).name
 row=next((r for r in r7report["results"] if r["source"].endswith("/"+name)),None)
 if not row:raise SystemExit(f"missing R7 row {rel}")
 log=Path(row["log"]);logtext=log.read_text(encoding="utf-8",errors="replace")
 errors=[line for line in logtext.splitlines() if "error " in line]
 r7_blob=git("rev-parse",f"{r7['head']}:{rel}")
 r8_blob=git("rev-parse",f"{pre['head']}:{rel}")
 if r7["head"]!="129afef95856e383366a55fb8f30a66ceb74de42" or rel in r7["paths"] or r7_blob!=r8_blob or pre["sources"][rel]["git_blob"]!=r7_blob:
  raise SystemExit(f"R7/R8 source provenance mismatch or target changed by R7 patch: {rel}")
 if row["passed"] or len(errors)!=(4,9,3,12,10)[relpaths.index(rel)]:
  raise SystemExit(f"R7 failure evidence mismatch: {rel} diagnostics={len(errors)}")
 r7_rows.append({"path":rel,"source_sha256":pre["sources"][rel]["sha256"],"source_blob_r7":r7_blob,"source_blob_r8":r8_blob,"unchanged_by_r7_recovered_patch":True,"passed":row["passed"],"diagnostic_count":len(errors),"log":str(log.resolve()),"log_sha256":sha(log)})
# Compose final source->object bindings from the four unaffected rows plus the final single-file camera rerun.
source_bindings=[]
for rel in relpaths:
 if rel==shake_source:
  row=shake_row; object_data=row["object"]; log=row["log"]; loghash=row["log_sha256"]; machine=row["machine"]
  source_sha=shake_bind["source_sha256_after_gate"]
  source_pre_blob=pre["sources"][rel]["git_blob"]
  source_compile_stage=str(shake_dir.resolve())
  header_binding_path=str(shake_bind_path.resolve());header_binding_sha=sha(shake_bind_path)
  actual_headers=shake_bind["actual_header_count"];matched_headers=shake_bind["preflight_bound_header_count"]
 else:
  row=final_rows[rel];object_data=row["object"];log=row["log"];loghash=row["log_sha256"];machine=row["machine"]
  source_sha=final_bind["sources_after"][rel]
  source_pre_blob=pre["sources"][rel]["git_blob"]
  source_compile_stage=str(final_dir.resolve())
  header_binding_path=str(final_bind_path.resolve());header_binding_sha=sha(final_bind_path)
  actual_headers=final_bind["actual_header_count"];matched_headers=final_bind["preflight_bound_header_count"]
 if not row["passed"] or machine!=0x01F2 or not object_data or not object_data["verified_ppc32be_coff"]:
  raise SystemExit(f"no final PPC32BE object for {rel}")
 source_bindings.append({"path":rel,"parent_blob":source_pre_blob,"preflight_sha256":pre["sources"][rel]["sha256"],"committed_blob":git("rev-parse",f"{head}:{rel}"),"committed_sha256":sha(repo/rel),"compile_stage":source_compile_stage,"passed":row["passed"],"returncode":row["returncode"],"machine":machine,"machine_hex":"0x01F2","diagnostic_count":row["diagnostic_count"],"object":object_data["path"],"object_sha256":object_data["sha256"],"log":log,"log_sha256":loghash,"header_binding":header_binding_path,"header_binding_sha256":header_binding_sha,"actual_header_count":actual_headers,"preflight_bound_header_count":matched_headers})
# ARTIST call-list artifact records the non-native debug logging absence used to retire the undeclared trace block.
artist=Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x8223EEC8.json")
j=json.loads(artist.read_text(encoding="utf-8"))
calls=[line.strip() for line in j["assembly"].splitlines() if " bl " in line]
call_out=art/"artist-camera-shake-calls.txt"
call_out.write_text("source_json="+str(artist.resolve())+"\nsource_json_sha256="+sha(artist)+"\nfunction="+j["name"]+"\n"+"\n".join(calls)+"\n",encoding="utf-8")
if any("gpDebugPrint" in line or "CgsDev__Log" in line for line in calls):raise SystemExit("unexpected native ARTIST logger call found")
receipt={
 "schema":"native-x360-camera-utils-delivery-v1",
 "objective":"Replace invalid C++03 Vector3 field/lambda spellings and disambiguate native math types in the five owned camera utility TUs, preserving lanes, scalar expression order and call order.",
 "repository":"b5-decomp","private_worktree":str(repo),"branch":branch,"commit":head,"parent":parent,"base":pre["head"],
 "co_author_trailer":"Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>","published":False,"private_clean_branch":True,
 "scope":{"changed_paths":committed_paths,"headers_changed":False,"committed_file_count":5,"PC_facades_added":False,"new_helper_headers_added":False},
 "frozen_inputs":{"head":pre["head"],"r8_parent":r8parent,"r8_direct_parent_diff_has_no_target_cpp_changes":True,"preflight_manifest":str((art/"preflight-input-binding.json").resolve()),"preflight_sha256":sha(art/"preflight-input-binding.json"),"source_count":pre["source_count"],"preflight_header_count":pre["header_count"],"all_actual_headers_preflight_bound":True},
 "r7_evidence":{"report":str((r7dir/"compile-results.json").resolve()),"report_sha256":sha(r7dir/"compile-results.json"),"full_recovered_patch":str(r7patch.resolve()),"full_recovered_patch_sha256":sha(r7patch),"files":r7_rows},
 "initial_actual_gate":{"stage":str(initial_dir.resolve()),"requested":initial["requested"],"passed":initial["passed"],"failed":initial["failed"],"driver_returncode":initial_bind["driver_returncode"],"report":str(initial_report_path.resolve()),"report_sha256":sha(initial_report_path),"binding":str(initial_bind_path.resolve()),"binding_sha256":sha(initial_bind_path)},
 "intermediate_gate":{"stage":str(final_dir.resolve()),"requested":final["requested"],"passed":final["passed"],"failed":final["failed"],"report":str(final_report_path.resolve()),"report_sha256":sha(final_report_path),"binding":str(final_bind_path.resolve()),"binding_sha256":sha(final_bind_path),"camera_trace_diagnostic":next((e for r in final_bind["results"] if r["source"]==shake_source for e in r["diagnostics"]),None),"resolution":"Removed the non-native CgsDev::Log debug trace; the ARTIST Update export call list contains no logger call."},
 "final_composite_native_gate":{"target":"xbox360-ppc-be","sdk":final_bind["sdk"],"jobs":1,"showIncludes":True,"canonical_vendor_fallbacks":["EABase/include/Common","EASTL/include","EAThread/include","renderware/include","PPMalloc/include","coreallocator/include","dirtysdk/include","zlib/include","lua/src"],"source_count":5,"passed":5,"failed":0,"all_objects_machine":"0x01F2","sources":source_bindings,"camera_shake_only_rerun":{"stage":str(shake_dir.resolve()),"requested":shake["requested"],"passed":shake["passed"],"failed":shake["failed"],"returncode":shake_bind["driver_returncode"],"compile_report":str(shake_report_path.resolve()),"compile_report_sha256":sha(shake_report_path),"binding":str(shake_bind_path.resolve()),"binding_sha256":sha(shake_bind_path),"actual_headers":shake_bind["actual_header_count"],"preflight_bound_headers":shake_bind["preflight_bound_header_count"],"all_preflight_bound_headers_unchanged":shake_bind["preflight_bound_headers_unchanged"]}},
 "non_native_trace_evidence":{"ARTIST_function":"BrnDirector::Camera::Utils::CameraShakeICEController::Update","ARTIST_address":"0x8223EEC8","call_list":str(call_out.resolve()),"call_list_sha256":sha(call_out),"removed_source":"CgsDev::Log::gpDebugPrint trace block had an undeclared trace flag and no counterpart in the X360 call list."},
 "verification_limits":["Compile-only; no link, image, or runtime launch was performed.","No source-mirroring or optional case-suite tests were added or run.","The final five-source result is a composite: four unchanged sources passed the five-file gate; after the sole remaining camera source fix, that changed source alone was recompiled and passed." ]
}
out=art/"DELIVERY.json";out.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"delivery":str(out),"sha256":sha(out),"commit":head,"parent":parent,"sources":len(source_bindings),"final_pass":5,"final_fail":0,"objects":[{"source":r["path"],"machine":r["machine_hex"],"sha256":r["object_sha256"]} for r in source_bindings]},indent=2))