import argparse
import hashlib
import json
import os
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "19ccd1a21737d387ef8732b84541138af324bd9f"

parser = argparse.ArgumentParser()
parser.add_argument("--head", required=True)
parser.add_argument("--receipt", required=True, type=pathlib.Path, action="append")
args = parser.parse_args()

def git(*arguments, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *arguments], cwd=work, env=env)

def pin(path):
    path = pathlib.Path(path)
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}

def entry(commit, path):
    return git("ls-tree", commit, "--", path).decode().split("\t")[0].strip() or None

assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD").decode().strip() == args.head
assert git("merge-base", base, args.head).decode().strip() == base
projected = git("rev-parse", args.head + "^{tree}").decode().strip()
paths = git("diff", "--name-only", base, args.head).decode().splitlines()
assert paths == [
    "src/SDKs/EATech/eathread/BrnEAThreadX360.cpp",
    "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.cpp",
    "src/SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h",
]
assert len(args.receipt) == 3
final = [{"path": path, "old": entry(base, path), "new": entry(projected, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)
patch = here / "r50.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, args.head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected
manifest = here / "MANIFEST.json"
data = {
    "schema": "r50-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base+"^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "vault-zeroing-and-eathread-static-initialization", "base": base, "head": args.head,
                "paths": paths, "patch": pin(patch), "receipts": [pin(p) for p in args.receipt]}],
    "patch": pin(patch),
    "base_compile": pin(wave/"r48-intake/completion.json"),
    "base_link": pin(wave/"r48-link/result.json"),
    "base_seal": pin(wave/"r48-link/seal.json"),
    "base_diagnostics": pin(wave/"r48-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave/"r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave/"r48-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave/"native-xenia-smoke/run-r48-native/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)

