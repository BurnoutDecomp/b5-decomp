import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "a749ce59b26d483dfed04ce0701e854ae4aa8bb4"
head = "4d20f7ee80157e046ad13df394f61633187d8302"
projected = "52735fc332df641e88a9fe51cf0a860ad98901a2"
paths = [
    "src/GameShared/GameClasses/Sound/CgsTestBedAllocator.cpp",
    "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.cpp",
    "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h",
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

patch = here / "r78.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

result = wave / "r78-allocator-sanity-preflight/result.json"
receipts = [
    pin(wave.parent / "wave76/collection-init-terra/RECEIPT.json"),
    pin(wave.parent / "wave77/testbed-sanity-terra/RECEIPT.json"),
    pin(result),
]
assert receipts[0]["sha256"] == "99091dab4ccb43e7de299302f288974629d169e954192d2ce2f90d8d26ee6e8d"
assert receipts[1]["sha256"] == "20b1b5c09a71a913d358c80012c5ddfd41b10ce71f941c311a2fc990260aa0e0"
tested = json.loads(result.read_text(encoding="utf-8"))
assert tested["all_post_checks_pass"] and any(
    row["sha256"] == "217a0c03fa7bc96facad89b16dd1cd92032df723302edff833bbd8a231f3cac4"
    for row in tested["outputs"].values()
)
manifest = here / "MANIFEST.json"
data = {
    "schema": "r78-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-collection-and-sound-guard-init", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r76-intake/completion.json"),
    "base_link": pin(wave / "r76-link/result.json"), "base_seal": pin(wave / "r76-link/seal.json"),
    "base_diagnostics": pin(wave / "r76-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r76-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r76-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
