import hashlib
import json
import os
from pathlib import Path
import subprocess
import tomllib


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "4f7e93e8dec607b337288eea8d1706cf3ed86198"
head = "c941450b7f2a8dd0b2bc31de0823574c33b94a85"
tree = "7f1ff7ab45b821856ad58a76e1e7aae7d3a39fe8"
source = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO_InputBuffer_Getters.cpp"
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
git("-c", "core.whitespace=cr-at-eol", "diff", "--check", base, head)
old, new = entry(base), entry(head)
assert old and new and old != new

receipt = pin(wave / "r116-traffic-active-clear-terra/RECEIPT.json")
assert receipt["sha256"] == "1cc329ac3f7480c7b47d363d82bbc0b39d37b7db9754dd0f436cec82018d07ad"
receipts = [receipt, verify_preflight("r117-traffic-clear-exact-preflight", [source],
    "f84ae7316616cc11881422da4de6ab4946cb1f267231bbfb0e8fe51567d5e175", "r115-link"),
    verify_preflight("r117-traffic-clear-diagnostic-preflight", [source, diagnostic],
    "74992164f90717627aaf0887a5f481c045bdf74c9752e77044e21b493ae66f63", "r115-link")]
before = verify_run("run-r116-active-car-caller",
    "3136c50018dfe3e81b14f95f6f45f56a701f7982d6ecb002b907a4ab65938b7f")
after = verify_run("run-r117-traffic-clear",
    "74992164f90717627aaf0887a5f481c045bdf74c9752e77044e21b493ae66f63")
receipts.extend(before + after)
old_log = Path(before[-1]["path"]).read_text(errors="replace")
new_log = Path(after[-1]["path"]).read_text(errors="replace")
assert "R116_INVALID_ACTIVE_CAR caller=826EA2E4" in old_log
assert "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" in old_log
assert "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" not in new_log
assert all(marker in new_log for marker in ("R112_SCRIPTED_TITLE_SELECT", "R102_LEGAL_STAGE stage=9 ",
    "WORLDTEX.BIN", "GLOBALPROPS.BIN", "Already locked for write"))

patch = here / "r117.patch"
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
    "schema": "r117-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710,
    "final_paths": [{"path": source, "old": old, "new": new}],
    "groups": [{"name": "native-traffic-race-car-input-clear", "base": base, "head": head,
                "paths": [source], "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r115-intake/completion.json"),
    "base_link": pin(wave / "r115-link/result.json"), "base_seal": pin(wave / "r115-link/seal.json"),
    "base_diagnostics": pin(wave / "r115-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r115-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 trace. R116 and R117 are private no-trace diagnostics.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
