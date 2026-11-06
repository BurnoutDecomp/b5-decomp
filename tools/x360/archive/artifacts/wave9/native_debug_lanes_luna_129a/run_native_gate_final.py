from pathlib import Path
import hashlib, json, os, struct, subprocess, sys

repo = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
canonical = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
sdk = canonical / "build/x360/sdk-6534.4/portable/XDK"
artifact = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a")
stage = artifact / "five-failure-native-gate-final"
source_paths = [
    "src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d_wBT_01.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dUntex.cpp",
]
preflight = json.loads((artifact / "preflight-input-binding.json").read_text(encoding="utf-8"))
expected = {str(Path(repo / s).resolve()).lower() for s in source_paths}
status = subprocess.check_output(["git", "status", "--porcelain"], cwd=repo, text=True).splitlines()
changed = {str((repo / line[3:]).resolve()).lower() for line in status if line and line[:2] != "??"}
untracked = [line for line in status if line.startswith("??")]
if changed != expected or untracked:
    raise SystemExit(f"scope mismatch before compile: changed={sorted(changed)} untracked={untracked}")
if stage.exists():
    raise SystemExit(f"gate output already exists: {stage}")
stage.mkdir(parents=True)
tmp = artifact / "temp"
tmp.mkdir(exist_ok=True)
def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()
sources_before = {s: sha(repo / s) for s in source_paths}
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
command = [sys.executable, "-X", "utf8", "-B", str(repo / "tools/build_x360.py"),
           "--sdk", str(sdk), "--out", str(stage), "--jobs", "1",
           "--compiler-arg=/showIncludes"]
for s in source_paths:
    command.extend(["--source", s])
for relative in (
    "EABase/include/Common", "EASTL/include", "EAThread/include", "renderware/include",
    "PPMalloc/include", "coreallocator/include", "dirtysdk/include", "zlib/include", "lua/src",
):
    command.append("--compiler-arg=/I" + str(canonical / "vendor" / relative))
(stage / "command.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8")
env = dict(os.environ)
env["TEMP"] = str(tmp)
env["TMP"] = str(tmp)
result = subprocess.run(command, cwd=repo, env=env, stdin=subprocess.DEVNULL,
                        capture_output=True, text=True, encoding="utf-8", errors="replace")
(stage / "driver.log").write_text(result.stdout + result.stderr, encoding="utf-8")
sources_after = {s: sha(repo / s) for s in source_paths}
if sources_before != sources_after:
    raise SystemExit("one or more source files changed during the native gate")
report_path = stage / "compile-results.json"
if not report_path.is_file():
    raise SystemExit(f"native build produced no compile report (exit {result.returncode})")
report = json.loads(report_path.read_text(encoding="utf-8"))
pre_headers = {str(Path(p).resolve()).lower(): h for p, h in preflight["headers"].items()}
header_binding = {}
objects = {}
rows = []
for row in report["results"]:
    log = Path(row["log"]).read_text(encoding="utf-8", errors="replace")
    for line in log.splitlines():
        marker = "Note: including file:"
        if line.startswith(marker):
            path = Path(line[len(marker):].strip()).resolve()
            if not path.is_file():
                raise SystemExit(f"showIncludes path is missing: {path}")
            key = str(path).lower()
            digest = sha(path)
            header_binding[key] = {"path": str(path), "sha256": digest,
                                   "preflight_sha256": pre_headers.get(key),
                                   "preflight_bound": key in pre_headers,
                                   "unchanged": pre_headers.get(key) == digest if key in pre_headers else None}
    obj_path = Path(row["object"])
    if obj_path.is_file():
        data = obj_path.read_bytes()
        machine = struct.unpack("<H", data[:2])[0] if len(data) >= 2 else None
        objects[row["source"]] = {"path": str(obj_path.resolve()), "size": len(data),
                                   "machine": machine, "machine_hex": f"0x{machine:04X}" if machine is not None else None,
                                   "sha256": sha(obj_path), "verified_ppc32be_coff": machine == 0x01F2}
    errors = [line for line in log.splitlines() if "error " in line]
    rows.append({"source": row["source"], "passed": row["passed"], "returncode": row["returncode"],
                 "machine": row["machine"], "log": str(Path(row["log"]).resolve()),
                 "log_sha256": sha(row["log"]), "diagnostic_count": len(errors),
                 "diagnostics": errors, "object": objects.get(row["source"])})
report = json.loads(report_path.read_text(encoding="utf-8"))
binding = {
    "base_head": head,
    "target": "xbox360-ppc-be",
    "sdk": str(sdk.resolve()),
    "command": command,
    "driver_returncode": result.returncode,
    "driver_sha256": sha(__file__),
    "preflight_sha256": sha(artifact / "preflight-input-binding.json"),
    "preflight_source_hashes": preflight["sources"],
    "sources_before_gate": sources_before,
    "sources_after_gate": sources_after,
    "source_unchanged_during_gate": sources_before == sources_after,
    "requested": report.get("requested"),
    "passed": report.get("passed"),
    "failed": report.get("failed"),
    "actual_header_count": len(header_binding),
    "actual_headers": sorted(header_binding.values(), key=lambda x: x["path"].lower()),
    "preflight_bound_header_count": sum(1 for h in header_binding.values() if h["preflight_bound"]),
    "preflight_unbound_headers": [h["path"] for h in header_binding.values() if not h["preflight_bound"]],
    "headers_unchanged": all(h["unchanged"] for h in header_binding.values() if h["preflight_bound"]),
    "native_objects": objects,
    "results": rows,
}
(stage / "source-object-header-binding.json").write_text(json.dumps(binding, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"head": head, "returncode": result.returncode,
                  "requested": report.get("requested"), "passed": report.get("passed"),
                  "failed": report.get("failed"), "headers": len(header_binding),
                  "preflight_bound": binding["preflight_bound_header_count"],
                  "preflight_unbound": len(binding["preflight_unbound_headers"]),
                  "headers_unchanged": binding["headers_unchanged"],
                  "binding": str(stage / "source-object-header-binding.json")}, indent=2))
for row in rows:
    print(("PASS " if row["passed"] else "FAIL ") + row["source"] +
          f" machine={row['machine']} diagnostics={row['diagnostic_count']} object_sha256=" +
          (row["object"]["sha256"] if row["object"] else "missing"))
    for line in row["diagnostics"][:8]:
        print("  " + line)
sys.exit(result.returncode or (1 if report.get("failed") else 0))