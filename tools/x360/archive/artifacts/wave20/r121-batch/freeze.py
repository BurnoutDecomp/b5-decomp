import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "f642eb75191c13eb3b595a1d36f56427f493109a"
head = "8c0d37f438af7ec851a4c7ad4167918d3ed33442"
tree = "c5b083af0f209bd218de6f86bed74017d02eed7c"
paths = [
    "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp",
    "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
    "src/GameSource/Game/BrnLoadingScreenRenderer.cpp",
    "src/GameSource/World/BrnWorldModule.cpp",
]


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(revision, path):
    return git("ls-tree", revision, "--", path).decode().split("\t")[0].strip()


assert git("rev-parse", "HEAD").decode().strip() == head and not git("status", "--porcelain")
assert git("merge-base", base, head).decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == tree
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("-c", "core.whitespace=cr-at-eol", "diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

out = wave / "r121-byte-match-exact-preflight"
result = json.loads((out / "result.json").read_text())
proof = json.loads((out / "R121_MATCH.json").read_text())
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
assert result["base_result_sha256"] == pin(wave / "r117-link/result.json")["sha256"]
assert sorted(row["source"] for row in result["preflight_replacements"]) == paths
assert all(pin(work / row["source"])["sha256"] == row["source_sha256"] for row in result["preflight_replacements"])
image = out / "native-game.exe"
assert result["outputs"][str(image)]["sha256"] == proof["private_xex_sha256"] == pin(image)["sha256"]
assert proof["commit"] == head and proof["add_command"]["linked_address_normalized_exact"]
assert proof["debug_point"][1]["aligned_identical_words"] == 36
assert all(proof["io_buffer"][name]["equal_after_masking_relocation_fields"]
           for name in ("UnlockForWrite","LockForRead","UnlockForRead"))
receipts = [pin(out / f) for f in ("result.json","R121_MATCH.json","verify_match.py")]
for name in ("r118-world-buffer-unlock-terra","iobuffer-byte-match-terra","iobuffer-byte-followup-terra"):
    receipts.append(pin(wave / name / "RECEIPT.json"))
receipts.append(pin(wave / "native-xenia-smoke/run-r118-io-lock-caller/process-result.json"))

patch = here / "r121.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == tree

manifest = here / "MANIFEST.json"
data = {
    "schema": "r121-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-ppc-byte-matching-and-world-output", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r117-intake/completion.json"),
    "base_link": pin(wave / "r117-link/result.json"), "base_seal": pin(wave / "r117-link/seal.json"),
    "base_diagnostics": pin(wave / "r117-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r117-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 rank. R117/R118 runtime is private diagnostic history. No R121 runtime attempted per user direction.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
