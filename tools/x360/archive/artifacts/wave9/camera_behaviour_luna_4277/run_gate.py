from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_behaviour_luna_4277")
CANON = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
ARTIFACT = Path(__file__).resolve().parent
SDK = CANON / "build/x360/sdk-6534.4/portable/XDK"
SOURCES = [
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp",
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.cpp",
    "src/GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCamera2DRotationController.cpp",
    "src/GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.cpp",
]
HEADER_EXTENSIONS = {".h", ".hpp", ".hxx", ".hh", ".inl", ".inc", ".ipp"}
SNAPSHOT = ARTIFACT / "preflight-input-snapshot.json"


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
    root = Path(root).resolve()
    is_sdk = root == (SDK / "include/xbox").resolve()
    if not root.is_dir():
        return
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() in HEADER_EXTENSIONS or (is_sdk and not path.suffix):
            yield path.resolve()


def capture_initial_snapshot(head):
    roots = [
        ROOT / "src",
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
    source_preimages = {}
    for source in SOURCES:
        blob = subprocess.check_output(["git", "rev-parse", "HEAD:" + source], cwd=ROOT, text=True).strip()
        source_bytes = subprocess.check_output(["git", "show", "HEAD:" + source], cwd=ROOT)
        worktree_sha = sha_file(ROOT / source)
        source_preimages[source] = {
            "blob": blob,
            "blob_bytes_sha256": sha_bytes(source_bytes),
            "worktree_preflight_sha256": worktree_sha,
        }

    headers = {}
    seen = set()
    for root in roots:
        for path in header_paths(root):
            key = canonical_key(path)
            if key in seen:
                continue
            seen.add(key)
            headers[str(path)] = sha_file(path)
    snapshot = {
        "head": head,
        "branch": git("branch", "--show-current"),
        "captured_utc": datetime.now(timezone.utc).isoformat(),
        "sources": source_preimages,
        "header_roots": [str(root) for root in roots],
        "header_count": len(headers),
        "headers_sha256": headers,
    }
    SNAPSHOT.write_text(json.dumps(snapshot, indent=2) + "\n", encoding="utf-8")
    return snapshot


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("initial", "post"):
        raise SystemExit("usage: run_gate.py initial|post")
    phase = sys.argv[1]
    head = git("rev-parse", "HEAD")
    status = subprocess.check_output(["git", "status", "--porcelain=v1"], cwd=ROOT, text=True).splitlines()

    if phase == "initial":
        if status:
            raise SystemExit("initial gate requires a clean R8 preimage: " + repr(status))
        snapshot = capture_initial_snapshot(head)
        sources = list(SOURCES)
    else:
        if not SNAPSHOT.is_file():
            raise SystemExit("post gate requires the initial input snapshot")
        snapshot = json.loads(SNAPSHOT.read_text(encoding="utf-8-sig"))
        if snapshot["head"] != head:
            raise SystemExit("post gate HEAD differs from the captured R8 base")
        changed = sorted(line[3:].replace("\\", "/") for line in status)
        unexpected = sorted(set(changed) - set(SOURCES))
        if unexpected:
            raise SystemExit("changes escape the assigned five sources: " + repr(unexpected))
        sources = [source for source in SOURCES if source in changed]
        if not sources:
            raise SystemExit("post gate has no changed assigned sources")

    stage = ARTIFACT / ("initial-gate" if phase == "initial" else "post-gate")
    if stage.exists():
        raise SystemExit("gate output already exists: " + str(stage))
    stage.mkdir(parents=True)
    tmp = ARTIFACT / "tmp"
    tmp.mkdir(exist_ok=True)

    source_preflight = {source: sha_file(ROOT / source) for source in sources}
    if phase == "initial":
        for source in sources:
            if source_preflight[source] != snapshot["sources"][source]["worktree_preflight_sha256"]:
                raise SystemExit("source changed after initial preflight: " + source)

    command = [
        sys.executable, "-X", "utf8", "-B", str(ROOT / "tools/build_x360.py"),
        "--sdk", str(SDK), "--out", str(stage), "--jobs", "1", "--timeout", "180",
        "--compiler-arg=/showIncludes",
    ]
    for source in sources:
        command.extend(["--source", source])
    for vendor_path in (
        "EABase/include/Common", "EASTL/include", "EAThread/include",
        "renderware/include", "PPMalloc/include", "coreallocator/include",
        "dirtysdk/include", "zlib/include", "lua/src",
    ):
        command.append("--compiler-arg=/I" + str(CANON / "vendor" / vendor_path))
    (stage / "command.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8")

    env = dict(os.environ)
    env["TEMP"] = str(tmp)
    env["TMP"] = str(tmp)
    result = subprocess.run(
        command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    (stage / "driver.log").write_text(result.stdout + result.stderr, encoding="utf-8")

    source_postflight = {source: sha_file(ROOT / source) for source in sources}
    if source_preflight != source_postflight:
        raise SystemExit("source changed during native compile")
    report_path = stage / "compile-results.json"
    report = json.loads(report_path.read_text(encoding="utf-8"))
    header_preflight = {canonical_key(path): value for path, value in snapshot["headers_sha256"].items()}
    postflight_headers = {}
    bindings = {}
    objects = {}
    for row in report["results"]:
        log_path = Path(row["log"])
        log = log_path.read_text(encoding="utf-8", errors="replace")
        actual = []
        for line in log.splitlines():
            marker = "Note: including file:"
            if not line.startswith(marker):
                continue
            header = Path(line[len(marker):].strip()).resolve()
            if not header.is_file():
                raise SystemExit("compiler-reported include is missing: " + str(header))
            key = canonical_key(header)
            post_sha = postflight_headers.get(key)
            if post_sha is None:
                post_sha = sha_file(header)
                postflight_headers[key] = post_sha
            pre_sha = header_preflight.get(key)
            actual.append({
                "path": str(header),
                "preflight_sha256": pre_sha,
                "postflight_sha256": post_sha,
                "preflight_status": "not_captured" if pre_sha is None else
                                    "matched" if pre_sha == post_sha else "changed",
            })
        bindings[row["source"]] = {
            "passed": row["passed"],
            "returncode": row["returncode"],
            "machine": row.get("machine"),
            "log": str(log_path),
            "log_sha256": sha_file(log_path),
            "errors": [line for line in log.splitlines() if "error " in line],
            "included_headers": actual,
        }
        obj = Path(row["object"])
        asm = Path(row["assembly"])
        objects[row["source"]] = {
            "path": str(obj),
            "exists": obj.is_file(),
            "size": obj.stat().st_size if obj.is_file() else None,
            "machine": row.get("machine"),
            "machine_hex": "0x%04X" % row["machine"] if row.get("machine") is not None else None,
            "sha256": sha_file(obj) if obj.is_file() else None,
            "assembly": str(asm),
            "assembly_sha256": sha_file(asm) if asm.is_file() else None,
        }
    source_blobs = {source: snapshot["sources"][source] for source in sources}
    binding_path = stage / "source-object-binding.json"
    binding = {
        "phase": phase,
        "head": head,
        "branch": git("branch", "--show-current"),
        "sources": sources,
        "source_preimages": source_blobs,
        "source_preflight_sha256": source_preflight,
        "source_postflight_sha256": source_postflight,
        "target": report.get("target"),
        "passed": report.get("passed"),
        "failed": report.get("failed"),
        "compile_results_sha256": sha_file(report_path),
        "driver_log_sha256": sha_file(stage / "driver.log"),
        "preflight_snapshot": str(SNAPSHOT),
        "preflight_snapshot_sha256": sha_file(SNAPSHOT),
        "preflight_header_count": snapshot["header_count"],
        "actual_header_count": len(postflight_headers),
        "actual_header_sha256": {path: value for path, value in postflight_headers.items()},
        "source_results": bindings,
        "objects": objects,
    }
    binding_path.write_text(json.dumps(binding, indent=2) + "\n", encoding="utf-8")
    print(result.stdout, end="")
    print("%s gate: %d pass, %d fail; sources=%d; actual headers=%d" % (
        phase, report.get("passed", 0), report.get("failed", 0), len(sources), len(postflight_headers)))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
