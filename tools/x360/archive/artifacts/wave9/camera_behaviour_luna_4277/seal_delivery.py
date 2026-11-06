from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_behaviour_luna_4277")
CANON = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
WORKFLOW = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow")
ARTIFACT = Path(__file__).resolve().parent
R7_ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r7")
ARTIST_ROOT = Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX")
SOURCES = [
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp",
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.cpp",
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCamera2DRotationController.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.cpp",
]
ARTIST_ADDRESSES = {
    "Vector2::Vector2(float,float)": "0x821F0DB0",
    "BehaviourFixedCam::Update": "0x82229DE0",
    "BehaviourHeliCam::Update": "0x82244568",
    "BehaviourLooseAttachment::Update": "0x82246738",
    "Camera2DRotationController::Update": "0x8220BFE0",
    "CameraSphericalRotationController::Update": "0x8223F808",
}
VECTOR3_INLINE = ROOT / "src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h"
VECTOR2_TYPES = ROOT / "vendor/renderware/include/rw/math/vpu/types.h"
VEC_FLOAT_INLINE = ROOT / "src/SDKs/EATech/include/rw/math/vpu/detail/vec_float_type_inline.h"
IMPACT_HEADER = ROOT / "src/GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def report_rows(report):
    return {row["source"]: row for row in report["results"]}


def gate_receipt(stage_name):
    stage = ARTIFACT / stage_name
    report_path = stage / "compile-results.json"
    report = read_json(report_path)
    binding_path = stage / "source-object-binding.json"
    binding = read_json(binding_path)
    rows = report_rows(report)
    summarized = []
    for source in binding["sources"]:
        row = rows[source]
        result = binding["source_results"][source]
        summarized.append({
            "source": source,
            "passed": row["passed"],
            "returncode": row["returncode"],
            "machine": row.get("machine"),
            "machine_hex": binding["objects"][source].get("machine_hex"),
            "errors": result["errors"],
            "log": result["log"],
            "log_sha256": result["log_sha256"],
            "object": binding["objects"][source],
        })
    all_headers = {}
    for result in binding["source_results"].values():
        for item in result["included_headers"]:
            all_headers[item["path"].casefold()] = item
    changed_headers = [item["path"] for item in all_headers.values()
                       if item["preflight_status"] == "changed"]
    uncaptured_headers = [item["path"] for item in all_headers.values()
                          if item["preflight_status"] == "not_captured"]
    return {
        "stage": stage_name,
        "target": report.get("target"),
        "requested": report.get("requested"),
        "passed": report.get("passed"),
        "failed": report.get("failed"),
        "report": str(report_path),
        "report_sha256": sha(report_path),
        "command": str(stage / "command.json"),
        "command_sha256": sha(stage / "command.json"),
        "driver_log": str(stage / "driver.log"),
        "driver_log_sha256": sha(stage / "driver.log"),
        "binding": str(binding_path),
        "binding_sha256": sha(binding_path),
        "source_rows": summarized,
        "unique_actual_headers": len(all_headers),
        "preflight_headers_changed": changed_headers,
        "headers_without_preflight_hash": uncaptured_headers,
    }


