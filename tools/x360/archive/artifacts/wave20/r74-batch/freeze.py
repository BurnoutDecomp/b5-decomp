import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "a3f0a7e9248a6b0dea1291b613c1503a375a8054"
head = "a66025dec3dcf0890c354f2a07111849e2ce5700"
projected = "6aad35674cb6865bc0cff9b6c6332f472ace9c1d"
paths = [
    "src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp",
    "src/GameSource/Gui/Flapt/BrnFlaptRenderer.cpp",
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

patch = here / "r74.patch"
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
    pin(wave.parent / "wave70/view-black-state-terra/RECEIPT.json"),
    pin(wave.parent / "wave71/view-render-order-terra/RECEIPT.json"),
    pin(wave.parent / "wave72/flapt-view-brackets-terra/RECEIPT.json"),
]
assert [row["sha256"] for row in receipts] == [
    "c4bd95d26115582dc1882eb6522cf0493d422b62bf4be0952729d04e21ce6c3a",
    "4e181abfc1d56ef43dd701beade7e3c6440d7535e0e267e4379dac123b49060f",
    "fc27853e3423b909e7fd4e37a81a75b5153b94ee37260cf46357d44692ea860e",
]
manifest = here / "MANIFEST.json"
data = {
    "schema": "r74-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-view-and-flapt-render-brackets", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r70-intake/completion.json"),
    "base_link": pin(wave / "r70-link/result.json"), "base_seal": pin(wave / "r70-link/seal.json"),
    "base_diagnostics": pin(wave / "r70-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r70-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r70-native/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
