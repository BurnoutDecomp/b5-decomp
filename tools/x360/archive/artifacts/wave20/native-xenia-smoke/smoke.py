"""Stage and launch a checked native game image with a private Xenia profile."""

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import time
import tomllib

HERE = pathlib.Path(__file__).resolve().parent
XENIA = pathlib.Path("D:/Emulation/Emulators/Xenia/Xenia Burnout 5 v6")
SOURCE_GAME = XENIA / "Burnout_tcartwright"
CONFIG_NAME = "xenia-burnout5.config.toml"

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def xex_magic(path):
    with path.open("rb") as stream:
        return stream.read(4) == b"XEX2"

def write(path, payload):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2)
        stream.write("\n")

def config_text(original, private, log):
    settings = {
        ("CPU", "trace_functions"): True,
        ("CPU", "trace_function_data"): True,
        ("CPU", "trace_function_data_path"): str(private.parent / "funcdata") + os.sep,
        ("General", "apply_patches"): False,
        ("General", "allow_plugins"): False,
        ("General", "allow_game_relative_writes"): False,
        ("Kernel", "apply_title_update"): False,
        ("Logging", "enable_console"): False,
        ("Logging", "flush_log"): True,
        ("Logging", "log_file"): str(log),
        ("Logging", "log_level"): 2,
        ("Logging", "log_to_stdout"): False,
        ("Storage", "cache_root"): str(private / "cache"),
        ("Storage", "content_root"): str(private / "content"),
        ("Storage", "profiles_root"): str(private / "profiles"),
        ("Storage", "storage_root"): str(private),
        ("UI", "headless"): True,
        ("Display", "fullscreen"): False,
    }
    found, lines, section = set(), [], ""
    for line in original.splitlines():
        section_line = re.match(r"^\s*\[([^\]]+)\]", line)
        if section_line:
            section = section_line.group(1)
        setting = re.match(r"^\s*([A-Za-z0-9_]+)\s*=", line)
        key = (section, setting.group(1)) if setting else None
        if key in settings:
            assert key not in found
            found.add(key)
            line = f"{key[1]} = {json.dumps(settings[key])}"
        lines.append(line)
    assert found == settings.keys(), sorted(settings.keys() - found)
    text = "\n".join(lines) + "\n"
    parsed = tomllib.loads(text)
    assert all(parsed[section][name] == value for (section, name), value in settings.items())
    return text

def ignore_game_files(directory, names):
    skipped = []
    for name in names:
        path = pathlib.Path(directory) / name
        if path.is_symlink() or path.is_junction():
            raise RuntimeError(f"Unexpected link in source game data: {path}")
        if path.suffix.lower() in {".xex", ".xdb", ".exe", ".pdb", ".pe", ".map"}:
            skipped.append(name)
    return skipped

def copy_game_assets(run):
    run.mkdir(exist_ok=False)
    game = run / "game"
    shutil.copytree(SOURCE_GAME, game, ignore=ignore_game_files)
    inventory = json.loads((HERE / "ASSET_INVENTORY.json").read_text(encoding="utf-8"))
    count = 0
    size = 0
    for directory, _, files in os.walk(game):
        for file in files:
            count += 1
            size += (pathlib.Path(directory) / file).stat().st_size
    assert (count, size) == (inventory["files_to_copy"], inventory["bytes_to_copy"])
    write(run / "assets.json", {"game": str(game), "origin": str(SOURCE_GAME),
                               "files": count, "bytes": size})

