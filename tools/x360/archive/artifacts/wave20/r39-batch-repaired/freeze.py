import hashlib
import json
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
WORK = pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "73dfafe5d9693359d462b07ceb216e9f38aae538"


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
    record = git("ls-files", "--stage", "--", path, env=environment).decode().split()
    if not record:
        return None
    mode, oid, *_ = record
    return f"{mode} blob {oid}"


def make_candidate(name, first_receipt, *extra_receipts):
    first = read(first_receipt)
    last = read(extra_receipts[-1]) if extra_receipts else first
    return name, first["base"], last["head"], (first_receipt, *extra_receipts)



audio = WAVE / "r38-ealayer3-root/DELIVERY.json"
ai = WAVE.parent / "wave36/ai-debug-render-terra/RECEIPT-complete.json"
addr = WAVE.parent / "wave37/sockaddr-parse-terra/DELIVERY.json"
tcp = WAVE.parent / "wave37/commtcp-native-terra/receipt.json"
srp = WAVE.parent / "wave37/commsrp-native-terra/receipt.txt"
srp_provenance = dict(line.split("=", 1) for line in srp.read_text().splitlines())

groups = [
    make_candidate("ai-debug-render-world", ai, ai.with_name("ROUTE_CORNER_FIX_RECEIPT.json")),
    make_candidate("ea-layer3-core-allocation", audio),
    make_candidate("dirtysdk-sockaddr-parse", addr),
    ("dirtysdk-commtcp", read(tcp)["base"], read(tcp)["candidate"], (tcp,)),
    ("dirtysdk-commsrp", srp_provenance["parent"], read(HERE / "SRP_REPAIR_BINDING.json")["head"], (srp, HERE / "SRP_REPAIR_BINDING.json")),
]
assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD").decode().strip() == BASE
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
        expected = tree_entry(group_base, path) or None
        assert (expected is None or expected.startswith("100644 blob ")) and index_entry(path, environment) == expected, (name, path)
    patch = HERE / (name + ".patch")
    with patch.open("xb") as stream:
        stream.write(git("diff", "--binary", "--full-index", group_base, head, "--", *paths))
    git("apply", "--cached", "--check", str(patch), env=environment)
    git("apply", "--cached", str(patch), env=environment)
    assert all(tree_entry(head, path) == index_entry(path, environment) for path in paths)
    manifest_groups.append({"name": name, "base": group_base, "head": head,
                            "paths": paths, "patch": pin(patch), "receipts": [pin(r) for r in receipts]})

projected = git("write-tree", env=environment).decode().strip()
final_paths = [{"path": path, "old": tree_entry(BASE, path) or None, "new": index_entry(path, environment)}
               for path in sorted(seen)]

assert len(final_paths) == 14 and all(row["new"] and row["old"] != row["new"] for row in final_paths)
assert {row["path"] for row in final_paths if row["old"] is None} == {
    "vendor/dirtysdk/src/x360/commtcp_native_x360.cpp",
    "vendor/dirtysdk/src/x360/commsrp_native_x360.cpp",
    "vendor/renderware/src/rw/audio/core/EaLayer3Dec.cpp",
    "vendor/renderware/src/rw/audio/core/EaLayer3Tables.inl",
}
patch = HERE / "r39.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--cached", "--binary", "--full-index", BASE, "--", *sorted(seen), env=environment))
manifest = HERE / "MANIFEST.json"
write(manifest, {
    "schema": "r39-native-intake-v1", "base": BASE, "base_tree": base_tree, "projected_tree": projected,
    "groups": manifest_groups, "patch": pin(patch), "final_paths": final_paths, "expected_total_native": 3698,
    "base_compile": pin(WAVE / "r38-intake/completion.json"), "base_link": pin(WAVE / "r38-link/result.json"),
    "base_seal": pin(WAVE / "r38-link/seal.json"), "base_diagnostics": pin(WAVE / "r38-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(WAVE / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(WAVE / "r38-intake/loading-coff-verification.json"),
})
print(pin(manifest), pin(patch), projected)
