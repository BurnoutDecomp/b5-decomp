import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "5d76c160d11665b499d50c1c80bde1eaedf64b0a"
head = "4c8b0d8b9ce9e4ce499f7da9dc45655193b6e3f0"
projected = "c86e818c48b4dd1feb73202da7f2db4388f1b08c"
paths = [
    "src/GameSource/Gui/BrnGuiModule.cpp",
    "src/GameSource/Gui/Flow/HUD/States/BrnBootPreload.cpp",
]


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(revision, path):
    return git("ls-tree", revision, "--", path).decode().split("\t")[0].strip()


assert git("merge-base", base, head).decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == projected
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

patch = here / "r81.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

result = wave / "r81-gui-preload-preflight/result.json"
receipts = [
    pin(wave.parent / "wave79/gui-preload-terra/RECEIPT.json"),
    pin(result),
]
assert receipts[0]["sha256"] == "3df9030c92fb46982b4bd9c0a3181a38f197c8b132123f5ea8210963ef2b0875"
tested = json.loads(result.read_text(encoding="utf-8"))
assert tested["all_post_checks_pass"] and any(
    row["sha256"] == "85a7be74d292d5c12f9e4a8bd3307bdb51ee140099a41facec741c73f1f8ec3e"
    for row in tested["outputs"].values()
)
manifest = here / "MANIFEST.json"
data = {
    "schema": "r81-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-gui-flow-and-preload", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r78-intake/completion.json"),
    "base_link": pin(wave / "r78-link/result.json"), "base_seal": pin(wave / "r78-link/seal.json"),
    "base_diagnostics": pin(wave / "r78-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r78-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r78-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
