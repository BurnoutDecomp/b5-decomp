import hashlib
import json
import os
from pathlib import Path
import subprocess

repo = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\effects_lanes_luna_4277")
artifacts = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave10\geometry_native_luna")
base = "e572f7db4c533546832e42c6f3f73149071b5f82"
expected_sources = [
    "src/GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests_LineNearest.cpp",
    "src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap_Build.cpp",
    "src/GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupPoly.cpp",
    "src/GameShared/GameClasses/Graphics/CgsCamera.cpp",
    "src/GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.cpp",
]
final_report_path = artifacts / "pass-006" / "compile-results.json"

def git(*args):
    return subprocess.check_output(["git", *args], cwd=str(repo), text=True).strip()

def sha_bytes(value):
    return hashlib.sha256(value).hexdigest()

def sha_file(path):
    path = Path(path)
    if not path.is_file():
        return None
    return sha_bytes(path.read_bytes())

def report_summary(report_path):
    report = json.loads(Path(report_path).read_text(encoding="utf-8"))
    return {
        "report": str(report_path),
        "sha256": sha_file(report_path),
        "passed": sum(1 for item in report["results"] if item["passed"]),
        "failed": sum(1 for item in report["results"] if not item["passed"]),
        "superseded": True,
    }

commit = git("rev-parse", "HEAD")
tree = git("rev-parse", "HEAD^{tree}")
branch = git("branch", "--show-current")
committed_paths = git("diff-tree", "--no-commit-id", "--name-only", "-r", "HEAD").splitlines()
base_delta_paths = git("diff", "--name-only", base, commit).splitlines()
if sorted(committed_paths) != sorted(expected_sources) or sorted(base_delta_paths) != sorted(expected_sources):
    raise RuntimeError("Committed path set differs from the five assigned CPPs")
if git("status", "--porcelain"):
    raise RuntimeError("Working tree is not clean after source-only commit")

report = json.loads(final_report_path.read_text(encoding="utf-8"))
if report.get("state") != "complete" or report.get("target") != "xbox360-ppc-be":
    raise RuntimeError("Final native compile report is incomplete or targets the wrong machine")
if len(report.get("results", [])) != 5 or any(not item.get("passed") or item.get("machine") != 0x01F2 for item in report["results"]):
    raise RuntimeError("Final native compile report does not show five PPC COFF passes")
if [item["source"] for item in report["results"]] != expected_sources:
    raise RuntimeError("Final native compile source order differs from the assigned set")

source_changes = []
for rel in expected_sources:
    before = subprocess.check_output(["git", "show", f"{base}:{rel}"], cwd=str(repo))
    source_changes.append({
        "path": rel,
        "sha256_before": sha_bytes(before),
        "sha256_after": sha_file(repo / rel),
    })

compile_sources = []
include_files = {}
missing_includes = []
for item in report["results"]:
    source = item["source"]
    compile_sources.append({
        "source": source,
        "passed": bool(item["passed"]),
        "returncode": int(item["returncode"]),
        "machine": f"0x{int(item['machine']):04X}",
        "object": {"path": item["object"], "sha256": sha_file(item["object"])},
        "assembly": {"path": item["assembly"], "sha256": sha_file(item["assembly"])},
        "log": {"path": item["log"], "sha256": sha_file(item["log"])},
        "command": item["command"],
    })
    for line in Path(item["log"]).read_text(encoding="utf-8", errors="replace").splitlines():
        marker = "Note: including file:"
        if marker not in line:
            continue
        resolved = line.split(marker, 1)[1].strip()
        if not resolved:
            continue
        normalized = os.path.normcase(resolved)
        entry = include_files.setdefault(normalized, {
            "path": resolved,
            "sha256": sha_file(resolved),
            "resolved_by": [],
        })
        if entry["sha256"] is None:
            missing_includes.append(resolved)
        if source not in entry["resolved_by"]:
            entry["resolved_by"].append(source)

if missing_includes:
    raise RuntimeError("Unhashed /showIncludes paths: " + ", ".join(sorted(set(missing_includes))))

compiler_path = report["results"][0]["command"][0]
fallback_prefix = "/IC:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor/"
include_roots = [arg[2:] for arg in report["results"][0]["command"] if arg.startswith("/I")]
fallback_roots = [arg[2:] for arg in report["results"][0]["command"] if arg.startswith(fallback_prefix)]
if len(fallback_roots) != 9:
    raise RuntimeError(f"Expected nine canonical fallback vendor roots, found {len(fallback_roots)}")

prior_attempts = []
prior_notes = {
    "pass-001": "superseded after the initial two native API/type failures were fixed",
    "pass-002": "superseded after the remaining SpatialMap Vector3 refit types were fixed",
    "pass-003": "superseded before the X360 assertion restoration and fallback include-root pass",
    "pass-004": "superseded because it omitted the nine canonical fallback vendor roots",
    "pass-005": "superseded by the final source-comment refresh and final native rerun",
}
for pass_name, note in prior_notes.items():
    pass_report = artifacts / pass_name / "compile-results.json"
    prior_attempts.append({
        **report_summary(pass_report),
        "pass": pass_name,
        "note": note,
    })

payload = {
    "task": "wave10/geometry_native_luna",
    "branch": branch,
    "base_commit": base,
    "final_commit": commit,
    "final_tree": tree,
    "model_coauthor": "Codex GPT-6 Luna <noreply@openai.com>",
    "changed_paths": committed_paths,
    "source_hashes": source_changes,
    "native_compile": {
        "state": report["state"],
        "target": report["target"],
        "sdk": report["sdk"],
        "compiler": {"path": compiler_path, "sha256": sha_file(compiler_path)},
        "environment": {
            "jobs": 1,
            "TEMP": str(artifacts / "temp-006"),
            "TMP": str(artifacts / "tmp-006"),
        },
        "fallback_vendor_include_roots": fallback_roots,
        "include_roots": include_roots,
        "report": {"path": str(final_report_path), "sha256": sha_file(final_report_path)},
        "sources": compile_sources,
        "resolved_includes": sorted(include_files.values(), key=lambda entry: entry["path"].casefold()),
    },
    "superseded_attempts": prior_attempts,
    "verification": {
        "whitespace_diff_check": "pass",
        "worktree_after_commit": "clean",
        "link": "not run",
        "runtime": "not run",
        "byte_match": "not claimed",
    },
    "limitations": [
        "Native compile-only evidence; no link or game runtime claim.",
        "This repair preserves native VMX vector operations and payload lanes but does not claim byte identity.",
    ],
}
output_path = artifacts / "DELIVERY.json"
output_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(json.dumps({
    "delivery": str(output_path),
    "sha256": sha_file(output_path),
    "final_commit": commit,
    "final_tree": tree,
    "sources": len(source_changes),
    "resolved_include_files": len(include_files),
    "native_passed": report["passed"],
    "native_failed": report["failed"],
    "fallback_vendor_roots": len(fallback_roots),
    "superseded_attempts": len(prior_attempts),
}, indent=2))
