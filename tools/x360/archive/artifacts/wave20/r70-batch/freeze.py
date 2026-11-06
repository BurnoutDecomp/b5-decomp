import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "0d11b95753abc0e2cec069155c8993ea9cd84315"
head = "73eba87c06d955f40cfc1b08f7fd091de61c1678"
projected = "73c35384ae153e3627e8e6026707a9e3bd019c01"
paths = [
    "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.cpp",
    "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHandler.h",
    "src/GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.cpp",
    "src/GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h",
    "src/GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.cpp",
    "src/GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h",
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

patch = here / "r70.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

receipts = [
    pin(wave.parent / "wave67/flapt-dispatch-terra/RECEIPT.json"),
    pin(wave.parent / "wave68/apt-data-init-terra/RECEIPT.json"),
    pin(wave.parent / "wave69/always-queue-terra/RECEIPT.json"),
]
assert [row["sha256"] for row in receipts] == [
    "e07bbc4e9a7fd12ebed0caeecc20186b73c76422712f4cb93604a74265aa00e3",
    "7a8de55d58fa7f35218cf489ceee814e0ff438ac7c9fff1cae9f480cb43a16a8",
    "37a2d58cf3279e22a0e4c95916c25ecf76bb7d83ad54e272bacfe5aec887ba3a",
]
manifest = here / "MANIFEST.json"
data = {
    "schema": "r70-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "flapt-apt-and-always-available-gui", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r67-intake/completion.json"),
    "base_link": pin(wave / "r67-link/result.json"), "base_seal": pin(wave / "r67-link/seal.json"),
    "base_diagnostics": pin(wave / "r67-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r67-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r67-native/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
