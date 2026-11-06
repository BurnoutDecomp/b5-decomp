import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r41-voip-send")
base = "8a7ce0cfdec89a4adb586ac46aa96f717e4c069f"
head = "9a29229e0fb6b3e71965471ee22ef8d04f365912"
projected = "5913f340ced8929a3027d8441e8e0a53046aff23"
paths = [
    "src/GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.cpp",
    "src/GameShared/GameClasses/Language/CgsLanguageManager.cpp",
    "src/GameShared/GameClasses/Language/CgsSku.cpp",
    "src/GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.cpp",
    "src/GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h",
    "src/GameSource/Gui/BrnGuiModule.cpp",
    "src/GameSource/Gui/BrnGuiModule.h",
    "src/GameSource/Gui/Flapt/BrnFlaptManager.cpp",
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

patch = here / "r67.patch"
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
    pin(wave.parent / "wave64/gui-module-dispatch-terra/RECEIPT.json"),
    pin(wave / "r66-sku-language-id-preflight/result.json"),
    pin(wave.parent / "wave66/language-native-terra/RECEIPT.json"),
]
assert [row["sha256"] for row in receipts] == [
    "8df28a51cd05b1fd70d5a8028382d26283e6e477338c8a9e23379b43062b6e0b",
    "375fe8cadfa744e613145c176d85244229253e0a151130468302fe5fa778be2c",
    "ba8fecd049707ba8006f39a41133b807cd7dfe506febfda4e0c22c5b1f834a91",
]
manifest = here / "MANIFEST.json"
data = {
    "schema": "r67-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "gui-queue-and-native-language-resource", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r64-intake/completion.json"),
    "base_link": pin(wave / "r64-link/result.json"), "base_seal": pin(wave / "r64-link/seal.json"),
    "base_diagnostics": pin(wave / "r64-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r64-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r64-native/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
