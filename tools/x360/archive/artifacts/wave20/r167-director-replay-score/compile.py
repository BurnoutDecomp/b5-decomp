"""Build the two source consumers and a typed accessor/layout probe with R165's SDK flags."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

WAVE = Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
ROOT = Path(r"C:\Users\adri1\.codex\worktrees\bp360\native-sound")
OLD_ROOT = r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake"
OUT = Path(__file__).resolve().parent
CODEC = "src/GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.cpp"
GAME = "src/GameSource/Game/BrnGameModule.cpp"
BASE_REPORT = WAVE / "r165-intake/composite-active-native.json"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(plan):
    name, source, row = plan
    src_path = ROOT / source
    source_hash = sha(src_path)
    obj = OUT / (name + ".obj")
    asm = obj.with_suffix(".asm")
    log = obj.with_suffix(".log")
    command = [arg.replace(OLD_ROOT, str(ROOT)) for arg in row["command"][:-3]]
    command.extend(["/Fo" + str(obj), "/Fa" + str(asm), source])
    sdk = Path(command[0]).parents[2]
    env = os.environ.copy()
    for key in ("CL", "_CL_", "LINK", "_LINK_"):
        env.pop(key, None)
    env["PATH"] = str(sdk / "bin/win32") + os.pathsep + env["PATH"]
    temp = OUT / ("temp-" + name)
    temp.mkdir(exist_ok=True)
    env["TEMP"] = env["TMP"] = str(temp)
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT)
    assert sha(src_path) == source_hash
    machine = struct.unpack("<H", obj.read_bytes()[:2])[0] if obj.exists() else None
    row = {
        "source": source, "source_sha256": source_hash, "object": str(obj),
        "assembly": str(asm), "log": str(log), "sdk": str(sdk), "command": command,
        "returncode": result.returncode, "machine": machine,
    }
    if obj.exists():
        row.update(object_sha256=sha(obj), assembly_sha256=sha(asm))
    if result.returncode or machine != 0x1F2:
        print(log.read_text(errors="replace")[-4000:])
    return row


def main():
    rows = {row["source"]: row for row in json.loads(BASE_REPORT.read_text())["results"]}
    plans = [
        ("BrnReplayDirectorBridgeSerialiser-1ea958bd", CODEC.replace("/", "\\"), rows[CODEC]),
        ("BrnGameModule-f766a093", GAME.replace("/", "\\"), rows[GAME]),
        ("layout_probe", str(OUT / "layout_probe.cpp"), rows[CODEC]),
    ]
    with ThreadPoolExecutor(max_workers=3) as workers:
        results = list(workers.map(build, plans))
    receipt = {
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "base_report_sha256": sha(BASE_REPORT), "results": results,
    }
    (OUT / "compile-results.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    assert all(row["returncode"] == 0 and row["machine"] == 0x1F2 for row in results)
    print("PASS 3/3 PPC COFF")


if __name__ == "__main__":
    main()
