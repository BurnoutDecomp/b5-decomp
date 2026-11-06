import hashlib
import json
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
BASE = "dce303140884efdc2237abb06026d32220e9217c"
PRIVATE_BASE = "c7d41f0331d3a78634e69c79d68fabc9d2ebb57f"
HEAD = "0967ded339da36eb921e2f373e2dcd5a56e54c6d"
SOURCE = "src/GameShared/GameClasses/SceneManager/Collision/ContactGenerator/"
PATHS = sorted([SOURCE + "CgsCollisionGenerator.cpp", SOURCE + "CgsCollisionGenerator_LineStream.cpp",
                SOURCE + "CgsCollisionGenerator_CollideStreams.cpp", "src/SDKs/EATech/eajobs/job_scheduler.cpp"])


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=WORK, env=env)


def pin(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


assert git("rev-parse", "HEAD").decode().strip() == HEAD
assert not git("status", "--porcelain")
base_tree = git("rev-parse", BASE + "^{tree}").decode().strip()
assert base_tree == git("rev-parse", PRIVATE_BASE + "^{tree}").decode().strip()
assert git("diff", "--name-only", BASE, HEAD).decode().splitlines() == PATHS
patch = HERE / "r33.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", BASE, HEAD, "--", *PATHS))
index = HERE / "projection.index"
assert not index.exists()
environment = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", BASE, env=environment)
git("apply", "--cached", "--check", str(patch), env=environment)
git("apply", "--cached", str(patch), env=environment)
projected_tree = git("write-tree", env=environment).decode().strip()
assert projected_tree == git("rev-parse", HEAD + "^{tree}").decode().strip()
paths = []
for path in PATHS:
    old = git("ls-tree", BASE, "--", path).decode().strip().split("\t")[0]
    new = git("ls-tree", HEAD, "--", path).decode().strip().split("\t")[0]
    assert old.startswith("100644 blob ") and new.startswith("100644 blob ") and old != new
    paths.append({"path": path, "old": old, "new": new})

receipts = [pin(HERE.parent / group / "DELIVERY.json") for group in
            ("r33-collision-streams-root", "r33-collision-scheduler-root")]
manifest = {
    "schema": "r33-native-intake-v1", "base": BASE, "base_tree": base_tree,
    "projected_tree": projected_tree,
    "groups": [{"name": "collision-native-scheduler", "base": PRIVATE_BASE, "head": HEAD,
                "paths": PATHS, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "final_paths": paths,
    "base_compile": pin(HERE.parent / "r32-intake/completion.json"),
    "base_link": pin(HERE.parent / "r32-link/result.json"),
    "base_seal": pin(HERE.parent / "r32-link/seal.json"),
    "base_diagnostics": pin(HERE.parent / "r32-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(HERE.parent / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(HERE.parent / "r32-intake/loading-coff-verification.json"),
    "active_native_expected": 3695,
}
path = HERE / "MANIFEST.json"
with path.open("x", encoding="utf-8") as stream:
    json.dump(manifest, stream, indent=2)
    stream.write("\n")
print(pin(path), pin(patch), projected_tree)
