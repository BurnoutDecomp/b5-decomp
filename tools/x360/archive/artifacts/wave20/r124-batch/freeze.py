import hashlib
import json
import os
from pathlib import Path
import subprocess


here = Path(__file__).resolve().parent
wave = here.parent
work = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r41-voip-send")
base = "1814a0d617d355d264ca93bd0e8acd1581b0fadc"
head = "91738671728fb1d2aa1da84c9c03a0b3fd7dd566"
tree = "1e0b4eb76130ce3952fd22f31bbf3636e3ef7f93"
paths = [
    "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp",
    "src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h",
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

out = wave / "r124-debug-cull-targeted-preflight"
result = json.loads((out / "result.json").read_text())
proof = json.loads((out / "R124_MATCH.json").read_text())
assert result["exit_code"] == 0 and result["all_post_checks_pass"]
assert result["base_result_sha256"] == pin(wave / "r123-link/result.json")["sha256"]
assert [row["source"] for row in result["preflight_replacements"]] == [paths[0]]
assert all(pin(work / row["source"])["sha256"] == row["source_sha256"] for row in result["preflight_replacements"])
image = out / "native-game.exe"
assert result["outputs"][str(image)]["sha256"] == proof["xex_sha256"] == pin(image)["sha256"]
assert proof["scope"] == "targeted object replacement"
assert proof["methods"]["Do2DBoxesIntersect"]["linked_raw_exact"]
assert proof["methods"]["Do2DBoxesIntersect"]["relocations"] == 0
assert proof["methods"]["Is2DPolygonOnScreen"]["aligned_coff_words"] == 30
assert proof["line_endpoint_calls"] == 2 and proof["line_edge_calls"] == 4 and proof["polygon_calls_box"]

closure = wave / "r124-intake/candidate-header-closure.json"
dependency = json.loads(closure.read_text())
assert dependency["base"] == base and dependency["candidate_tree"] == tree
assert set(dependency["selected_sources"]) == set(dependency["actual_trace_consumers"]) == set(dependency["lexical_native_consumers"])
assert len(dependency["selected_sources"]) == 98 and paths[0] in dependency["selected_sources"]
assert dependency["candidate_blobs_sha256"] == {path:hashlib.sha256(git("show",head+":"+path)).hexdigest() for path in paths}
receipts = [pin(out / f) for f in ("result.json","R124_MATCH.json","verify_debug_cull.py")]
receipts.extend(pin(wave / f) for f in ("r123-link/PUBLISHED_MATCH.json",
    "r124-debug-polygon-match/COMBINED.json","r123-debug2d-line-terra/receipt.txt"))
receipts.append(pin(closure))

patch = here / "r124.patch"
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
    "schema": "r124-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": tree, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-debug-line-and-polygon-culling", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r123-intake/completion.json"),
    "base_link": pin(wave / "r123-link/result.json"), "base_seal": pin(wave / "r123-link/seal.json"),
    "base_diagnostics": pin(wave / "r123-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r123-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r105-native-trace/startup-trace-functions.json"),
    "trace_qualification": "Historical R105 rank. R117/R118 runtime is private diagnostic history. No R124 runtime attempted per user direction.",
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), tree)
