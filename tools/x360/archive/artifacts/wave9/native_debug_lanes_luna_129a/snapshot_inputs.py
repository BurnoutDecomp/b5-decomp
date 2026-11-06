from pathlib import Path
import hashlib, json, subprocess

repo = Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
canonical = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
sdk = canonical / "build/x360/sdk-6534.4/portable/XDK"
artifact = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a")
sources = [
    "src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d_wBT_01.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.cpp",
    "src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dUntex.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Menu/CgsMenuItem.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsErrorWindow.cpp",
    "src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.cpp",
]
def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()
head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
status = subprocess.check_output(["git", "status", "--porcelain"], cwd=repo, text=True).strip()
if head != "129afef95856e383366a55fb8f30a66ceb74de42" or status:
    raise SystemExit(f"preflight requires clean frozen R7: head={head}, status={status!r}")
header_paths = set()
for raw in subprocess.check_output(["git", "ls-files", "-z", "--", "*.h", "*.hpp", "*.inl", "*.ipp"], cwd=repo).decode().split("\0"):
    if raw:
        p = repo / raw
        if p.is_file():
            header_paths.add(p.resolve())
external_roots = [
    canonical / "vendor/EABase/include/Common", canonical / "vendor/EASTL/include",
    canonical / "vendor/EAThread/include", canonical / "vendor/renderware/include",
    canonical / "vendor/PPMalloc/include", canonical / "vendor/coreallocator/include",
    canonical / "vendor/dirtysdk/include", canonical / "vendor/zlib/include",
    canonical / "vendor/lua/src", sdk / "include/xbox",
]
exts = {".h", ".hpp", ".inl", ".ipp"}
for root in external_roots:
    if not root.is_dir():
        raise SystemExit(f"missing expected native include root: {root}")
    for p in root.rglob("*"):
        if p.is_file() and p.suffix.lower() in exts and ".git" not in p.parts:
            header_paths.add(p.resolve())
source_rows = {}
for name in sources:
    p = repo / name
    if not p.is_file():
        raise SystemExit(f"missing owned candidate source: {name}")
    blob = subprocess.check_output(["git", "rev-parse", f"HEAD:{name}"], cwd=repo, text=True).strip()
    source_rows[name] = {"git_blob": blob, "sha256": sha(p)}
headers = {str(p): sha(p) for p in sorted(header_paths, key=lambda x: str(x).lower())}
manifest = {
    "head": head,
    "branch": subprocess.check_output(["git", "branch", "--show-current"], cwd=repo, text=True).strip(),
    "source_count": len(source_rows),
    "sources": source_rows,
    "header_count": len(headers),
    "headers": headers,
    "header_roots": [str(p.resolve()) for p in external_roots],
}
(artifact / "preflight-input-binding.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"head": head, "branch": manifest["branch"], "sources": len(source_rows), "headers": len(headers), "manifest": str(artifact / "preflight-input-binding.json")}, indent=2))