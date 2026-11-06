import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "ba592580ec482586bba7b4aea9fbdd09ab53f823"
head = "2a83a7927901c38f447a17ae8ff47043b0bfe887"
projected = "1b3d707bfce1436115290ca2b64bc63bf1da2090"
paths = [
    f"src/SDKs/EATech/include/Apt/AptActionInterpreter{name}.cpp"
    for name in ("Branch", "Builtins", "SpecialOps", "StackOps")
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
    assert process["controlled_timeout"] and not process["trace_files"]
    assert pin(log)["sha256"] == process["logs"]["emulator/xenia.log"]["sha256"]
    return [pin(run / "staged.json"), pin(run / "process-result.json"), pin(log)]


assert git("merge-base", base, head).decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == projected
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

result = wave / "r97-apt-operands-preflight/result.json"
tested = json.loads(result.read_text(encoding="utf-8"))
exact_sha = "da16490b6039b2c1dad22e8a6baf4bcc6e34125d9f62c08173c2185e1cad9e46"
assert tested["all_post_checks_pass"]
assert sorted(row["source"] for row in tested["preflight_replacements"]) == paths
assert any(row["sha256"] == exact_sha for row in tested["outputs"].values())
receipts = []
for directory, expected in (
    ("r94-apt-register-terra", "47595e2e214d186bf1d9c1fed419a968b230752c03dd4256ab3df478c35a5d1c"),
    ("r95-apt-push-terra", "58058aef8b886ed77786925849d4197c33525c8587894363149fd608f3727629"),
    ("r96-apt-operands-terra", "bf0891bc159470c148d182a5d20fb1513fd0ca5811ae0a3fed60b0e903633743"),
):
    receipt = pin(wave / directory / "RECEIPT.json")
    assert receipt["sha256"] == expected
    receipts.append(receipt)
receipts.extend((pin(result), pin(wave / "r97-apt-operands-diagnostic-preflight/result.json")))
receipts.extend(verify_run("run-r97-apt-operands-preflight", exact_sha))
receipts.extend(verify_run("run-r96-apt-push-diagnostic",
                           "3ab2fcb784c9ad83bf84c303d9051aed007e425d9af38d0f253f403242ba87a5"))
receipts.extend(verify_run("run-r97-apt-operands-diagnostic",
                           "e5e3833aa0dfd529986f11fd48e7a984ef0842896f3f99776d1d8058fa7e9c4b"))

patch = here / "r96.patch"
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
    "schema": "r96-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-apt-action-operands", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r90-intake/completion.json"),
    "base_link": pin(wave / "r90-link/result.json"), "base_seal": pin(wave / "r90-link/seal.json"),
    "base_diagnostics": pin(wave / "r90-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r90-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r90-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
