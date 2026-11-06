from pathlib import Path
import hashlib, json, os, subprocess, sys

ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\effects_lanes_luna_4277")
CANON = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
HERE = Path(__file__).resolve().parent
OWNED = [
    "src/GameSource/Effects/Boost/BoostStateMachine.cpp",
    "src/GameSource/Effects/BrnCrashTriangleCache.cpp",
    "src/GameSource/Effects/Particles/Native/BrnDebrisArray.cpp",
    "src/GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.cpp",
    "src/GameSource/Effects/Particles/Native/BrnTrailSystem.cpp",
]

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().lower()

if len(sys.argv) < 2:
    raise SystemExit("usage: python run_gate.py STAGE [repo-relative-source.cpp ...]")
stage = HERE / sys.argv[1]
stage.mkdir(exist_ok=False)
sources = sys.argv[2:] or OWNED
if not sources or any(s not in OWNED for s in sources):
    raise SystemExit("the native gate only accepts the five named owned source files")
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
inputs_before = {s: sha256(ROOT / s) for s in sources}
command = [
    sys.executable, "-X", "utf8", "-B", str(ROOT / "tools/build_x360.py"),
    "--sdk", str(CANON / "build/x360/sdk-6534.4/portable/XDK"),
    "--out", str(stage), "--jobs", "1", "--compiler-arg=/showIncludes",
]
for source in sources:
    command.extend(["--source", source])
for vendor in (
    "EABase/include/Common", "EASTL/include", "EAThread/include", "renderware/include",
    "PPMalloc/include", "coreallocator/include", "dirtysdk/include", "zlib/include", "lua/src",
):
    command.append("--compiler-arg=/I" + str(CANON / "vendor" / vendor))

tmp = HERE / "tmp"
tmp.mkdir(exist_ok=True)
env = dict(os.environ)
env["TEMP"] = env["TMP"] = str(tmp)
(stage / "command.json").write_text(json.dumps(command, indent=2) + "\n", encoding="utf-8")
result = subprocess.run(
    command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
    capture_output=True, text=True, encoding="utf-8", errors="replace",
)
(stage / "driver.log").write_text(result.stdout + result.stderr, encoding="utf-8")
inputs_after = {s: sha256(ROOT / s) for s in sources}
if inputs_before != inputs_after:
    raise SystemExit("source changed during compile")
report_path = stage / "compile-results.json"
report = json.loads(report_path.read_text(encoding="utf-8"))
binding = {
    "head_before_compile": head,
    "sources_before_compile": inputs_before,
    "sources_after_compile": inputs_after,
    "compile_report": str(report_path),
    "compile_report_sha256": sha256(report_path),
    "driver_sha256": sha256(__file__),
    "command_sha256": sha256(stage / "command.json"),
    "actual_headers": {},
    "objects_and_assembly": {},
    "logs": {},
}
for row in report.get("results", []):
    log_path = Path(row["log"])
    binding["logs"][row["source"]] = {"path": str(log_path), "sha256": sha256(log_path)}
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        marker = "Note: including file:"
        if marker in line:
            header = Path(line.split(marker, 1)[1].strip())
            if header.is_file():
                binding["actual_headers"][str(header.resolve())] = sha256(header)
    for field in ("object", "assembly"):
        candidate = row.get(field)
        if candidate and Path(candidate).is_file():
            binding["objects_and_assembly"].setdefault(row["source"], {})[field] = {
                "path": candidate,
                "sha256": sha256(candidate),
                "size": Path(candidate).stat().st_size,
            }
(stage / "source-object-header-binding.json").write_text(
    json.dumps(binding, indent=2) + "\n", encoding="utf-8"
)
print(json.dumps({
    "stage": str(stage), "head_before_compile": head, "requested": report.get("requested"),
    "passed": report.get("passed"), "failed": report.get("failed"),
    "actual_headers": len(binding["actual_headers"]), "runner_returncode": result.returncode,
}, indent=2))
sys.exit(result.returncode)
