import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "14fd35595e2a27dd63c4d756a9f4910a42357ec8"
head = "eb74c85173898fb68d236aa3a7d89036c1fc4aa3"
projected = "b1ac4f2e1a983bfd1e80547137f56ac231069c07"
paths = [
    "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp",
    "src/GameSource/Gui/BrnGuiCache.cpp",
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

result = wave / "r109-title-clock-preflight/result.json"
tested = json.loads(result.read_text(encoding="utf-8"))
assert tested["all_post_checks_pass"]
assert sorted(row["source"] for row in tested["preflight_replacements"]) == paths
assert any(row["sha256"] == "4810014044000b10532369eeddbf067f68f0112ba04c0c11f5c852604daacdc9"
           for row in tested["outputs"].values())
receipts = [pin(result), pin(wave / "r109-title-clock-diagnostic-preflight/result.json")]
for directory, expected in (
    ("r107-font-state-terra", "561780aef6aa5ecadc0dbc3904502a343903469358039734c5b477daede13b56"),
    ("r108-gui-time-event-terra", "bc8e7736dbf5bf514ac0fe65f7d06e29e3734519cd3b769bbdd07c67facf6c30"),
):
    receipt = pin(wave / directory / "RECEIPT.json")
    assert receipt["sha256"] == expected
    receipts.append(receipt)
for run_name, sha in (
    ("run-r107-title-access-diagnostic", "c8d2bca9e56dd712dcdde04c46b3a962cf8221bb51641d8cbd44a37c0d3ae215"),
    ("run-r108-title-font-state-diagnostic", "62a6c7e6482d36759b750ef0385f3737ffdc93689db0428280e6a13232d063f8"),
    ("run-r109-title-clock-diagnostic", "fd48273a8f670795872540598d53ec100c5374b0f0a2c80114d4ea589c8bbf52"),
):
    receipts.extend(verify_run(run_name, sha))
log = Path(receipts[-1]["path"]).read_text(errors="replace")
assert "R109_LEGAL_DWELL" in log and "R102_LEGAL_STAGE stage=4 " in log
assert "lpTextureState!=NULL" not in log

patch = here / "r110.patch"
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
    "schema": "r110-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-title-font-state-and-clock", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r105-intake/completion.json"),
    "base_link": pin(wave / "r105-link/result.json"), "base_seal": pin(wave / "r105-link/seal.json"),
    "base_diagnostics": pin(wave / "r105-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r105-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
