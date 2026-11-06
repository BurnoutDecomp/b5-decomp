import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "cb443f6880e8a0cc01578b014110afc9c9c85ca1"
head = "f453a83d3d82f68dc224f72dc19397d9f342b66e"
projected = "441a429ba77169ba37284ec0ba9222f5209de0a3"
paths = [
    "src/GameSource/Gui/BrnGuiModule.cpp",
    "src/SDKs/EATech/include/Apt/AptCIHText.cpp",
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

result = wave / "r107-title-access-preflight/result.json"
tested = json.loads(result.read_text(encoding="utf-8"))
assert tested["all_post_checks_pass"]
assert sorted(row["source"] for row in tested["preflight_replacements"]) == paths
assert any(row["sha256"] == "4bf6c604e0b1df4cb6cf2b9a381088069d7e9b79b0b51e92e9d4c6649fe955af"
           for row in tested["outputs"].values())
receipts = [pin(result), pin(wave / "r107-title-access-diagnostic-preflight/result.json")]
for directory, expected in (
    ("r104-apt-font-param-terra", "788eab4b8acacd12a6a5f45dc47f3891f70a9212e36d5bbe9f793b274254bb76"),
    ("r104-apt-font-fallback-terra", "e0738a37b1fe9f6e28ae5cd290c2fc0cd0182b9a58bbcaa64756083e4a047e49"),
    ("r106-aptaux-binding-terra", "ec8d30ba7210f045f664b82fc5c3171d679dd06ee07f7bc152b23944d279b0cb"),
):
    receipt = pin(wave / directory / "RECEIPT.json")
    assert receipt["sha256"] == expected
    receipts.append(receipt)
for run_name, sha in (
    ("run-r104-title-member-diagnostic", "65ced8a78544867b56aa645084775826f785926fac9b87c4cdf4e1c5f00d0977"),
    ("run-r106-title-font-diagnostic", "535d1cccb9bdf45fb433016a872a134191a9680f9c6beeebcef71a2217c39606"),
    ("run-r107-title-access-diagnostic", "c8d2bca9e56dd712dcdde04c46b3a962cf8221bb51641d8cbd44a37c0d3ae215"),
):
    receipts.extend(verify_run(run_name, sha))
log = Path(receipts[-1]["path"]).read_text(errors="replace")
assert "Invalid pointer to apt communicator in AptAux::UpdateFlashComponent" not in log
assert any(
    aux == live and aux != "00000000" and communicator != "00000000"
    for aux, live, communicator in re.findall(
        r"R107_TITLE_VIEW aux=([0-9A-F]{8}) live=([0-9A-F]{8}) communicator=([0-9A-F]{8})", log
    )
)

patch = here / "r105.patch"
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
    "schema": "r105-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-title-font-and-apt-binding", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r100-intake/completion.json"),
    "base_link": pin(wave / "r100-link/result.json"), "base_seal": pin(wave / "r100-link/seal.json"),
    "base_diagnostics": pin(wave / "r100-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r100-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r100-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
