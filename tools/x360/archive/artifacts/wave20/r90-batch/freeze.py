import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "84632378ed7ff6599413c45b498de6fce2559cd0"
head = "5d81e035f67f598516602e34a0ad7aa598f5b526"
projected = "d77ab6120708c8cf02728773e34989231a9ad46b"
paths = [
    "src/SDKs/EATech/Apt/AptValueGCAllocator.h",
    "src/SDKs/EATech/Apt/AptValueGCPoolManager.cpp",
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


assert git("rev-parse", head + "^").decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == projected
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

result = wave / "r90-apt-gc-preflight/result.json"
tested = json.loads(result.read_text(encoding="utf-8"))
exact_sha = "ab4fa947df4b8ff28727141cb1aaaada8d843a19850420e237f6c22c20b13eb5"
assert tested["all_post_checks_pass"] and len(tested["preflight_replacements"]) == 38
assert any(row["sha256"] == exact_sha for row in tested["outputs"].values())
receipts = [pin(wave / "r89-apt-gc-terra/RECEIPT.json"), pin(result)]
assert receipts[0]["sha256"] == "e3d37e8bf06b6fa25741fcb03388d48c98c08d9e8a78427154bfd79be26c6749"
receipts.extend(verify_run("run-r90-apt-gc-preflight", exact_sha))
receipts.extend(verify_run("run-r89-apt-gc-diagnostic",
                           "951903aa210d6ed0a454600ea96c872a1955585a63539bcb09f06977143947eb"))
receipts.extend(verify_run("run-r90-apt-gc-diagnostic",
                           "ba6cdc94ab119f6020bd6740fed82d18b389f8bdd88d788b0434a09e9473093b"))

patch = here / "r90.patch"
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
    "schema": "r90-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-apt-gc-pool", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r81-intake/completion.json"),
    "base_link": pin(wave / "r81-link/result.json"), "base_seal": pin(wave / "r81-link/seal.json"),
    "base_diagnostics": pin(wave / "r81-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r81-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r81-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
