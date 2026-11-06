import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "942d30cafa848a040cc68dd9de363ab9c143b8de"
head = "1d8f586ef262b7c271f229422768dcb94209d3a5"
projected = "e8ee0a264b8dd7c82c837e69607350454a3ea35f"
cache = "src/GameSource/Gui/BrnGuiCache.cpp"
paths = [
    "src/GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.cpp",
    "src/GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h",
    cache,
]


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(revision, path):
    return git("ls-tree", revision, "--", path).decode().split("\t")[0].strip()


def verify_run(run_name, image_sha):
    run = wave / "native-xenia-smoke" / run_name
    staged = json.loads((run / "staged.json").read_text())
    process = json.loads((run / "process-result.json").read_text())
    log = run / "emulator/xenia.log"
    assert staged["image_sha256"] == image_sha
    assert not process["trace_files"]
    assert pin(log)["sha256"] == process["logs"]["emulator/xenia.log"]["sha256"]
    return [pin(run / "staged.json"), pin(run / "process-result.json"), pin(log)]


assert git("merge-base", base, head).decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == projected
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

result = wave / "r100-audio-stream-init-preflight/result.json"
tested = json.loads(result.read_text(encoding="utf-8"))
image, = (row for name, row in tested["outputs"].items() if name.endswith("native-game.exe"))
assert tested["all_post_checks_pass"] and len(tested["preflight_replacements"]) == 98
command = wave / "r99-audio-stream-init-terra/command.json"
assert pin(command)["sha256"] == "6a1dcb412dadbeb845849b792e64092a75d2c7771ebc8d0769ee356892f98346"
expected_sources = {path.replace("\\", "/") for path in json.loads(command.read_text())["selected"]} | {cache}
assert {row["source"] for row in tested["preflight_replacements"]} == expected_sources
receipts = [pin(result), pin(command)]
for directory, expected in (
    ("r97-guicache-race-index-terra", "17293f9ec7ec05d9d64faac84c572b6fa09f29ab2ff01bb9acb523988e8095a7"),
    ("r99-audio-stream-init-terra", "eb1f30bcfde004236a691abacded4a23143862843b68032cf63421454f066896"),
):
    receipt = pin(wave / directory / "RECEIPT.json")
    assert receipt["sha256"] == expected
    receipts.append(receipt)
for run_name, sha in (
    ("run-r98-gui-car-index-baseline", "13c024c493c6e3f67b1c16a119bf6ce40626a7d60ed8a4f7e827b26826360f8d"),
    ("run-r98-gui-car-index-fixed", "6b618f1d5f977f013c4962ddf3f0ccb0dd9ab97a16418c7471209c5aecda1aa7"),
    ("run-r99-video-stream-diagnostic", "23e6baf9695d62a1c03d9aea376679b100218e032aae8a7821691d82c1be0a71"),
    ("run-r100-audio-stream-init-preflight", image["sha256"]),
):
    receipts.extend(verify_run(run_name, sha))

patch = here / "r100.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

manifest = here / "MANIFEST.json"
data = {
    "schema": "r100-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-boot-gui-and-audio-initialization", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r96-intake/completion.json"),
    "base_link": pin(wave / "r96-link/result.json"), "base_seal": pin(wave / "r96-link/seal.json"),
    "base_diagnostics": pin(wave / "r96-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r96-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r96-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected, image)
