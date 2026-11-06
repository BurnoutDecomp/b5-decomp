import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "b13d59383a84c55572fe35990ba1a4109b2d3279"
head = "0d00977719e63d71bb6af4c8467b6d2970d84194"
private_head = "ee3bee596569a7887cb4d784e8f3ef414c72db35"
paths = [
    "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.cpp",
    "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h",
    "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp",
    "src/GameSource/Gui/Flapt/BrnFlaptManager.cpp",
]


def git(*arguments, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *arguments], cwd=work, env=env)


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(commit, path):
    return git("ls-tree", commit, "--", path).decode().split("\t")[0].strip()


def tree(commit):
    return git("rev-parse", commit + "^{tree}").decode().strip()


assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD").decode().strip() == head
assert git("merge-base", base, head).decode().strip() == base
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
projected = tree(head)
assert projected == tree(private_head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

patch = here / "r64.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

receipt_paths = [
    "wave61/gui-out-event-dispatch-terra/RECEIPT.json",
    "wave62/view-queue-advance-terra/RECEIPT.json",
    "wave63/flapt-monitor-terra/RECEIPT.json",
]
receipts = [pin(wave.parent / path) for path in receipt_paths]
assert [receipt["sha256"] for receipt in receipts] == [
    "602f37842fec423ebeaf5d9bb3ec0d5354aa8f18bbea3b62cf005f5c6485bd3b",
    "97d97c7810beffb9e99e5a7881b2766545725ea7395f9b86879511a0f4a307b1",
    "85fc2b200069f08d6a824582cf28dc3718e798eb38eabace42f804a989255168",
]
manifest = here / "MANIFEST.json"
data = {
    "schema": "r64-native-intake-v1", "base": base,
    "base_tree": tree(base), "projected_tree": projected,
    "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "gui-event-queues-and-flapt-monitors", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r61-intake/completion.json"),
    "base_link": pin(wave / "r61-link/result.json"), "base_seal": pin(wave / "r61-link/seal.json"),
    "base_diagnostics": pin(wave / "r61-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r61-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r61-native/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