def main():
    commit = git("rev-parse", "HEAD")
    parent = git("rev-parse", "HEAD^")
    branch = git("branch", "--show-current")
    reflog = subprocess.check_output(
        ["git", "reflog", "show", "--date=iso-strict", "--format=%gD %H %gs", "-n", "8"],
        cwd=ROOT, text=True).splitlines()
    commit_reflog_entry = next((entry for entry in reflog if commit in entry), None)
    commit_identity = git("show", "-s", "--format=%an <%ae>|%aI|%cn <%ce>|%cI", commit)
    if parent != "42772cbe0386462d1c32c8442e9f58c9e534e75b":
        raise SystemExit("commit parent is not the frozen R8 input")
    if branch != "codex/experimental-x360-camera-behaviour-luna":
        raise SystemExit("unexpected private branch")

    initial = gate_receipt("initial-gate")
    post = gate_receipt("post-gate")
    snapshot_path = ARTIFACT / "preflight-input-snapshot.json"
    snapshot = read_json(snapshot_path)
    if snapshot["head"] != parent or initial["passed"] != 0 or initial["failed"] != 5:
        raise SystemExit("initial R8 gate did not match the five failing sources")
    if post["passed"] != 5 or post["failed"] != 0:
        raise SystemExit("post gate did not pass all five repaired sources")

    status = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()
    if status:
        raise SystemExit("camera private worktree is not clean after commit")
    committed_paths = subprocess.check_output(
        ["git", "diff-tree", "--no-commit-id", "--name-only", "-r", commit], cwd=ROOT, text=True
    ).splitlines()
    if sorted(committed_paths) != sorted(SOURCES):
        raise SystemExit("camera commit changed files outside the assigned five")

    sources = []
    for source in SOURCES:
        pre = snapshot["sources"][source]
        post_sha = sha(ROOT / source)
        post_blob = git("rev-parse", commit + ":" + source)
        if post_sha != read_json(ARTIFACT / "post-gate" / "source-object-binding.json")["source_postflight_sha256"][source]:
            raise SystemExit("committed file differs from the native postflight: " + source)
        sources.append({
            "path": source,
            "preimage_blob": pre["blob"],
            "preimage_blob_sha256": pre["blob_bytes_sha256"],
            "preimage_worktree_sha256": pre["worktree_preflight_sha256"],
            "committed_blob": post_blob,
            "committed_worktree_sha256": post_sha,
            "post_compile_passed": True,
        })

    r7_report_path = R7_ROOT / "full-recovered/compile-results.json"
    r7_report = read_json(r7_report_path)
    r7_command_path = R7_ROOT / "full-stage1-command.json"
    r7_head = read_json(r7_command_path)["head"]
    r7_rows = report_rows(r7_report)
    r7_selected = []
    for source in SOURCES:
        row = r7_rows[source]
        log = Path(row["log"])
        errors = [line for line in log.read_text(encoding="utf-8", errors="replace").splitlines()
                  if "error " in line]
        r7_selected.append({
            "source": source,
            "passed": row["passed"],
            "error_count": len(errors),
            "errors": errors,
            "log": str(log),
            "log_sha256": sha(log),
        })
    if r7_head != "129afef95856e383366a55fb8f30a66ceb74de42":
        raise SystemExit("R7 full-stage1 head differs from the recorded frozen head")
    unchanged = subprocess.run(
        ["git", "diff", "--quiet", r7_head, parent, "--", *SOURCES], cwd=ROOT).returncode == 0
    if not unchanged or any(row["passed"] for row in r7_selected):
        raise SystemExit("R7 failures/source preimages differ from the assigned preflight")

    artist = {}
    for name, address in ARTIST_ADDRESSES.items():
        path = ARTIST_ROOT / (address + ".json")
        if not path.is_file():
            raise SystemExit("missing ARTIST export: " + str(path))
        obj = read_json(path)
        artist[name] = {"address": address, "name": obj.get("name"), "export": str(path), "sha256": sha(path)}

    vector2_ctor = read_json(ARTIST_ROOT / "0x821F0DB0.json")
    vector2_ctor_lines = [line.strip() for line in vector2_ctor["assembly"].splitlines()
                          if line.strip().startswith(("0x821F0DB0", "0x821F0DB4", "0x821F0DB8",
                                                      "0x821F0DBC", "0x821F0DC0", "0x821F0DC4",
                                                      "0x821F0DC8", "0x821F0DCC"))]

    initial_binding_path = ARTIFACT / "initial-gate/source-object-binding.json"
    post_binding_path = ARTIFACT / "post-gate/source-object-binding.json"
    initial_binding = read_json(initial_binding_path)
    post_binding = read_json(post_binding_path)
    prior_diff_verified = True

    delivery = {
        "schema": "native-x360-experiment-delivery/v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "task": "Native camera vector and matrix API spelling repairs",
        "worktree": str(ROOT),
        "branch": branch,
        "parent": parent,
        "commit": commit,
        "commit_message": git("show", "-s", "--format=%s", commit),
        "co_author_trailer": "Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>",
        "commit_observation": {
            "author_and_times": commit_identity,
            "reflog_entry": commit_reflog_entry,
            "note": "The exact five-file commit was already at HEAD during commit verification. Its committed source hashes match the native postflight bindings; no duplicate or amendment was made. The reflog timestamp is recorded without inferring which process initiated it.",
        },
        "published": False,
        "owned_sources": sources,
        "r7_current_full_recovered": {
            "head": r7_head,
            "report": str(r7_report_path),
            "report_state": r7_report.get("state"),
            "requested": r7_report.get("requested"),
            "passed": r7_report.get("passed"),
            "failed": r7_report.get("failed"),
            "report_sha256": sha(r7_report_path),
            "command": str(r7_command_path),
            "command_sha256": sha(r7_command_path),
            "assigned_rows": r7_selected,
            "unchanged_through_r8": prior_diff_verified,
        },
        "compile_gates": {
            "sdk": str(CANON / "build/x360/sdk-6534.4/portable/XDK"),
            "jobs": 1,
            "show_includes": True,
            "compiler_flags": "tools/build_x360.py defaults unchanged; /showIncludes added; canonical vendor fallback include roots used",
            "preflight_snapshot": str(snapshot_path),
            "preflight_snapshot_sha256": sha(snapshot_path),
            "initial": initial,
            "post": post,
            "initial_binding": str(initial_binding_path),
            "initial_binding_sha256": sha(initial_binding_path),
            "post_binding": str(post_binding_path),
            "post_binding_sha256": sha(post_binding_path),
            "post_header_binding": {
                "actual_headers": post_binding["actual_header_count"],
                "preflight_headers": post_binding["preflight_header_count"],
                "changed": sum(1 for value in post_binding["source_results"].values()
                               for item in value["included_headers"] if item["preflight_status"] == "changed"),
                "uncaptured": sum(1 for value in post_binding["source_results"].values()
                                  for item in value["included_headers"] if item["preflight_status"] == "not_captured"),
            },
        },
        "native_type_and_assembly_evidence": {
            "qualified_existing_types": [
                "rw::math::vpu::Vector3",
                "rw::math::vpu::Matrix44Affine",
                "rw::math::vpu::Vector2",
            ],
            "native_Vector2_constructor": {
                "address": vector2_ctor["address"],
                "name": vector2_ctor["name"],
                "export": str(ARTIST_ROOT / "0x821F0DB0.json"),
                "export_sha256": sha(ARTIST_ROOT / "0x821F0DB0.json"),
                "assembly": vector2_ctor_lines,
                "effect": "Stores X/Y and zeros the upper eight bytes; the constructor replaces the former four scalar member writes.",
            },
            "native_vector_apis": [
                {
                    "header": str(VECTOR3_INLINE),
                    "sha256": sha(VECTOR3_INLINE),
                    "construct_float3": "Vector3(float x,float y,float z) stores XYZ and explicitly clears mV.u[3].",
                    "set_y": "Vector3::SetY(float) delegates to SetComponent(1,y), writing native lane Y only.",
                },
                {
                    "header": str(VECTOR2_TYPES),
                    "sha256": sha(VECTOR2_TYPES),
                    "storage": "Vector2 owns the genuine __vector4 mV register.",
                    "construct_float2": "Vector2(x,y) stores X/Y and zeros mV.u[2]/mV.u[3].",
                    "set_zero": "Vector2::SetZero() uses __vspltisw(0) over all four lanes.",
                },
                {
                    "header": str(VEC_FLOAT_INLINE),
                    "sha256": sha(VEC_FLOAT_INLINE),
                    "y_lane_add": "VecFloatRef<Y>::operator+=(float) computes mV.v[Y] = mV.v[Y] + value.",
                },
                {
                    "header": str(IMPACT_HEADER),
                    "sha256": sha(IMPACT_HEADER),
                    "type": "The existing Utils::CameraImpactEffect::Parameters nested type used by the behavior block.",
                },
            ],
            "artist_functions": artist,
            "lane_handling": [
                "Vector3::SetY writes native lane Y only; the fixed and heli sources no longer depend on fake scalar fields.",
                "Vector2 input and held-stick reads use the native mV.v X/Y lanes; the two-float constructor zeroes Z/W, and SetZero clears all four lanes.",
                "LooseAttachment uses the native Vector3 Y lane reference for the height increment and an explicit existing CameraImpactEffect::Parameters type instead of C++03 auto.",
                "No arithmetic order was reorganized or fused; source transformations are limited to native type qualification, lane APIs, the actual constructor, and type spelling.",
            ],
        },
        "scope_and_limits": {
            "changed_files": committed_paths,
            "headers_changed": False,
            "other_translation_units_changed": False,
            "whole_project_link_attempted": False,
            "runtime_validation_attempted": False,
            "extra_fixtures_or_test_suites": False,
            "status": "The five assigned TUs compile for XDK PPC32 big-endian; this does not certify whole-project linking or runtime behavior.",
        },
    }
    output = ARTIFACT / "DELIVERY.json"
    output.write_text(json.dumps(delivery, indent=2) + "\n", encoding="utf-8")
    print(str(output))
    print("commit=%s parent=%s initial=%d/%d post=%d/%d objects=%d" % (
        commit, parent, initial["passed"], initial["failed"], post["passed"], post["failed"],
        len(post["source_rows"])))


if __name__ == "__main__":
    main()
