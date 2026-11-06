import hashlib
import json
import os
from pathlib import Path
import subprocess
import tomllib


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r41-voip-send")
base = "06f8bd729e0c0586d9dbfb47581aac33174f493f"
head = "a53bcd14ec8eaa1999e1a875816cf3ca57d0ad56"
tree = "9ed23be4731468ce22e99dcc38a3a41319849053"
source = "src/GameShared/GameClasses/SceneManager/CgsVolumeStore.cpp"
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


def verify_preflight(directory, sources, image):
    path = wave / directory / "result.json"
    result = read(path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert result["base_result_sha256"] == pin(wave / "r110-link/result.json")["sha256"]
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
    assert config["UI"]["headless"] == (name != "run-r110-published-windowed")
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

receipt = pin(wave / "r112-volume-type-terra/RECEIPT.json")
assert receipt["sha256"] == "4f3180a3829978bbb26b113ef1515e05908cc91218db5d723487822d80640ef7"
receipts = [receipt, verify_preflight("r113-volume-exact-preflight", [source],
    "817d6a67e67ca3fdc90e13c7015fd93cdba12b7c26912ffebca1e985c80e4ad4"),
    verify_preflight("r113-volume-diagnostic-preflight", [source, diagnostic],
    "a769734f4e07279bc87996afae6ffc47573eb15f02c713bf2689636838abed58")]
receipts.extend(verify_run("run-r110-published-windowed",
    "6e77c049e6e3da21fcd5263550ec2d886d1d1723fa0044560ddb9ee6a763b312"))
before = verify_run("run-r112-scripted-title-select",
    "0f0271d5e44688826d4d6280662ca9ef2efde8e1dd885c7b686ffe4fd8cea3c7")
after = verify_run("run-r113-volume-diagnostic",
    "a769734f4e07279bc87996afae6ffc47573eb15f02c713bf2689636838abed58")
receipts.extend(before + after)
old_log = Path(before[-1]["path"]).read_text(errors="replace")
new_log = Path(after[-1]["path"]).read_text(errors="replace")
assert "Aggregate volumes are not supported by the scene manager" in old_log
assert "Aggregate volumes are not supported by the scene manager" not in new_log
assert all(marker in new_log for marker in ("R112_SCRIPTED_TITLE_SELECT", "R102_LEGAL_STAGE stage=9 ",
                                            "JUNKYARDLIGHTING.DAT", "VEHICLETEX.BIN"))

patch = here / "r113.patch"
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
    "schema": "r113-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710,
    "final_paths": [{"path": source, "old": old, "new": new}],
    "groups": [{"name": "native-collision-volume-type", "base": base, "head": head,
                "paths": [source], "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r110-intake/completion.json"),
    "base_link": pin(wave / "r110-link/result.json"), "base_seal": pin(wave / "r110-link/seal.json"),
    "base_diagnostics": pin(wave / "r110-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r110-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 trace. R110 and R113 runtime receipts are no-trace.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