def stage(result_path, run, *, reference=False):
    if reference:
        image = result_path.resolve()
        assert image == (SOURCE_GAME / "BURNOUT_X360_ARTIST.XEX").resolve() and xex_magic(image)
        provenance = {"reference_image": str(image), "reference_image_sha256": sha(image)}
    else:
        result = json.loads(result_path.read_text(encoding="utf-8"))
        assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
        outputs = {pathlib.Path(name): row for name, row in result["outputs"].items()}
        images = [path for path in outputs if path.suffix.lower() in {".exe", ".xex"} and path.is_file()
                  and xex_magic(path)]
        assert len(images) == 1, images
        image, = images
        assert image.resolve().parent == result_path.resolve().parent
        assert sha(image) == outputs[image]["sha256"]
        provenance = {"strict_link_result": str(result_path.resolve()),
                      "strict_link_result_sha256": sha(result_path)}
    game = run / "game"
    if run.exists():
        assets = json.loads((run / "assets.json").read_text(encoding="utf-8"))
        assert assets["game"] == str(game) and assets["origin"] == str(SOURCE_GAME)
        assert game.is_dir() and not game.is_junction() and not game.is_symlink()
        assert not any(path.suffix.lower() in {".xex", ".exe"} for path in game.iterdir())
    else:
        copy_game_assets(run)
    emulator = run / "emulator"
    emulator.mkdir()
    private = emulator
    staged_xex = game / "native-game.xex"
    shutil.copy2(image, staged_xex)
    assert sha(staged_xex) == sha(image)
    emulator_exe = emulator / "xenia_burnout5.exe"
    installed_exe = XENIA / emulator_exe.name
    installed_sha = sha(installed_exe)
    snapshot_exe = HERE / "run-r45" / "emulator" / emulator_exe.name
    if (snapshot_exe.is_file() and snapshot_exe.resolve() == snapshot_exe
            and snapshot_exe.stat().st_nlink < 950 and sha(snapshot_exe) == installed_sha):
        os.link(snapshot_exe, emulator_exe)
    else:
        shutil.copy2(installed_exe, emulator_exe)
    assert sha(emulator_exe) == installed_sha
    for name in ("content", "profiles", "cache"):
        (private / name).mkdir()
    (emulator / "portable.txt").touch(exist_ok=False)
    (run / "funcdata").mkdir()
    log = run / "xenia.log"
    config = emulator / CONFIG_NAME
    config.write_text(config_text((XENIA / CONFIG_NAME).read_text(encoding="utf-8-sig"), private, log),
                      encoding="utf-8")
    receipt = {
        **provenance,
        "image": str(staged_xex),
        "image_sha256": sha(staged_xex),
        "emulator": str(emulator_exe),
        "emulator_sha256": sha(emulator_exe),
        "config": str(config),
        "config_sha256": sha(config),
        "game_data_origin": str(SOURCE_GAME),
        "xenia_log": str(log),
        "emulator_default_log": str(emulator / "xenia.log"),
        "function_trace_dir": str(run / "funcdata"),
        "private_storage_root": str(private),
        "command": [str(emulator_exe), str(staged_xex)],
    }
    write(run / "staged.json", receipt)
    return receipt

def launch(run, receipt, timeout):
    child = subprocess.Popen(receipt["command"], cwd=receipt["private_storage_root"],
                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             creationflags=subprocess.CREATE_NO_WINDOW)
    start = time.monotonic()
    timed_out = False
    try:
        write(run / "started.json", {"pid": child.pid, "command": receipt["command"]})
        child.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
    finally:
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
    report = {"pid": child.pid, "returncode": child.returncode, "controlled_timeout": timed_out,
              "wall_seconds": time.monotonic() - start, "logs": {}}
    for name in ("xenia.log", "emulator/xenia.log"):
        path = run / name
        if path.is_file():
            report["logs"][name] = {"sha256": sha(path), "size": path.stat().st_size}
    report["trace_files"] = {path.name: path.stat().st_size
                             for path in sorted((run / "funcdata").iterdir()) if path.is_file()}
    write(run / "process-result.json", report)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--link-result", required=True, type=pathlib.Path)
    parser.add_argument("--run", required=True)
    parser.add_argument("--seconds", type=int, default=45)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    assert re.fullmatch(r"run-[A-Za-z0-9-]+", args.run), args.run
    assert 10 <= args.seconds <= 180
    run = HERE / args.run
    receipt = stage(args.link_result, run)
    if not args.prepare_only:
        launch(run, receipt, args.seconds)

if __name__ == "__main__":
    main()
