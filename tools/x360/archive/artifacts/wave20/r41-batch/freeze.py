import argparse
import hashlib
import json
import os
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
root = wave.parent
repo = pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
base = "8e9b44e0058424905069834f4953fe3358ffe492"
base_tree = "eccf35d2f81cc46758a3e5bbd1b8d92e9eff965b"
voip = "vendor/dirtysdk/src/x360/voip_x360.cpp"
audio_receipt = wave / "r41-audio-preflight/DELIVERY.json"
xhv_receipt = root / "wave40/xhv-sdk-terra/receipt.json"

def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=repo, env=env)

def pin(path):
    path = pathlib.Path(path)
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

def write(path, data):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")

def entry(commit, path):
    return git("ls-tree", commit, "--", path).decode().split("\t")[0].strip() or None

parser = argparse.ArgumentParser()
parser.add_argument("--voip-head", required=True)
parser.add_argument("--voip-receipt", required=True, type=pathlib.Path)
args = parser.parse_args()
assert git("rev-parse", "HEAD").decode().strip() == base
assert git("rev-parse", "HEAD^{tree}").decode().strip() == base_tree
assert not git("status", "--porcelain")
audio = json.loads(audio_receipt.read_text())
assert audio["base"] == base
assert pin(audio["patch"]["path"])["sha256"] == audio["patch"]["sha256"]
assert pin(xhv_receipt)["sha256"] == "6f15066376c1cf4dbe35b7830b54f9aa7e7b9ed3111b8ed222b5ff708a03c0fd"
assert not entry(base, voip)
voip_entry = entry(args.voip_head, voip)
assert voip_entry and voip_entry.startswith("100644 blob ")
voip_blob = voip_entry.split()[-1]

index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", audio["patch"]["path"], env=env)
git("apply", "--cached", audio["patch"]["path"], env=env)
git("update-index", "--add", "--cacheinfo", f"100644,{voip_blob},{voip}", env=env)
projected = git("write-tree", env=env).decode().strip()
paths = sorted(set(audio["paths"]) | {voip})
assert len(paths) == 8
final = [{"path": path, "old": entry(base, path), "new": entry(projected, path)} for path in paths]
assert all(row["old"] != row["new"] and row["new"] for row in final)
added = [row["path"] for row in final if not row["old"]]
assert len(added) == 6 and len([path for path in added if path.endswith(".cpp")]) == 4

voip_patch = here / "dirtysdk-voip-xhv-archive.patch"
with voip_patch.open("xb") as stream:
    stream.write(git("diff", "--cached", "--binary", "--full-index", base, "--", voip, env=env))
patch = here / "r41.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--cached", "--binary", "--full-index", base, env=env))
manifest = here / "MANIFEST.json"
write(manifest, {
    "schema": "r41-native-intake-v1", "base": base, "base_tree": base_tree,
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [
        {"name": "snd9-native-service-and-resampler", "base": base, "head": audio["head"],
         "paths": audio["paths"], "patch": audio["patch"], "receipts": [pin(audio_receipt)]},
        {"name": "dirtysdk-voip-xhv-archive", "base": base, "head": args.voip_head,
         "paths": [voip], "patch": pin(voip_patch), "receipts": [pin(args.voip_receipt), pin(xhv_receipt)]},
    ],
    "patch": pin(patch), "base_compile": pin(wave / "r40-intake-repaired/completion.json"),
    "base_link": pin(wave / "r40-link-repaired/result.json"),
    "base_seal": pin(wave / "r40-link-repaired/seal.json"),
    "base_diagnostics": pin(wave / "r40-link-repaired/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r40-intake-repaired/loading-coff-verification.json"),
    "native_archive_addition": {"name": "xhv.lib",
                                "sha256": "d4cff97c603e15bce167559bca2e7d02f6676c8a958f0e66b37e5c873497485e",
                                "audit": pin(xhv_receipt)},
})
print(pin(manifest), pin(patch), projected)
