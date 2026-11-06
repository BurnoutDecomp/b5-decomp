"""Compile the private GameData DLC stage candidate with sealed R156 XDK flags."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess

OUT = Path(__file__).resolve().parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
OLD_REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASELINE = OUT.parent / "r156-intake-v2" / "composite-active-native.json"
SOURCES = (
    "src/GameSource/Resource/BrnGameDataModule.cpp",
    "src/GameSource/Resource/BrnDLCManager.cpp",
    "src/GameSource/Game/BrnGameModule.cpp",
    "src/GameShared/GameClasses/System/Resource/CgsResourceModuleIO_InputBuffer_GetResourceQueue.cpp",
)
INPUTS = (*SOURCES, "src/GameSource/Resource/BrnGameDataModule.h",
          "src/GameSource/Resource/BrnDLCManager.h")


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=REPO, text=True).strip()


def compile_one(row):
    source = row["source"]
    name = Path(source).stem
    obj = OUT / f"{name}.obj"
    asm = OUT / f"{name}.asm"
    log = OUT / f"{name}.log"
    cmd = row["command"]
    assert cmd[-3].startswith("/Fo") and cmd[-2].startswith("/Fa")
    cmd = [item.replace(str(OLD_REPO), str(REPO)) for item in cmd[:-3]]
    cmd.extend([f"/Fo{obj}", f"/Fa{asm}", source])
    env = {key: value for key, value in os.environ.items()
           if key.upper() not in {"CL", "_CL_", "LINK", "_LINK_"}}
    with log.open("wb") as stream:
        completed = subprocess.run(cmd, cwd=REPO, env=env, stdout=stream,
                                   stderr=subprocess.STDOUT)
    result = {"source": source, "command": cmd, "exit_code": completed.returncode,
              "log_sha256": sha(log)}
    if completed.returncode == 0:
        with obj.open("rb") as stream:
            assert stream.read(2) == b"\xf2\x01"
        result.update({"ppc_coff_sha256": sha(obj), "listing_sha256": sha(asm)})
    else:
        for line in log.read_text(errors="replace").splitlines():
            if "error " in line.lower():
                print(line, flush=True)
    return result


def main():
    head = git("rev-parse", "HEAD")
    tree = git("rev-parse", "HEAD^{tree}")
    snapshots = {source: sha(REPO / source) for source in INPUTS}
    rows = {row["source"]: row for row in json.loads(BASELINE.read_bytes())["results"]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=len(SOURCES)) as pool:
        results = list(pool.map(compile_one, [rows[source] for source in SOURCES]))
    assert head == git("rev-parse", "HEAD")
    assert all(sha(REPO / source) == digest for source, digest in snapshots.items())
    record = {"head": head, "tree": tree, "baseline_sha256": sha(BASELINE),
              "input_sha256": snapshots, "results": results,
              "scope": "Focused XDK PPC COFF only; no strict link or launch."}
    (OUT / "compile-receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    passed = sum(result["exit_code"] == 0 for result in results)
    print(f"{passed}/{len(results)} XDK PPC compiles passed", flush=True)
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
