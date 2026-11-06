from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_gui_lanes_luna_129a")
ARTIFACT = Path(__file__).resolve().parent
GATE = ARTIFACT / "native-5file-gate"
R7 = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7\full-stage1")
R6 = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r6\full-stage1")
ARTIST = Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX")
SOURCES = [
    "src/GameShared/GameClasses/Gui/CgsGuiShared.cpp",
    "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.cpp",
    "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp",
    "src/GameShared/GameClasses/Gui/View/CgsParticleSystem2d.cpp",
    "src/GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.cpp",
]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def rows_by_source(report):
    return {row["source"]: row for row in report["results"]}


def baseline(stage, label):
    report_path = stage / "compile-results.json"
    command_path = stage.parent / "full-stage1-command.json"
    report = read_json(report_path)
    command = read_json(command_path)
    rows = rows_by_source(report)
    selected = []
    for source in SOURCES:
        row = rows[source]
        log_path = Path(row["log"])
        selected.append({
            "source": source,
            "passed": row["passed"],
            "returncode": row["returncode"],
            "log": str(log_path),
            "log_sha256": sha(log_path),
            "errors": [line for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines()
                       if "error " in line],
        })
    return {
        "label": label,
        "head": command["head"],
        "report_state": report["state"],
        "requested": report["requested"],
        "passed": report.get("passed"),
        "failed": report.get("failed"),
        "compile_results": str(report_path),
        "compile_results_sha256": sha(report_path),
        "selected_rows": selected,
    }


