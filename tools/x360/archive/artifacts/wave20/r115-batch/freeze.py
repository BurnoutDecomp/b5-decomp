import hashlib
import json
import os
from pathlib import Path
import subprocess
import tomllib


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "ec677b917009932e9a60498d31ec1cb1aa231a3f"
head = "bb70bccddc59617553d1e9395b8e468bc9a2003b"
tree = "17c26f49aae7789b032a79dddcb71fd761bfa5c4"
source = "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp"
volume = "src/GameShared/GameClasses/SceneManager/CgsVolumeStore.cpp"
diagnostic = "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp"
emulator_sha = "785e6e5d488c5e516491f0de04fcac01259231f129bb48bd5d48dd2bae537dec"


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(revision):
    return git("ls-tree", revision, "--", source).decode().split("\t")[0].strip()


def verify_preflight(directory, sources, image, baseline):
    path = wave / directory / "result.json"
    result = read(path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert result["base_result_sha256"] == pin(wave / baseline / "result.json")["sha256"]
    replacements = result["preflight_replacements"]
    assert [row["source"] for row in replacements] == sources
    assert replacements[0]["source_sha256"] == pin(work / source)["sha256"]
    executable = wave / directory / "native-game.exe"
    assert result["outputs"][str(executable)]["sha256"] == pin(executable)["sha256"] == image
    return pin(path)


def verify_run(name, image):
    path = wave / "native-xenia-smoke" / name
    staged, started, result = (path / item for item in ("staged.json", "started.json", "process-result.json"))
    receipt = read(staged)
    assert receipt["image_sha256"] == pin(Path(receipt["image"]))["sha256"] == image
    assert receipt["emulator_sha256"] == pin(Path(receipt["emulator"]))["sha256"] == emulator_sha
    process = read(result)
    assert process["pid"] == read(started)["pid"] and not process["trace_files"]
    log = pin(path / "emulator/xenia.log")
    assert log["sha256"] == process["logs"]["emulator/xenia.log"]["sha256"]
    with Path(log["path"]).open(errors="replace") as stream:
        header = "".join(next(stream) for _ in range(300))
    config_dump = header.split("----------- CONFIG DUMP -----------", 1)[1]
    config_dump = config_dump.split("----------- END OF CONFIG DUMP ----", 1)[0]
    config = tomllib.loads(config_dump)
    for key in ("trace_functions", "trace_function_data"):
        assert config["CPU"][key] is False
    for key in ("allow_game_relative_writes", "apply_patches", "allow_plugins"):
        assert config["General"][key] is False
    assert config["UI"]["headless"] is True
    root = Path(receipt["private_storage_root"])
    assert Path(config["Storage"]["storage_root"]) == root
    assert Path(config["Storage"]["profiles_root"]) == root / "profiles"
    assert Path(config["Storage"]["content_root"]) == root / "content"
    return [pin(staged), pin(started), pin(result), log]


assert git("rev-parse", head + "^").decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == tree
assert git("rev-parse", "HEAD").decode().strip() == head and not git("status", "--porcelain")
assert git("diff", "--name-only", base, head).decode().splitlines() == [source]
git("diff", "--check", base, head)
old, new = entry(base), entry(head)
assert old and new and old != new

receipt = pin(wave / "r114-debug2d-native-terra/receipt.txt")
assert receipt["sha256"] == "aef4f534903d9336c0e838d922f7a84cd111bc978da1c66eae407424674f0e1c"
receipts = [receipt, verify_preflight("r115-debug2d-exact-preflight", [source],
    "89f57a10a37f4de3f86f8a92d088836a0b019ffaaac0cf91ee3250a39c76c090", "r113-link"),
    verify_preflight("r115-debug2d-diagnostic-preflight", [source, volume, diagnostic],
    "8756292be5c3ae46faa32a9bb871a548d4856a176312d1fcc91851c27db4dda5", "r110-link")]
before = verify_run("run-r114-native-caller",
    "a945fd94f2b13450c559c774e035ff5b2e4ffc98d1c2678023fc08ec657f100f")
after = verify_run("run-r115-native-debug2d",
    "8756292be5c3ae46faa32a9bb871a548d4856a176312d1fcc91851c27db4dda5")
receipts.extend(before + after)
old_log = Path(before[-1]["path"]).read_text(errors="replace")
new_log = Path(after[-1]["path"]).read_text(errors="replace")
assert "R114_NULL_RENDER_STATE lpBlendState!=NULL caller=821B60F0" in old_log
assert "R114_NULL_RENDER_STATE lpTexture!=NULL caller=821B6100" in old_log
assert all(marker not in new_log for marker in ("lpBlendState!=NULL", "lpTexture!=NULL",
    "Aggregate volumes are not supported by the scene manager"))
assert all(marker in new_log for marker in ("R112_SCRIPTED_TITLE_SELECT", "R102_LEGAL_STAGE stage=9 ",
    "WORLDTEX.BIN", "GLOBALPROPS.BIN", "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0"))

patch = here / "r115.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == tree

manifest = here / "MANIFEST.json"
data = {
    "schema": "r115-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710,
    "final_paths": [{"path": source, "old": old, "new": new}],
    "groups": [{"name": "native-debug2d-render-states", "base": base, "head": head,
                "paths": [source], "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r113-intake/completion.json"),
    "base_link": pin(wave / "r113-link/result.json"), "base_seal": pin(wave / "r113-link/seal.json"),
    "base_diagnostics": pin(wave / "r113-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r113-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 trace. R114 and R115 are private no-trace diagnostics.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
