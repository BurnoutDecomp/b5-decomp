from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_gui_lanes_luna_129a")
CANON = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
HERE = Path(__file__).resolve().parent
STAGE = HERE / "native-5file-gate"
SOURCES = [
    "src/GameShared/GameClasses/Gui/CgsGuiShared.cpp",
    "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.cpp",
    "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp",
    "src/GameShared/GameClasses/Gui/View/CgsParticleSystem2d.cpp",
    "src/GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.cpp",
]


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_key(path):
    return str(Path(path).resolve()).casefold()


def main():
    if STAGE.exists():
        raise SystemExit("gate output already exists: " + str(STAGE))
    STAGE.mkdir(parents=True)
    tmp = HERE / "tmp"
    tmp.mkdir(exist_ok=True)

    preflight = json.loads((HERE / "preflight-input-snapshot.json").read_text(encoding="utf-8-sig"))
    vendor_snapshot = json.loads((HERE / "canonical-vendor-input-snapshot.json").read_text(encoding="utf-8-sig"))
    old_source_hashes = preflight["source_sha256"]
    source_before = {name: sha256(ROOT / name) for name in SOURCES}
    if set(old_source_hashes) != set(SOURCES):
        raise SystemExit("source preimage snapshot does not match the assigned five files")
    if source_before.keys() != old_source_hashes.keys():
        raise SystemExit("source key set changed")

    header_before = {}
    for key, value in preflight["headers_sha256"].items():
        header_before[canonical_key(key)] = value
    for key, value in vendor_snapshot["headers_sha256"].items():
        header_before[canonical_key(key)] = value

    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    branch = subprocess.check_output(["git", "branch", "--show-current"], cwd=ROOT, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()
    changed = sorted(line[3:].replace("\\", "/") for line in status)
    if changed != sorted(SOURCES):
        raise SystemExit("worktree changes escape the assigned five files: " + repr(changed))

    command = [
        sys.executable, "-X", "utf8", "-B", str(ROOT / "tools/build_x360.py"),
        "--sdk", str(CANON / "build/x360/sdk-6534.4/portable/XDK"),
        "--out", str(STAGE), "--jobs", "1", "--timeout", "180",
        "--compiler-arg=/showIncludes",
    ]
    for source in SOURCES:
        command.extend(["--source", source])
    for vendor_path in (
        "EABase/include/Common", "EASTL/include", "EAThread/include",
        "renderware/include", "PPMalloc/include", "coreallocator/include",
        "dirtysdk/include", "zlib/include", "lua/src",
    ):
        command.append("--compiler-arg=/I" + str(CANON / "vendor" / vendor_path))

    (STAGE / "command.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8")
    env = dict(os.environ)
    env["TEMP"] = str(tmp)
    env["TMP"] = str(tmp)
    result = subprocess.run(
        command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    (STAGE / "driver.log").write_text(result.stdout + result.stderr, encoding="utf-8")

    source_after = {name: sha256(ROOT / name) for name in SOURCES}
    if source_before != source_after:
        raise SystemExit("assigned source changed during the native gate")

    report_path = STAGE / "compile-results.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    actual_headers = {}
    objects = {}
    diagnostics = {}
    for row in report["results"]:
        log_path = Path(row["log"])
        log = log_path.read_text(encoding="utf-8", errors="replace")
        included = []
        for line in log.splitlines():
            marker = "Note: including file:"
            if not line.startswith(marker):
                continue
            included_path = Path(line[len(marker):].strip()).resolve()
            if not included_path.is_file():
                raise SystemExit("compiler reported a missing included header: " + str(included_path))
            included_sha = sha256(included_path)
            before_sha = header_before.get(canonical_key(included_path))
            included.append({
                "path": str(included_path),
                "preflight_sha256": before_sha,
                "postflight_sha256": included_sha,
                "preflight_status": ("not_captured" if before_sha is None else
                                     "matched" if before_sha == included_sha else "changed"),
                "matches_preflight": None if before_sha is None else before_sha == included_sha,
            })
            actual_headers[str(included_path)] = included_sha
        diagnostics[row["source"]] = {
            "errors": [line for line in log.splitlines() if "error " in line],
            "included_headers": included,
        }
        object_path = Path(row["object"])
        objects[row["source"]] = {
            "path": str(object_path),
            "exists": object_path.is_file(),
            "size": object_path.stat().st_size if object_path.is_file() else None,
            "machine": row.get("machine"),
            "machine_hex": "0x%04X" % row["machine"] if row.get("machine") is not None else None,
            "sha256": sha256(object_path) if object_path.is_file() else None,
            "assembly_sha256": sha256(row["assembly"]) if Path(row["assembly"]).is_file() else None,
        }

    binding = {
        "head": head,
        "branch": branch,
        "compiler_target": report.get("target"),
        "sdk": report.get("sdk"),
        "source_preimage_sha256": old_source_hashes,
        "source_preflight_sha256": source_before,
        "source_postflight_sha256": source_after,
        "source_postflight_matches_preflight": source_before == source_after,
        "actual_headers": actual_headers,
        "actual_header_count": len(actual_headers),
        "diagnostics": diagnostics,
        "objects": objects,
        "compile_report_sha256": sha256(report_path),
        "driver_log_sha256": sha256(STAGE / "driver.log"),
        "exit_code": result.returncode,
        "passed": report.get("passed"),
        "failed": report.get("failed"),
    }
    (STAGE / "source-object-binding.json").write_text(json.dumps(binding, indent=2) + "\n", encoding="utf-8")
    print(result.stdout, end="")
    print("driver exit=%d; result=%d pass, %d fail; actual included headers=%d" % (
        result.returncode, report.get("passed", 0), report.get("failed", 0), len(actual_headers)))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
