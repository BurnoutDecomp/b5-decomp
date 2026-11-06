from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_gui_lanes_luna_129a")
CANON = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
ARTIFACT = Path(__file__).resolve().parent
STAGE = ARTIFACT / "native-onefile-gate"
SOURCE = "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp"
SDK = CANON / "build/x360/sdk-6534.4/portable/XDK"
HEADER_EXTENSIONS = {".h", ".hpp", ".hxx", ".hh", ".inl", ".inc", ".ipp"}


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def canonical_key(path):
    return str(Path(path).resolve()).casefold()


def header_paths(root):
    root = Path(root)
    if not root.is_dir():
        return
    is_sdk = root == (SDK / "include/xbox").resolve()
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() in HEADER_EXTENSIONS or (is_sdk and not path.suffix):
            yield path.resolve()


def main():
    if STAGE.exists():
        raise SystemExit("gate output already exists: " + str(STAGE))
    STAGE.mkdir(parents=True)
    tmp = ARTIFACT / "tmp"
    tmp.mkdir(exist_ok=True)

    head = git("rev-parse", "HEAD")
    branch = git("branch", "--show-current")
    preimage_blob = git("rev-parse", "HEAD:" + SOURCE)
    preimage = subprocess.check_output(["git", "show", "HEAD:" + SOURCE], cwd=ROOT)
    source_preimage_sha256 = sha_bytes(preimage)
    source_preflight_sha256 = sha_file(ROOT / SOURCE)
    status = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()
    changed = sorted(line[3:].replace("\\", "/") for line in status)
    if changed != [SOURCE]:
        raise SystemExit("worktree changes escape the one-file scope: " + repr(changed))

    include_roots = [
        ROOT / "src",
        ROOT / "src/SDKs/EATech",
        ROOT / "vendor/EABase/include/Common",
        ROOT / "vendor/EASTL/include",
        ROOT / "vendor/EAThread/include",
        ROOT / "vendor/renderware/include",
        ROOT / "vendor/PPMalloc/include",
        ROOT / "vendor/coreallocator/include",
        ROOT / "vendor/dirtysdk/include",
        ROOT / "vendor/zlib/include",
        ROOT / "vendor/lua/src",
        CANON / "vendor/EABase/include/Common",
        CANON / "vendor/EASTL/include",
        CANON / "vendor/EAThread/include",
        CANON / "vendor/renderware/include",
        CANON / "vendor/PPMalloc/include",
        CANON / "vendor/coreallocator/include",
        CANON / "vendor/dirtysdk/include",
        CANON / "vendor/zlib/include",
        CANON / "vendor/lua/src",
        SDK / "include/xbox",
    ]
    header_preflight = {}
    for root in include_roots:
        for path in header_paths(root):
            header_preflight[str(path)] = sha_file(path)
    input_snapshot = {
        "head": head,
        "branch": branch,
        "source": SOURCE,
        "source_preimage_blob": preimage_blob,
        "source_preimage_sha256": source_preimage_sha256,
        "source_preflight_sha256": source_preflight_sha256,
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "header_roots": [str(root) for root in include_roots],
        "header_count": len(header_preflight),
        "headers_sha256": header_preflight,
    }
    input_snapshot_path = ARTIFACT / "preflight-input-snapshot.json"
    input_snapshot_path.write_text(json.dumps(input_snapshot, indent=2) + "\n", encoding="utf-8")

    command = [
        sys.executable, "-X", "utf8", "-B", str(ROOT / "tools/build_x360.py"),
        "--sdk", str(SDK), "--out", str(STAGE), "--jobs", "1", "--timeout", "180",
        "--compiler-arg=/showIncludes", "--source", SOURCE,
    ]
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
    source_postflight_sha256 = sha_file(ROOT / SOURCE)
    if source_postflight_sha256 != source_preflight_sha256:
        raise SystemExit("source changed during native compile")

    report_path = STAGE / "compile-results.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if len(report.get("results", [])) != 1 or report["results"][0]["source"] != SOURCE:
        raise SystemExit("compile report is not exactly the assigned source")
    row = report["results"][0]
    log_path = Path(row["log"])
    log = log_path.read_text(encoding="utf-8", errors="replace")
    actual_headers = []
    postflight_header_hashes = {}
    for line in log.splitlines():
        marker = "Note: including file:"
        if not line.startswith(marker):
            continue
        path = Path(line[len(marker):].strip()).resolve()
        if not path.is_file():
            raise SystemExit("compiler-reported include is missing: " + str(path))
        post_sha = sha_file(path)
        pre_sha = next((value for name, value in header_preflight.items()
                        if canonical_key(name) == canonical_key(path)), None)
        actual_headers.append({
            "path": str(path),
            "preflight_sha256": pre_sha,
            "postflight_sha256": post_sha,
            "preflight_status": "not_captured" if pre_sha is None else
                                "matched" if pre_sha == post_sha else "changed",
        })
        postflight_header_hashes[str(path)] = post_sha

    object_path = Path(row["object"])
    listing_path = Path(row["assembly"])
    machine = row.get("machine")
    binding = {
        "head": head,
        "branch": branch,
        "source": SOURCE,
        "source_preimage_blob": preimage_blob,
        "source_preimage_sha256": source_preimage_sha256,
        "source_preflight_sha256": source_preflight_sha256,
        "source_postflight_sha256": source_postflight_sha256,
        "source_preflight_matches_postflight": source_preflight_sha256 == source_postflight_sha256,
        "target": report.get("target"),
        "sdk": report.get("sdk"),
        "passed": row["passed"],
        "returncode": row["returncode"],
        "machine": machine,
        "machine_hex": "0x%04X" % machine if machine is not None else None,
        "object": str(object_path),
        "object_sha256": sha_file(object_path) if object_path.is_file() else None,
        "object_size": object_path.stat().st_size if object_path.is_file() else None,
        "assembly": str(listing_path),
        "assembly_sha256": sha_file(listing_path) if listing_path.is_file() else None,
        "compile_report": str(report_path),
        "compile_report_sha256": sha_file(report_path),
        "compile_log": str(log_path),
        "compile_log_sha256": sha_file(log_path),
        "driver_log": str(STAGE / "driver.log"),
        "driver_log_sha256": sha_file(STAGE / "driver.log"),
        "actual_header_count": len(postflight_header_hashes),
        "actual_header_hashes": postflight_header_hashes,
        "actual_header_bindings": actual_headers,
        "preflight_header_count": len(header_preflight),
        "preflight_headers_changed": sorted(
            item["path"] for item in actual_headers if item["preflight_status"] == "changed"),
        "headers_without_preflight_hash": sorted(
            item["path"] for item in actual_headers if item["preflight_status"] == "not_captured"),
    }
    binding_path = STAGE / "source-object-binding.json"
    binding_path.write_text(json.dumps(binding, indent=2) + "\n", encoding="utf-8")
    print(result.stdout, end="")
    print("native gate: %d passed, %d failed; headers=%d; machine=%s" % (
        report.get("passed", 0), report.get("failed", 0), len(postflight_header_hashes), binding["machine_hex"]))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
