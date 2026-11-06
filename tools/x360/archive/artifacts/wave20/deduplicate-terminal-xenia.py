"""Coalesce verified, immutable files from completed private Xenia runs."""
from functools import cache
import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import time


here = Path(__file__).resolve().parent
root = here / "native-xenia-smoke"
reference = root / "run-r45"
game = reference / "game"
emulator = reference / "emulator/xenia_burnout5.exe"
game_copies = (
    "run-r41", "run-r42", "run-r43", "run-r44", "run-r45-alt-xenia",
    "run-r45-no-trace", "run-r45-precompiled", "run-r45-queue-diagnostic",
    "run-r46-iterator-preflight",
)
debugger_only = "--retired-debugger" in sys.argv
if debugger_only:
    game_copies = ("run-r45-gdb", "run-r45-rsp", "run-r45-rsp2", "run-r45-rsp3", "run-r45-rsp4")

skipped_suffixes = {".xex", ".xdb", ".exe", ".pdb", ".pe", ".map"}
active_pids = {int(arg) for arg in sys.argv[1:] if arg.isdecimal()}
digest_cache = {}
errors = []


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def path_info(path):
    info = os.stat(path, follow_symlinks=False)
    if info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise RuntimeError(f"Reparse point in experiment: {path}")
    return info


@cache
def validate_parent(directory):
    assert directory.is_relative_to(root) and directory.resolve(strict=True) == directory
    assert stat.S_ISDIR(path_info(directory).st_mode)


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def ref_sha(path, info):
    key = (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
    if key not in digest_cache:
        digest_cache[key] = sha(path)
    return digest_cache[key]


def coalesce(source, target, expected_sha=None):
    validate_parent(source.parent)
    validate_parent(target.parent)
    original, duplicate = path_info(source), path_info(target)
    if not all(stat.S_ISREG(info.st_mode) for info in (original, duplicate)):
        return 0
    if original.st_dev != duplicate.st_dev or original.st_size != duplicate.st_size:
        return 0
    if duplicate.st_nlink != 1 or original.st_ino == duplicate.st_ino or original.st_nlink >= 950:
        return 0
    if original.st_file_attributes != duplicate.st_file_attributes:
        return 0
    source_sha = ref_sha(source, original)
    if expected_sha and source_sha != expected_sha:
        raise RuntimeError(f"Reference digest differs from receipt: {source}")
    if source_sha != sha(target):
        if expected_sha:
            raise RuntimeError(f"Duplicate digest differs from receipt: {target}")
        return 0
    verified = path_info(target)
    if (verified.st_ino, verified.st_size, verified.st_mtime_ns) != (
        duplicate.st_ino, duplicate.st_size, duplicate.st_mtime_ns
    ):
        raise RuntimeError(f"Duplicate changed during hashing: {target}")
    temporary = target.parent / (".bp360-relink-" + str(os.getpid()) + ".tmp")
    os.link(source, temporary)
    try:
        os.replace(temporary, target)
    finally:
        if temporary.exists():
            temporary.unlink()
    assert os.path.samefile(source, target)
    return duplicate.st_size


def report_error(path, error):
    errors.append({"path": str(path), "error": str(error)})


assert root.resolve(strict=True) == root
validate_parent(reference)
validate_parent(game)
assert read(reference / "assets.json")["game"] == str(game)
ref_assets = read(reference / "assets.json")
asset_list = []
for folder, dirs, files in os.walk(game, followlinks=False):
    parent = Path(folder)
    validate_parent(parent)
    dirs[:] = [name for name in dirs if not (parent / name).is_symlink() and not (parent / name).is_junction()]
    for name in files:
        source = parent / name
        if source.suffix.lower() not in skipped_suffixes:
            if stat.S_ISREG(path_info(source).st_mode):
                asset_list.append((source, source.relative_to(game)))
assert len(asset_list) == ref_assets["files"]
assert sum(path_info(source).st_size for source, _ in asset_list) == ref_assets["bytes"]

start = time.monotonic()
asset_summaries = []
for name in game_copies:
    run = root / name
    validate_parent(run)
    validate_parent(run / "game")
    done = read(run / ("started.json" if debugger_only else "process-result.json"))
    assets = read(run / "assets.json")
    if done["pid"] in active_pids:
        continue
    assert assets["game"] == str(run / "game")
    assert all(assets[key] == ref_assets[key] for key in ("origin", "files", "bytes"))
    count, saved = 0, 0
    for source, relative in asset_list:
        target = run / "game" / relative
        try:
            reclaimed = coalesce(source, target)
            if reclaimed:
                count += 1
                saved += reclaimed
        except (OSError, RuntimeError) as error:
            report_error(target, error)
    row = {"run": name, "relinked": count, "duplicate_logical_bytes": saved}
    asset_summaries.append(row)
    print(json.dumps(row), flush=True)

emulator_sha = read(reference / "staged.json")["emulator_sha256"]
assert sha(emulator) == emulator_sha
log_count = log_bytes = exe_count = exe_bytes = 0
for run in (() if debugger_only else sorted(root.iterdir())):
    terminal = run / "process-result.json"
    if not terminal.is_file() or not (run / "staged.json").is_file():
        continue
    validate_parent(run)
    result = read(terminal)
    if result["pid"] in active_pids:
        continue
    logs = result.get("logs", {})
    host_log = logs.get("host-output.log")
    if host_log and host_log == logs.get("emulator/xenia.log"):
        try:
            saved = coalesce(run / "emulator/xenia.log", run / "host-output.log", host_log["sha256"])
            log_count += bool(saved)
            log_bytes += saved
        except (OSError, RuntimeError) as error:
            report_error(run / "host-output.log", error)
    staged = read(run / "staged.json")
    copied = run / "emulator/xenia_burnout5.exe"
    if run != reference and staged.get("emulator_sha256") == emulator_sha and staged.get("emulator") == str(copied):
        try:
            saved = coalesce(emulator, copied, emulator_sha)
            exe_count += bool(saved)
            exe_bytes += saved
        except (OSError, RuntimeError) as error:
            report_error(copied, error)

summary = {
    "assets": asset_summaries, "logs_relinked": log_count, "log_duplicate_logical_bytes": log_bytes,
    "emulators_relinked": exe_count, "emulator_duplicate_logical_bytes": exe_bytes,
    "total_duplicate_logical_bytes": sum(row["duplicate_logical_bytes"] for row in asset_summaries) + log_bytes + exe_bytes,
    "errors": errors, "wall_seconds": time.monotonic() - start,
}
result_name = "deduplicate-debugger-xenia-result.json" if debugger_only else "deduplicate-terminal-xenia-result.json"
with (here / result_name).open("x", encoding="utf-8") as output:
    json.dump(summary, output, indent=2)
    output.write("\n")
print(json.dumps({k: v for k, v in summary.items() if k not in ("assets", "errors")}), flush=True)
print("errors", len(errors), flush=True)
