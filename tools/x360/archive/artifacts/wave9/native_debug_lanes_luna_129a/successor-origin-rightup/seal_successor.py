from pathlib import Path
import hashlib, json, subprocess
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\successor-origin-rightup")
rel="src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp"
pre=json.loads((art/"successor-preflight.json").read_text(encoding="utf-8"))
gate=art/"im2d-transform-native-gate"
bindpath=gate/"source-object-header-binding.json"
bind=json.loads(bindpath.read_text(encoding="utf-8"))
reportpath=gate/"compile-results.json"
report=json.loads(reportpath.read_text(encoding="utf-8"))
proof=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\artist-static-init-proof")
init=proof/"static_initializer.json"
constants=art/"artist-constant-values.txt"
header=repo/"src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""):h.update(block)
    return h.hexdigest()
def git(*args): return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
commit=git("rev-parse","HEAD"); parent=git("rev-parse","HEAD^"); branch=git("branch","--show-current")
status=git("status","--porcelain")
if parent!="e52679267f7c4b1e0403b9f240aa5f3a1758bec1" or status:
    raise SystemExit(f"receipt requires clean narrow successor: commit={commit} parent={parent} status={status!r}")
if report.get("requested")!=1 or report.get("passed")!=1 or report.get("failed")!=0 or len(report.get("results",[]))!=1:
    raise SystemExit(f"single-file native gate was not clean: {report}")
row=report["results"][0]
if not row["passed"] or row["machine"]!=0x01F2 or row["returncode"]!=0:
    raise SystemExit(f"native object validation failed: {row}")
headers={h["path"].lower():h["sha256"] for h in pre["headers"]}
header_rows=[]
for h in bind["actual_headers"]:
    if not h["preflight_bound"] or not h["unchanged"]:
        raise SystemExit(f"header was not preflight-bound and unchanged: {h}")
    header_rows.append(h)
if len(header_rows)!=33:
    raise SystemExit(f"expected single-file gate's 33 actual bound headers, got {len(header_rows)}")
