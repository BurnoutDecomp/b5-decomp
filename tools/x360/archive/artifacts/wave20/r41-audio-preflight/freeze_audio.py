import hashlib
import json
import pathlib
import subprocess

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
repo = pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
base = "8e9b44e0058424905069834f4953fe3358ffe492"
private_base = "25b961832bb7ef4c85a555478eee5e61580564cb"
head = "e04b2e3fb34756b02898b7b88f634d9e488896f4"

def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=repo)

def pin(path):
    path = pathlib.Path(path)
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

assert git("rev-parse", "HEAD").decode().strip() == base
assert not git("status", "--porcelain")
assert git("rev-parse", base+"^{tree}") == git("rev-parse", private_base+"^{tree}")
assert git("rev-parse", head+"^").decode().strip() == private_base
paths = git("diff", "--name-only", base, head).decode().splitlines()
assert len(paths) == 7 and all(path.startswith("src/SDKs/EATech/include/snd/") for path in paths)
patch = here / "r41-audio.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
data = {"base": base, "private_base": private_base, "head": head,
        "tree": git("rev-parse", head+"^{tree}").decode().strip(), "paths": paths,
        "patch": pin(patch), "compile": pin(here/"native/compile-results.json"),
        "boundary": pin(here/"COFF_BOUNDARY.json"),
        "r40_completion": pin(wave/"r40-intake-repaired/completion.json"),
        "r40_link": pin(wave/"r40-link-repaired/result.json"),
        "r40_seal": pin(wave/"r40-link-repaired/seal.json")}
with (here / "DELIVERY.json").open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(here / "DELIVERY.json"))
