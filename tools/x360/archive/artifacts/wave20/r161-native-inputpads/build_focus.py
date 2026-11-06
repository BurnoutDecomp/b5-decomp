
"""Run the sealed XDK commands for the R161 pad input provider and direct consumers."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

OUT = Path(__file__).resolve().parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native-sound")
OLD_REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "d9fe6e6026013a76bb1fb987e7aa78a54affa569"
SEAL = OUT.parent / "r160-intake/composite-active-native.json"
INPUT = "src/GameShared/GameClasses/System/Input/"
GAME = "src/GameSource/Game/"
SOURCES = (
    "src/GameShared/GameClasses/Input/CgsInputModule.cpp",
    INPUT + "CgsInputPads.cpp",
    INPUT + "CgsInputModuleIO.cpp",
    INPUT + "Devices/X360/CgsInputManagerX360.cpp",
    INPUT + "Devices/X360/CgsInputDeviceX360Pad.cpp",
    GAME + "BrnAutoTestManager.cpp",
    GAME + "BrnGameModule.cpp",
    GAME + "GameBridgeControllerToX.cpp",
    "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp",
    "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp",
)
HEADERS = (
    "src/GameShared/GameClasses/Input/CgsInputModule.h",
    INPUT + "CgsInputPads.h",
    INPUT + "CgsInputModuleIO.h",
    INPUT + "CgsInputTypes.h",
    INPUT + "Devices/X360/CgsInputManagerX360.h",
    INPUT + "Devices/X360/CgsInputDeviceX360Pad.h",
    GAME + "BrnAutoTestManager.h",
    GAME + "BrnGameModule.hpp",
)


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
    original = row["command"]
    assert original[-3].startswith("/Fo") and original[-2].startswith("/Fa")
    command = [item.replace(str(OLD_REPO), str(REPO)) for item in original[:-3]]
    assert any(str(REPO) in item for item in command) and all(str(OLD_REPO) not in item for item in command)
    command.extend([f"/Fo{obj}", f"/Fa{asm}", source])
    env = {key: value for key, value in os.environ.items()
           if key.upper() not in {"CL", "_CL_", "LINK", "_LINK_"}}
    with log.open("wb") as stream:
        run = subprocess.run(command, cwd=REPO, env=env, stdout=stream, stderr=subprocess.STDOUT)
    record = {"source": source, "command": command, "exit_code": run.returncode,
              "log_sha256": sha(log)}
    diagnostics = log.read_text(errors="replace")
    record["warnings"] = re.findall(r"^.*(?:warning [A-Z]+[0-9]+).*$", diagnostics, re.M)
    if run.returncode == 0:
        with obj.open("rb") as stream:
            assert stream.read(2) == b"\xf2\x01"
        record.update(machine="0x01F2", ppc_coff_sha256=sha(obj), listing_sha256=sha(asm))
    else:
        for line in diagnostics.splitlines():
            if "error " in line.lower():
                print(line, flush=True)
    return record


def main():
    head = git("rev-parse", "HEAD")
    assert git("merge-base", BASE, head) == BASE
    tree = git("rev-parse", "HEAD^{tree}")
    snapshots = {source: sha(REPO / source) for source in (*SOURCES, *HEADERS)}
    sealed = {row["source"]: row for row in json.loads(SEAL.read_bytes())["results"]}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(compile_one, [sealed[source] for source in SOURCES]))
    assert head == git("rev-parse", "HEAD")
    assert all(sha(REPO / source) == digest for source, digest in snapshots.items())
    receipt = {"head": head, "tree": tree, "status": git("status", "--porcelain=v1"),
               "sealed_native_commands_sha256": sha(SEAL), "input_sha256": snapshots, "results": results}
    (OUT / "compile-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    passed = sum(row["exit_code"] == 0 for row in results)
    warnings = sum(len(row["warnings"]) for row in results)
    print(f"{passed}/{len(results)} Xbox 360 compiles passed with {warnings} compiler warnings", flush=True)
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())