source_before=pre["source"]
object_binding=row["object"]
source_after=sha(repo/rel)
header_sha=sha(header)
init_json=json.loads(init.read_text(encoding="utf-8"))
receipt={
    "schema":"native-x360-im2d-origin-rightup-successor-v1",
    "objective":"Correct the Im2dTransform default's named native member order using ARTIST stores; retain the prior commit and add only the two initializer values as a successor.",
    "repository":"b5-decomp",
    "private_worktree":str(repo),
    "branch":branch,
    "commit":commit,
    "parent":parent,
    "original_base":"129afef95856e383366a55fb8f30a66ceb74de42",
    "supersedes_candidate_commit":"e52679267f7c4b1e0403b9f240aa5f3a1758bec1",
    "prior_commit_rewritten":False,
    "co_author_trailer":"Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>",
    "private_clean_branch":True,
    "integration_hold":True,
    "source_change":{
        "path":rel,
        "parent_blob":source_before["parent_blob"],
        "preflight_sha256":source_before["pre_sha256"],
        "committed_blob":git("rev-parse",f"{commit}:{rel}"),
        "committed_sha256":source_after,
        "changed_lines":2,
        "header_path":"src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h",
        "header_sha256":header_sha,
        "headers_changed":False,
        "corrected_assignments":{
            "mOriginXYZ":[-1.0,1.0,0.0,0.0],
            "mRightUp":[2.0,0.0,0.0,-2.0]
        }
    },
    "artist_evidence":{
        "binary":"BURNOUT_X360_ARTIST.XEX",
        "source_database":"D:\\Reverse\\BP-Decomp_Workflow\\IDA Files\\BURNOUT_X360_ARTIST.XEX.i64",
        "source_database_sha256":"b563ffee0368e6ba3cec7a17ed07a643b86441588a66361d09ccfe0ac2738d40",
        "database_copy_on_D_was_hash_identical_before_IDA":"b563ffee0368e6ba3cec7a17ed07a643b86441588a66361d09ccfe0ac2738d40",
        "static_initializer_json":str(init.resolve()),
        "static_initializer_json_sha256":sha(init),
        "constant_word_dump":str(constants.resolve()),
        "constant_word_dump_sha256":sha(constants),
        "range_start":"0x82C6C3E8",
        "range_end_exclusive":"0x82C6C494",
        "function_name":init_json["function_name"],
        "object_address":"0x830112D0",
        "member_order_from_header":[
            {"member":"mOriginXYZ","offset":"+0x00"},
            {"member":"mRightUp","offset":"+0x10"},
            {"member":"mColourShift","offset":"+0x20"},
            {"member":"mColourScale","offset":"+0x30"}
        ],
        "native_stores":[
            {"address":"0x82C6C454","instruction":"addi r11, r11, flt_830112D0@l","effect":"r11 = 0x830112D0"},
            {"address":"0x82C6C464","instruction":"stvx128 v0, r0, r11","target":"0x830112D0 (+0x00)"},
            {"address":"0x82C6C470","instruction":"stvx128 v0, r11, r10","target":"0x830112D0 (+0x10), r10=0x10"},
            {"address":"0x82C6C480","instruction":"stvx128 v0, r11, r10","target":"0x830112D0 (+0x20), r10=0x20"},
            {"address":"0x82C6C490","instruction":"stvx128 v0, r11, r10","target":"0x830112D0 (+0x30), r10=0x30"}
        ],
        "stack_vectors_before_stvx128":[
            {"stack_base":"r1-0x40","lanes":[-1.0,1.0,0.0,0.0],"stores":["0x82C6C3F4","0x82C6C400","0x82C6C418","0x82C6C41C"]},
            {"stack_base":"r1-0x30","lanes":[2.0,0.0,0.0,-2.0],"stores":["0x82C6C434","0x82C6C424","0x82C6C428","0x82C6C44C"]},
            {"stack_base":"r1-0x20","lanes":[0.0,0.0,0.0,0.0],"stores":["0x82C6C43C","0x82C6C440","0x82C6C458","0x82C6C45C"]},
            {"stack_base":"r1-0x10","lanes":[1.0,1.0,1.0,1.0],"stores":["0x82C6C404","0x82C6C408","0x82C6C40C","0x82C6C460"]}
        ],
        "constant_words":[
            {"address":"0x820037C8","word":"0xBF800000","f32":-1.0},
            {"address":"0x82001C98","word":"0x3F800000","f32":1.0},
            {"address":"0x82001CC0","word":"0x00000000","f32":0.0},
            {"address":"0x82001D9C","word":"0x40000000","f32":2.0},
            {"address":"0x82006D70","word":"0xC0000000","f32":-2.0}
        ],
        "image_static_bytes_before_initializer":"all-zero 64-byte range at 0x830112D0 in IDA copy; constructor stores above establish defaults"
    },
    "native_compile_gate":{
        "artifact_directory":str(gate.resolve()),
        "source_count":report["requested"],
        "passed":report["passed"],
        "failed":report["failed"],
        "returncode":bind["driver_returncode"],
        "jobs":1,
        "target":"xbox360-ppc-be",
        "sdk":bind["sdk"],
        "machine":"0x01F2",
        "object":object_binding,
        "compile_report":str(reportpath.resolve()),
        "compile_report_sha256":sha(reportpath),
        "source_object_header_binding":str(bindpath.resolve()),
        "source_object_header_binding_sha256":sha(bindpath),
        "actual_include_count":bind["actual_header_count"],
        "preflight_bound_include_count":bind["preflight_bound_header_count"],
        "actual_includes_all_preflight_bound_and_unchanged":bind["preflight_bound_headers_unchanged"],
        "headers":header_rows
    },
    "preflight":{"path":str((art/"successor-preflight.json").resolve()),
                  "sha256":sha(art/"successor-preflight.json"),
                  "header_count":pre["header_count"],
                  "prior_gate_header_binding_sha256":pre["prior_header_binding_sha256"]},
    "verification_limits":["One translation unit compile-only; no link, image, or runtime launch was performed.",
                            "No changes were made outside CgsIm2dTransform.cpp.",
                            "The four R7-passing debug TUs remain untouched; the earlier five-source receipt is preserved separately and its Im2dTransform object is superseded by this corrected object."]
}
out=art/"DELIVERY.json"
out.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"delivery":str(out),"sha256":sha(out),"commit":commit,"parent":parent,
                  "source_blob":receipt["source_change"]["committed_blob"],
                  "compiled":f"{report['passed']}/{report['requested']}",
                  "machine":"0x01F2","headers":bind["actual_header_count"],
                  "header_binding_sha256":sha(bindpath)},indent=2))