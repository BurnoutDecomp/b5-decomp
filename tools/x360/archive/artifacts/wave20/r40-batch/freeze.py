import hashlib
import json
import os
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
artifacts = wave.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "5977a1e0eabad82a7dbd0620601ca3f545d08bbc"
head = "498ac5ecbc1fe02ccce101125f8886884dada6aa"
tree = "44e3f45ca94c915cad77e01ac56bbd9161d6c3af"

def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)

def pin(path):
    path = pathlib.Path(path)
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

def tree_entry(commit, path):
    return git("ls-tree", commit, "--", path).decode().strip().split("\t")[0] or None

assert git("rev-parse", "HEAD").decode().strip() == head
assert git("rev-parse", "HEAD^{tree}").decode().strip() == tree
assert not git("status", "--porcelain")
assert git("merge-base", base, head).decode().strip() == base

video = "src/SDKs/EATech/rwmovie/CVideoFrameAnalyst.cpp"
lobby = "vendor/dirtysdk/src/x360/lobbylogin.cpp"
paths = set(git("diff", "--name-only", base, head).decode().splitlines())
assert len(paths) == 23 and {video, lobby} <= paths
audio = sorted(paths - {video, lobby})
groups = [
    ("snd9-native-mixer-and-lifetime", audio, [
        wave / "r37-snd9-filters-sol/STAGE_3_COMBINED_RECEIPT.json",
        wave / "r37-snd9-filters-sol/STAGE_4_VOICE_FREE_RECEIPT.json",
        artifacts / "wave37/snd-panning-terra/DELIVERY.json",
        artifacts / "wave39/snd-mix-audio-terra/receipt.json",
    ]),
    ("video-native-texture-analysis", [video], [wave / "r39-video-root/DELIVERY.json"]),
    ("dirtysdk-lobby-native-remaps", [lobby], [artifacts / "wave37/lobby-dispatch-luna/receipt.json"]),
]
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
records = []
for name, group_paths, receipts in groups:
    patch = here / f"{name}.patch"
    with patch.open("xb") as stream:
        stream.write(git("diff", "--binary", "--full-index", base, head, "--", *group_paths))
    git("apply", "--cached", "--check", str(patch), env=env)
    git("apply", "--cached", str(patch), env=env)
    records.append({"name": name, "base": base, "head": head, "paths": group_paths,
                    "patch": pin(patch), "receipts": [pin(receipt) for receipt in receipts]})
assert git("write-tree", env=env).decode().strip() == tree
final_paths = [{"path": path, "old": tree_entry(base, path), "new": tree_entry(head, path)}
               for path in sorted(paths)]
assert all(item["new"] and item["new"] != item["old"] for item in final_paths)
added = [row["path"] for row in final_paths if row["old"] is None]
assert len(added) == 13 and len([p for p in added if p.endswith(".cpp")]) == 8, added
patch = here / "r40.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
manifest = here / "MANIFEST.json"
with manifest.open("x", encoding="utf-8") as stream:
    json.dump({"schema": "r40-native-intake-v1", "base": base,
               "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
               "projected_tree": tree, "private_head": head,
               "groups": records, "patch": pin(patch), "final_paths": final_paths,
               "expected_total_native": 3706,
               "base_compile": pin(wave / "r39-intake-repaired/completion.json"),
               "base_link": pin(wave / "r39-link-repaired/result.json"),
               "base_seal": pin(wave / "r39-link-repaired/seal.json"),
               "base_diagnostics": pin(wave / "r39-link-repaired/diagnostic-parsed.json"),
               "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
               "base_loading_coff": pin(wave / "r39-intake-repaired/loading-coff-verification.json")}, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
