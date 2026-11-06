from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_gui_lanes_luna_129a")
WORKFLOW = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow")
ARTIFACT = Path(__file__).resolve().parent
GATE = ARTIFACT / "native-onefile-gate"
SOURCE = "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp"
IDENTITY_PATH = WORKFLOW / "progress/identity.json"
ARTIST_EXPORT = Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x82858988.json")
IM2D_HEADER = ROOT / "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
IM2D_SOURCE = ROOT / "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp"
PRIOR_DELIVERY = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_gui_lanes_luna_129a\DELIVERY.json")
ARTIST_INIT_IDB = Path(r"D:\Reverse\BP-Decomp_Workflow\IDA Files\BURNOUT_X360_ARTIST.XEX.i64")
ARTIST_INIT_PROOF = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\artist-static-init-proof\static_initializer.json")


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


def main():
    commit = git("rev-parse", "HEAD")
    parent = git("rev-parse", "HEAD^")
    branch = git("branch", "--show-current")
    preflight = read_json(ARTIFACT / "preflight-input-snapshot.json")
    binding_path = GATE / "source-object-binding.json"
    binding = read_json(binding_path)
    report_path = GATE / "compile-results.json"
    report = read_json(report_path)
    command_path = GATE / "command.json"
    command = read_json(command_path)

    identity = read_json(IDENTITY_PATH)
    identity_row = identity["CgsGui::ViewModule::RenderBlackScreen"]
    if "0x82858988" not in identity_row.get("x360_addrs", []):
        raise SystemExit("progress identity does not attest RenderBlackScreen at 0x82858988")
    artist = read_json(ARTIST_EXPORT)
    artist_lines = artist["assembly"].splitlines()
    evidence_addresses = ("0x828589C0", "0x828589C4", "0x828589C8", "0x828589CC")
    artist_api_stores = [line.strip() for line in artist_lines
                         if line.strip().startswith(evidence_addresses)]
    if len(artist_api_stores) != len(evidence_addresses):
        raise SystemExit("ARTIST SetTransform call site is incomplete")
    initializer = read_json(ARTIST_INIT_PROOF)
    initializer_addresses = {
        "0x82C6C448", "0x82C6C454", "0x82C6C464", "0x82C6C470",
        "0x82C6C480", "0x82C6C490",
    }
    initializer_instructions = [
        {"address": item["address"], "disassembly": item["disassembly"]}
        for item in initializer["instructions"] if item["address"] in initializer_addresses
    ]
    if len(initializer_instructions) != len(initializer_addresses):
        raise SystemExit("ARTIST default-transform initializer evidence is incomplete")

    if report.get("target") != "xbox360-ppc-be" or report.get("passed") != 1 or report.get("failed") != 0:
        raise SystemExit("one-file XDK gate did not pass")
    row = report["results"][0]
    if row["source"] != SOURCE or row["machine"] != 0x01F2 or not row["passed"]:
        raise SystemExit("native object report does not match the requested source/target")
    object_path = Path(row["object"])
    if int.from_bytes(object_path.read_bytes()[:2], "little") != 0x01F2:
        raise SystemExit("object header is not PPC32BE COFF machine 0x01F2")

    pre_blob = git("rev-parse", parent + ":" + SOURCE)
    post_blob = git("rev-parse", commit + ":" + SOURCE)
    post_sha = sha(ROOT / SOURCE)
    if post_sha != binding["source_postflight_sha256"]:
        raise SystemExit("committed source differs from compiled postflight")
    status = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()
    if status:
        raise SystemExit("private worktree is not clean after successor commit")
    changed = subprocess.check_output(
        ["git", "diff-tree", "--no-commit-id", "--name-only", "-r", commit], cwd=ROOT, text=True
    ).splitlines()
    if changed != [SOURCE]:
        raise SystemExit("successor commit escaped the one-file scope: " + repr(changed))

    generated_asm = Path(row["assembly"]).read_text(encoding="utf-8", errors="replace").splitlines()
    mgdefault_lines = [line.strip() for line in generated_asm if "mgDefault@Im2dTransform" in line]
    clear_start = next((index for index, line in enumerate(generated_asm)
                        if "?RenderBlackScreen@ViewModule" in line and "PROC NEAR" in line), None)
    clear_getdefault = []
    if clear_start is not None:
        for line in generated_asm[clear_start:clear_start + 100]:
            if "?RenderBlackScreen@ViewModule" in line and "ENDP" in line:
                break
            if "mgDefault@Im2dTransform" in line or "SetTransform" in line:
                clear_getdefault.append(line.strip())

    preimage_sha = preflight["source_preimage_sha256"]
    if preimage_sha != binding["source_preimage_sha256"]:
        raise SystemExit("source preimage hashes differ between snapshots")
    prior_delivery_data = read_json(PRIOR_DELIVERY)

    delivery = {
        "schema": "native-x360-experiment-delivery/v1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "task": "RenderBlackScreen shared native transform and host-trace removal",
        "worktree": str(ROOT),
        "branch": branch,
        "parent": parent,
        "commit": commit,
        "previous_successor_commit": prior_delivery_data["commit"],
        "commit_message": git("show", "-s", "--format=%s", commit),
        "co_author_trailer": "Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>",
        "published": False,
        "owned_source": {
            "path": SOURCE,
            "preimage_blob": pre_blob,
            "preimage_sha256": preimage_sha,
            "committed_blob": post_blob,
            "committed_sha256": post_sha,
        },
        "identity": {
            "file": str(IDENTITY_PATH),
            "sha256": sha(IDENTITY_PATH),
            "canonical": identity_row["canonical"],
            "x360_addrs": identity_row["x360_addrs"],
            "primary_file": identity_row["primary_file"],
        },
        "native_gate": {
            "kind": "compile-only for exactly CgsGuiViewModule.cpp",
            "target": report["target"],
            "sdk": report["sdk"],
            "jobs": 1,
            "show_includes": True,
            "passed": report["passed"],
            "failed": report["failed"],
            "runner": str(ARTIFACT / "run_gate.py"),
            "runner_sha256": sha(ARTIFACT / "run_gate.py"),
            "command": str(command_path),
            "command_sha256": sha(command_path),
            "compile_results": str(report_path),
            "compile_results_sha256": sha(report_path),
            "driver_log": str(GATE / "driver.log"),
            "driver_log_sha256": sha(GATE / "driver.log"),
            "source_object_binding": str(binding_path),
            "source_object_binding_sha256": sha(binding_path),
            "actual_header_count": binding["actual_header_count"],
            "preflight_header_count": binding["preflight_header_count"],
            "preflight_headers_changed": binding["preflight_headers_changed"],
            "headers_without_preflight_hash": binding["headers_without_preflight_hash"],
            "object": {
                "path": str(object_path),
                "machine": binding["machine"],
                "machine_hex": binding["machine_hex"],
                "size": binding["object_size"],
                "sha256": binding["object_sha256"],
                "assembly": binding["assembly"],
                "assembly_sha256": binding["assembly_sha256"],
            },
        },
        "artist_evidence": {
            "render_black_screen_export": str(ARTIST_EXPORT),
            "render_black_screen_export_sha256": sha(ARTIST_EXPORT),
            "render_black_screen_address": "0x82858988",
            "set_transform_argument": "flt_830112D0 / shared CgsGraphics::Im2dTransform::mgDefault",
            "set_transform_call_site": artist_api_stores,
            "get_default_api": {
                "header": str(IM2D_HEADER),
                "header_sha256": sha(IM2D_HEADER),
                "declaration": "static const Im2dTransform& GetDefault() { return mgDefault; }",
                "global_definition_source": str(IM2D_SOURCE),
                "global_definition_source_sha256": sha(IM2D_SOURCE),
            },
            "default_global_layout": {
                "address": "0x830112D0",
                "source_database": str(ARTIST_INIT_IDB),
                "initializer_evidence_file": str(ARTIST_INIT_PROOF),
                "initializer_evidence_file_sha256": sha(ARTIST_INIT_PROOF),
                "initializer_range": [initializer["function_start"], initializer["function_end"]],
                "initializer_instructions": initializer_instructions,
                "evidence": [
                    "ARTIST 0x82C6C454 forms destination 0x830112D0.",
                    "ARTIST 0x82C6C464 stores vector (-1,+1,0,0) at +0x00: mOriginXYZ.",
                    "ARTIST 0x82C6C470 stores vector (+2,0,0,-2) at +0x10: mRightUp.",
                    "The following vectors at +0x20 and +0x30 are (0,0,0,0) colour shift and (1,1,1,1) colour scale.",
                ],
            },
            "generated_object_reference": mgdefault_lines,
            "generated_render_black_screen_reference": clear_getdefault,
        },
        "changes": [
            "RenderBlackScreen now passes the shared GetDefault transform, matching ARTIST's flt_830112D0 SetTransform argument.",
            "Removed the BRN_IM2D_TRACE host witness and its unused Win32/XTL include, present-counter declaration, and static state.",
            "BeginRendering, state and texture binding, the quad writes, Render, and EndRendering remain in the existing order.",
        ],
        "scope_and_limits": {
            "changed_files": changed,
            "headers_changed": False,
            "other_translation_units_changed": False,
            "whole_project_link_attempted": False,
            "runtime_validation_attempted": False,
            "status": "The changed TU compiles for XDK PPC32 big-endian; this does not certify the whole-project link or runtime.",
        },
    }
    output = ARTIFACT / "DELIVERY.json"
    output.write_text(json.dumps(delivery, indent=2) + "\n", encoding="utf-8")
    print(str(output))
    print("commit=%s parent=%s pass=%d fail=%d object=%s" % (
        commit, parent, report["passed"], report["failed"], binding["machine_hex"]))


if __name__ == "__main__":
    main()
