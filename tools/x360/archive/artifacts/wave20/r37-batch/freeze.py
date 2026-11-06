import hashlib
import json
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
WORK = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
BASE = "84df79ddf8d4ce8e0c31beb4fb30ed29e650cabd"


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=WORK, env=env)


def pin(path):
    return {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path, payload):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2)
        stream.write("\n")


def tree_entry(commit, path):
    return git("ls-tree", commit, "--", path).decode().strip().split("\t")[0]


def index_entry(path, environment):
    mode, oid, *_ = git("ls-files", "--stage", "--", path, env=environment).decode().split()
    return f"{mode} blob {oid}"


def make_candidate(name, first_receipt, *extra_receipts):
    first = read(first_receipt)
    last = read(extra_receipts[-1]) if extra_receipts else first
    return name, first["base"], last["head"], (first_receipt, *extra_receipts)


sound = WAVE / "r37-snd9-services-root/DELIVERY.json"
game = WAVE / "r35-gamestate-events-sol/RECEIPT.json"
groups = [make_candidate("snd9-native-channel-state", sound),
          make_candidate("game-state-event-dispatch", game)]

assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD").decode().strip() == read(sound)["head"]
base_tree = git("rev-parse", BASE + "^{tree}").decode().strip()
index = HERE / "projection.index"
assert not index.exists()
environment = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", BASE, env=environment)
manifest_groups = []
seen = set()
for name, group_base, head, receipts in groups:
    paths = git("diff", "--name-only", group_base, head).decode().splitlines()
    assert paths and not seen.intersection(paths), (name, paths)
    seen.update(paths)
    for path in paths:
        expected = tree_entry(group_base, path)
        assert expected.startswith("100644 blob ") and index_entry(path, environment) == expected, (name, path)
    patch = HERE / (name + ".patch")
    with patch.open("xb") as stream:
        stream.write(git("diff", "--binary", "--full-index", group_base, head, "--", *paths))
    git("apply", "--cached", "--check", str(patch), env=environment)
    git("apply", "--cached", str(patch), env=environment)
    assert all(tree_entry(head, path) == index_entry(path, environment) for path in paths)
    manifest_groups.append({"name": name, "base": group_base, "head": head,
                            "paths": paths, "patch": pin(patch), "receipts": [pin(r) for r in receipts]})

projected = git("write-tree", env=environment).decode().strip()
final_paths = [{"path": path, "old": tree_entry(BASE, path), "new": index_entry(path, environment)}
               for path in sorted(seen)]
assert len(final_paths) == 4 and all(row["old"] != row["new"] for row in final_paths)
patch = HERE / "r37.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--cached", "--binary", "--full-index", BASE, "--", *sorted(seen), env=environment))
manifest = HERE / "MANIFEST.json"
write(manifest, {
    "schema": "r37-native-intake-v1", "base": BASE, "base_tree": base_tree, "projected_tree": projected,
    "groups": manifest_groups, "patch": pin(patch), "final_paths": final_paths, "expected_total_native": 3694,
    "base_compile": pin(WAVE / "r36-intake-repaired/completion.json"), "base_link": pin(WAVE / "r36-link/result.json"),
    "base_seal": pin(WAVE / "r36-link/seal.json"), "base_diagnostics": pin(WAVE / "r36-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(WAVE / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(WAVE / "r36-intake-repaired/loading-coff-verification.json"),
})
print(pin(manifest), pin(patch), projected)
