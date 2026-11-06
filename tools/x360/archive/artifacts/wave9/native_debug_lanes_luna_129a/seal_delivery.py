from pathlib import Path
import hashlib, json, subprocess

repo = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
art = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a")
parent = "129afef95856e383366a55fb8f30a66ceb74de42"
source_paths = [
    "src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d_wBT_01.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dUntex.cpp",
]
prior_pass_paths = [
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsErrorWindow.cpp",
    "src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.cpp",
]
all_candidates = prior_pass_paths + source_paths

def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()
def git(*args):
    return subprocess.check_output(["git", *args], cwd=repo, text=True).strip()
head = git("rev-parse", "HEAD")
commit = head
commit_parent = git("rev-parse", "HEAD^")
branch = git("branch", "--show-current")
status = git("status", "--porcelain")
if commit_parent != parent or status:
    raise SystemExit(f"receipt requires committed clean branch at parent {parent}: commit={commit}, parent={commit_parent}, status={status!r}")
pre = json.loads((art / "preflight-input-binding.json").read_text(encoding="utf-8"))
final_dir = art / "five-failure-native-gate-final"
final_binding_path = final_dir / "source-object-header-binding.json"
final_bind = json.loads(final_binding_path.read_text(encoding="utf-8"))
final_report = json.loads((final_dir / "compile-results.json").read_text(encoding="utf-8"))
first_dir = art / "five-failure-native-gate"
first_bind = json.loads((first_dir / "source-object-header-binding.json").read_text(encoding="utf-8"))
first_report = json.loads((first_dir / "compile-results.json").read_text(encoding="utf-8"))
r7_dir = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\full-stage1")
r7_report_path = r7_dir / "compile-results.json"
r7_report = json.loads(r7_report_path.read_text(encoding="utf-8"))
def build_row(row):
    log = Path(row["log"])
    logtext = log.read_text(encoding="utf-8", errors="replace")
    errs = [line for line in logtext.splitlines() if "error " in line]
    obj = Path(row["object"])
    return {"source": row["source"], "passed": row["passed"], "returncode": row["returncode"],
            "machine": row.get("machine"), "diagnostic_count": len(errs),
            "log": str(log.resolve()), "log_sha256": sha(log),
            "object": str(obj.resolve()) if obj.is_file() else None,
            "object_sha256": sha(obj) if obj.is_file() else None}
r7_rows = []
for rel in all_candidates:
    name = Path(rel).name
    row = next((r for r in r7_report["results"] if r["source"].endswith("/" + name)), None)
    if not row:
        raise SystemExit(f"missing exact R7 compiler row: {rel}")
    r7_rows.append({"path": rel, **build_row(row)})
source_rows = []
final_by_source = {r["source"]: r for r in final_bind["results"]}
for rel in source_paths:
    row = next((r for r in final_report["results"] if r["source"] == rel), None)
    bound = final_by_source.get(rel)
    if not row or not bound or not row["passed"]:
        raise SystemExit(f"missing final pass binding: {rel}")
    source_rows.append({
        "path": rel,
        "parent_blob": pre["sources"][rel]["git_blob"],
        "preflight_sha256": pre["sources"][rel]["sha256"],
        "commit_blob": git("rev-parse", f"{commit}:{rel}"),
        "committed_sha256": sha(repo / rel),
        "compile": {"passed": row["passed"], "returncode": row["returncode"],
                    "machine": row["machine"], "machine_hex": "0x01F2",
                    "diagnostic_count": bound["diagnostic_count"],
                    "object": bound["object"]["path"], "object_size": bound["object"]["size"],
                    "object_sha256": bound["object"]["sha256"],
                    "log": bound["log"], "log_sha256": bound["log_sha256"]},
    })
unchanged_rows = []
for rel in prior_pass_paths:
    if sha(repo / rel) != pre["sources"][rel]["sha256"]:
        raise SystemExit(f"prior R7 pass source changed: {rel}")
    unchanged_rows.append({"path": rel, "source_sha256": sha(repo / rel),
                           "r7_pass": next(r for r in r7_rows if r["path"] == rel)})
first_im3d = next((r for r in first_bind["results"] if r["source"].endswith("/CgsIm3d.cpp")), None)
if not first_im3d:
    raise SystemExit("missing first Im3d diagnostics")