def main():
    commit = git("rev-parse", "HEAD")
    parent = git("rev-parse", "HEAD^")
    branch = git("branch", "--show-current")
    preflight = read_json(ARTIFACT / "preflight-input-snapshot.json")
    vendor_snapshot = read_json(ARTIFACT / "canonical-vendor-input-snapshot.json")
    report_path = GATE / "compile-results.json"
    report = read_json(report_path)
    gate_command = read_json(GATE / "command.json")
    binding_path = GATE / "source-object-binding.json"
    binding = read_json(binding_path)
    result_rows = rows_by_source(report)

    source_delivery = []
    for source in SOURCES:
        pre_blob = git("rev-parse", parent + ":" + source)
        post_blob = git("rev-parse", commit + ":" + source)
        post_sha = sha(ROOT / source)
        if post_sha != binding["source_postflight_sha256"][source]:
            raise SystemExit("committed source differs from the compiled postflight: " + source)
        source_delivery.append({
            "path": source,
            "preimage_blob": pre_blob,
            "preimage_sha256": preflight["source_sha256"][source],
            "committed_blob": post_blob,
            "committed_sha256": post_sha,
            "compile_passed": result_rows[source]["passed"],
            "machine_hex": binding["objects"][source]["machine_hex"],
            "object_sha256": binding["objects"][source]["sha256"],
        })

    object_rows = []
    for source in SOURCES:
        row = binding["objects"][source]
        if not row["exists"] or row["machine"] != 0x01F2:
            raise SystemExit("missing/non-PPC32BE native object for " + source)
        object_path = Path(row["path"])
        machine = int.from_bytes(object_path.read_bytes()[:2], "little")
        if machine != 0x01F2 or sha(object_path) != row["sha256"]:
            raise SystemExit("native COFF witness changed for " + source)
        object_rows.append({"source": source, **row})

    report_rows = []
    for source in SOURCES:
        row = result_rows[source]
        report_rows.append({
            "source": source,
            "passed": row["passed"],
            "returncode": row["returncode"],
            "machine": row["machine"],
            "log": row["log"],
            "log_sha256": sha(row["log"]),
        })

    current_status = subprocess.check_output(
        ["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()
    if current_status:
        raise SystemExit("private worktree is not clean after the owned commit")

    delivery = {
        "schema": "native-x360-experiment-delivery/v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "task": "Native GUI vector lane C++03 fixes",
        "worktree": str(ROOT),
        "branch": branch,
        "parent": parent,
        "commit": commit,
        "commit_message": git("show", "-s", "--format=%s", commit),
        "co_author_trailer": "Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>",
        "published": False,
        "owned_source_files": source_delivery,
        "prior_failure_evidence": {
            "r7_partial": baseline(R7, "wave8 repair-r7 partial full-stage1 at frozen R7 head"),
            "r6": baseline(R6, "wave8 repair-r6 full-stage1"),
        },
        "native_gate": {
            "kind": "one compile-only gate for exactly five assigned sources",
            "target": report.get("target"),
            "sdk": report.get("sdk"),
            "jobs": 1,
            "show_includes": True,
            "compiler_flags": "tools/build_x360.py defaults unchanged; /showIncludes added; canonical vendor fallback include roots used",
            "runner": str(ARTIFACT / "run_gate.py"),
            "runner_sha256": sha(ARTIFACT / "run_gate.py"),
            "command": str(GATE / "command.json"),
            "command_sha256": sha(GATE / "command.json"),
            "compile_results": str(report_path),
            "compile_results_sha256": sha(report_path),
            "driver_log": str(GATE / "driver.log"),
            "driver_log_sha256": sha(GATE / "driver.log"),
            "passed": report["passed"],
            "failed": report["failed"],
            "rows": report_rows,
            "native_coff_objects": object_rows,
            "source_object_binding": str(binding_path),
            "source_object_binding_sha256": sha(binding_path),
            "headers": {
                **binding["header_preflight_comparison"],
                "postflight_hashes_file": str(binding_path),
                "preflight_private_header_snapshot": str(ARTIFACT / "preflight-input-snapshot.json"),
                "preflight_private_header_snapshot_sha256": sha(ARTIFACT / "preflight-input-snapshot.json"),
                "canonical_vendor_header_snapshot": str(ARTIFACT / "canonical-vendor-input-snapshot.json"),
                "canonical_vendor_header_snapshot_sha256": sha(ARTIFACT / "canonical-vendor-input-snapshot.json"),
            },
        },
        "behavior_checks": [
            {
                "source": "ARTIST 0x82851A90 CgsGui::ObjectController::GetPos",
                "finding": "Guest reads Y then X, stores X/Y, zeroes the upper eight bytes and copies the full 16-byte Vector2. Native Vector2(float,float) emits the same two lane stores and zero high lanes.",
                "export": str(ARTIST / "0x82851A90.json"),
            },
            {
                "source": "ARTIST 0x824507B8 CgsGui::ParticleSystem2d::GetRGBA",
                "finding": "The unchanged clamp and all-lane scale feed four lane scalar extractions and truncating conversions; the edit uses the SDK X/Y/Z/W references to extract those same lanes.",
                "export": str(ARTIST / "0x824507B8.json"),
            },
            {
                "source": "native VPU constructors and setters",
                "finding": "Vector3(float,float,float) initializes W to zero; Vector4::Set writes all four lanes; Vector2::mV exposes the real XDK __vector4 lanes. No vector headers or scalar compatibility members were added.",
                "headers": [
                    "src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h",
                    "src/SDKs/EATech/include/rw/math/vpu/detail/vector4_type_inline.h",
                    "vendor/renderware/include/rw/math/vpu/types.h",
                    "build/x360/sdk-6534.4/portable/XDK/include/xbox/vectorintrinsics.h",
                ],
            },
        ],
        "scope_and_limits": {
            "changed_files": SOURCES,
            "headers_changed": False,
            "other_translation_units_changed": False,
            "whole_program_link_attempted": False,
            "runtime_validation_attempted": False,
            "status": "The assigned five sources compile for XDK PPC32 big-endian; whole-project link/runtime remain outside this bounded gate.",
        },
    }
    output = ARTIFACT / "DELIVERY.json"
    output.write_text(json.dumps(delivery, indent=2) + "\n", encoding="utf-8")
    print(str(output))
    print("commit=%s parent=%s pass=%d fail=%d objects=%d headers=%d" % (
        commit, parent, report["passed"], report["failed"], len(object_rows),
        binding["header_preflight_comparison"]["unique_actual_headers"]))


if __name__ == "__main__":
    main()
