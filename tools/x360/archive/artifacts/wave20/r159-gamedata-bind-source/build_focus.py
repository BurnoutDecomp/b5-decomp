"""Compile the GameData bind donor using the sealed R157 XDK command lines."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess

OUT = Path(__file__).resolve().parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native-sound")
BASE = "dfa82054e6232064aaa9b9c4e8071dd760dc6dbc"
SEALED = OUT.parent / "r157-intake" / "composite-active-native.json"
OLD_REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
SOURCES = (
    "src/GameSource/Resource/BrnGameDataModule.cpp",
    "src/GameSource/Game/BrnGameModule.cpp",
    "src/SharedClasses/DataLists/VehicleList.cpp",
    "src/SharedClasses/DataLists/BrnHudMessageController.cpp",
    "src/SharedClasses/DataLists/BrnPopupController.cpp",
)
INPUTS = (*SOURCES, "src/GameSource/Resource/BrnGameDataModule.h")

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=REPO, text=True).strip()


def compile_one(row):
    source = row["source"]
    stem = Path(source).stem
    obj = OUT / f"{stem}.obj"
    asm = OUT / f"{stem}.asm"
    log = OUT / f"{stem}.log"
    cmd = row["command"]
    assert cmd[-3].startswith("/Fo") and cmd[-2].startswith("/Fa")
    cmd = [item.replace(str(OLD_REPO), str(REPO)) for item in cmd[:-3]]
    assert any(str(REPO) in item for item in cmd)
    cmd.extend([f"/Fo{obj}", f"/Fa{asm}", source])
    env = {key: value for key, value in os.environ.items()
           if key.upper() not in {"CL", "_CL_", "LINK", "_LINK_"}}
    with log.open("wb") as stream:
        run = subprocess.run(cmd, cwd=REPO, env=env, stdout=stream,
                             stderr=subprocess.STDOUT)
    record = {"source": source, "command": cmd, "exit_code": run.returncode,
              "log_sha256": sha(log)}
    if run.returncode == 0:
        with obj.open("rb") as stream:
            assert stream.read(2) == b"\xf2\x01"
        record.update(ppc_coff_sha256=sha(obj), listing_sha256=sha(asm))
    else:
        for line in log.read_text(errors="replace").splitlines():
            if "error " in line.lower():
                print(line, flush=True)
    return record


def main():
    head = git("rev-parse", "HEAD")
    assert head == BASE or git("rev-parse", "HEAD^") == BASE
    tree = git("rev-parse", "HEAD^{tree}")
    snapshots = {source: sha(REPO / source) for source in INPUTS}
    rows = {row["source"]: row for row in json.loads(SEALED.read_bytes())["results"]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(compile_one, [rows[source] for source in SOURCES]))
    assert head == git("rev-parse", "HEAD")
    assert all(sha(REPO / source) == digest for source, digest in snapshots.items())
    record = {"head": head, "tree": tree, "baseline_sha256": sha(SEALED),
              "input_sha256": snapshots, "results": results}
    (OUT / "compile-receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    passed = sum(row["exit_code"] == 0 for row in results)
    print(f"{passed}/{len(results)} XDK PPC compiles passed", flush=True)
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
