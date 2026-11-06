import argparse
import hashlib
import json
import pathlib
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
actual_base = "8e9b44e0058424905069834f4953fe3358ffe492"
audio_head = "e04b2e3fb34756b02898b7b88f634d9e488896f4"
voip_base = "6fbd85919f6e848425bc89af9560dfef0200209a"
voip_path = "vendor/dirtysdk/src/x360/voip_x360.cpp"

parser = argparse.ArgumentParser()
parser.add_argument("--startup-head", required=True)
parser.add_argument("--startup-receipt", required=True, type=pathlib.Path)
parser.add_argument("--repair", nargs=3, action="append", required=True,
                    metavar=("LANE", "HEAD", "RECEIPT"))
args = parser.parse_args()
assert {r[0] for r in args.repair} == {"receive", "worker", "send"} and len(args.repair) == 3

def git(*argv):
    return subprocess.check_output(["git", "--no-optional-locks", *argv], cwd=work)

def pin(path):
    path = pathlib.Path(path)
    return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}

def write(path, data):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")

assert not git("status", "--porcelain")
stages = [("startup", args.startup_head, args.startup_receipt), *args.repair]
for lane, head, receipt in stages:
    assert git("merge-base", voip_base, head).decode().strip() == voip_base
    assert git("diff", "--name-only", voip_base, head).decode().splitlines() == [voip_path]
    assert pathlib.Path(receipt).is_file()

git("switch", "-c", "codex/experimental-x360-r41-reviewed-preflight", audio_head)
git("restore", "--source=" + args.startup_head, "--staged", "--worktree", "--", voip_path)
for lane, head, receipt in args.repair:
    patch = here / (lane + ".patch")
    with patch.open("xb") as stream:
        stream.write(git("diff", "--binary", "--full-index", voip_base, head, "--", voip_path))
    git("apply", "--3way", "--index", str(patch))
assert git("diff", "--cached", "--name-only").decode().splitlines() == [voip_path]
git("-c", "core.whitespace=cr-at-eol", "diff", "--cached", "--check")
git("-c", "core.hooksPath=NUL", "commit", "-m", "Compose reviewed native Xbox 360 VoIP corrections",
    "-m", "Co-Authored-By: GPT-6 <noreply@openai.com>")
head = git("rev-parse", "HEAD").decode().strip()
tree = git("rev-parse", "HEAD^{tree}").decode().strip()
assert not git("status", "--porcelain")
print("COMPOSED", head, tree, flush=True)

compile_command = json.loads((wave / "r41-xhv-preflight/command.json").read_text(encoding="utf-8-sig"))
compile_command[compile_command.index("--out") + 1] = str(here / "native")
write(here / "command.json", compile_command)
subprocess.run(compile_command, cwd=work, check=True)
report = here / "native/compile-results.json"
boundary = here / "COFF_BOUNDARY.json"
verifier = wave / "r41-xhv-preflight/verify_boundary.py"
subprocess.run([sys.executable, "-X", "utf8", "-B", str(verifier),
                "--compiled", str(report), "--head", head, "--tree", tree,
                "--output", str(boundary)], cwd=work, check=True)
write(here / "DELIVERY.json", {
    "base": actual_base, "head": head, "tree": tree, "source": voip_path,
    "startup_and_repair_heads": {lane: commit for lane, commit, _ in stages},
    "receipts": {lane: pin(receipt) for lane, _, receipt in stages},
    "compile": pin(report), "boundary": pin(boundary),
    "preliminary_audit": pin(wave / "r41-batch/SOURCE_AUDIT.md"),
})
print(pin(here / "DELIVERY.json"))