first_im3d_log = Path(first_im3d["log"])
first_im3d_text = first_im3d_log.read_text(encoding="utf-8", errors="replace")
first_im3d_errors = [line for line in first_im3d_text.splitlines() if "error " in line]
headers = final_bind["actual_headers"]
receipt = {
    "schema": "native-x360-debug-lanes-delivery-v1",
    "objective": "Remove native PPC32/BE C++03 Vector2/3/4 aggregate/member spellings that failed the frozen R7 source gate, without changing the four R7-passing owned files.",
    "repository": "b5-decomp",
    "private_worktree": str(repo),
    "branch": branch,
    "commit": commit,
    "parent": commit_parent,
    "co_author_trailer": "Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>",
    "published": False,
    "private_commit_clean": True,
    "scope": {
        "committed_sources": source_rows,
        "unchanged_previously_passing_sources": [
            {"path": r["path"], "source_sha256": r["source_sha256"],
             "r7_pass": {"machine": r["r7_pass"]["machine"],
                         "object_sha256": r["r7_pass"]["object_sha256"],
                         "log_sha256": r["r7_pass"]["log_sha256"],
                         "log": r["r7_pass"]["log"]}}
            for r in unchanged_rows
        ],
        "headers_changed": False,
        "files_changed": 5,
        "no_PC_compatibility_facades": True,
        "no_link_or_launch_claim": True,
    },
    "selection_evidence": {
        "r7_artifact": str(r7_report_path),
        "r7_report_sha256": sha(r7_report_path),
        "rows": [{"path": r["path"], "passed": r["passed"], "machine": r["machine"],
                  "diagnostic_count": r["diagnostic_count"], "log": r["log"],
                  "log_sha256": r["log_sha256"], "object_sha256": r["object_sha256"]}
                 for r in r7_rows],
    },
    "preflight": {
        "head": pre["head"],
        "manifest": str(art / "preflight-input-binding.json"),
        "manifest_sha256": sha(art / "preflight-input-binding.json"),
        "candidate_cpp_count": pre["source_count"],
        "header_count": pre["header_count"],
        "headers_sha256_bound": True,
    },
    "native_compile_gate": {
        "artifact_directory": str(final_dir),
        "command_file": str(final_dir / "command.json"),
        "compile_report": str(final_dir / "compile-results.json"),
        "compile_report_sha256": sha(final_dir / "compile-results.json"),
        "source_object_header_binding": str(final_binding_path),
        "source_object_header_binding_sha256": sha(final_binding_path),
        "driver_log": str(final_dir / "driver.log"),
        "driver_log_sha256": sha(final_dir / "driver.log"),
        "sdk": final_bind["sdk"],
        "jobs": 1,
        "show_includes": True,
        "canonical_vendor_fallbacks": ["EABase/include/Common", "EASTL/include", "EAThread/include",
                                        "renderware/include", "PPMalloc/include", "coreallocator/include",
                                        "dirtysdk/include", "zlib/include", "lua/src"],
        "requested": final_report["requested"],
        "passed": final_report["passed"],
        "failed": final_report["failed"],
        "returncode": final_bind["driver_returncode"],
        "actual_header_count": final_bind["actual_header_count"],
        "preflight_bound_header_count": final_bind["preflight_bound_header_count"],
        "postflight_only_headers": [{"path": h["path"], "sha256": h["sha256"]}
                                     for h in headers if not h["preflight_bound"]],
        "preflight_bound_headers_unchanged": final_bind["headers_unchanged"],
        "native_coff_machine": "0x01F2",
        "native_objects": final_bind["native_objects"],
    },
    "first_attempt_preserved": {
        "artifact_directory": str(first_dir),
        "requested": first_report["requested"],
        "passed": first_report["passed"],
        "failed": first_report["failed"],
        "failed_translation_unit": "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp",
        "log": str(first_im3d_log.resolve()),
        "log_sha256": sha(first_im3d_log),
        "diagnostics": first_im3d_errors,
        "resolution": "The instantiated Render template also read Vector3 x/y/z fields. Replaced only those source reads with GetX/Y/Z().GetFloat(); the destination Vector3F POD fields and three scalar stores remain unchanged.",
    },
    "verification_limits": [
        "Compile-only native gate; no whole-program link, image build, or runtime launch was performed.",
        "This receipt binds the five failing source objects and actual includes; the four earlier R7 passing objects remain bound to their R7 logs and were left unchanged.",
        "Nine extensionless XDK C wrapper headers were postflight-hashed but were outside the preflight header suffix scan; their exact hashes are included in the header binding artifact.",
    ],
}
out = art / "DELIVERY.json"
out.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"delivery": str(out), "sha256": sha(out), "commit": commit,
                  "parent": commit_parent, "sources": len(source_rows),
                  "gate_passed": final_report["passed"], "gate_failed": final_report["failed"],
                  "actual_headers": final_bind["actual_header_count"],
                  "preflight_bound": final_bind["preflight_bound_header_count"],
                  "postflight_only": len(receipt["native_compile_gate"]["postflight_only_headers"])}, indent=2))