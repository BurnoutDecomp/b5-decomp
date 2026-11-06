import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "966fd954d2215f767993a09b049e06d953698d33"
head = "afbc2910cff536d3f3d48272cecfcb948f9c20d7"
tree = "5d77ae9e2503dec8d9ec5dda161a6301b9d985cc"
paths = [
    "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp",
    "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
    "src/GameSource/Game/BrnLoadingScreenRenderer.cpp",
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

out = wave / "r123-byte-match-exact-preflight"
result = json.loads((out / "result.json").read_text())
proof = json.loads((out / "R123_MATCH.json").read_text())
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
assert result["base_result_sha256"] == pin(wave / "r122-link/result.json")["sha256"]
assert sorted(row["source"] for row in result["preflight_replacements"]) == paths
assert all(pin(work / row["source"])["sha256"] == row["source_sha256"] for row in result["preflight_replacements"])
image = out / "native-game.exe"
assert result["outputs"][str(image)]["sha256"] == proof["private_xex_sha256"] == pin(image)["sha256"]
assert proof["commit"] == head and proof["add_command_linked_address_normalized_exact"]
assert proof["debug"]["Do2DBoxesIntersect"]["raw_exact"]
assert proof["debug"]["Do2DBoxesIntersect"]["relocations"] == 0
assert proof["debug"]["Is2DPointOnScreen"]["aligned_words"] == 40
assert proof["io"]["preserved_address_normalized_functions"] == 3
assert proof["overlay"]["direct_native_quad_call"]
receipts = [pin(out / f) for f in ("result.json","R123_MATCH.json","verify_match.py")]
receipts.append(pin(wave / "r122-link/PUBLISHED_MATCH.json"))
receipts.append(pin(wave / "r123-debug-box-match/COMPARISON.json"))
receipts.append(pin(wave / "iobuffer-ordered-stream-terra/RECEIPT.json"))
receipts.append(pin(wave / "r122-overlay-quad-terra/receipt.txt"))

patch = here / "r123.patch"
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
    "schema": "r123-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-ppc-debug-boxes-and-overlay", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r122-intake/completion.json"),
    "base_link": pin(wave / "r122-link/result.json"), "base_seal": pin(wave / "r122-link/seal.json"),
    "base_diagnostics": pin(wave / "r122-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r122-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 rank. R117/R118 runtime is private diagnostic history. No R123 runtime attempted per user direction.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
