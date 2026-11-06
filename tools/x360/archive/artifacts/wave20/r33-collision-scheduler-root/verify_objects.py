import json
import pathlib
import re
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORKTREE = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(HERE.parent / "r32-link/prepare_seal.py"))
read, parse, sha = (TOOLS[key] for key in ("read", "parse", "sha"))
results = HERE / "native/compile-results.json"
rows = read(results)["results"]
assert len(rows) == 11 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
units = {pathlib.Path(row["source"]).stem: row for row in rows}
unresolved = {row["symbol"] for row in read(HERE.parent / "r31-link/diagnostic-parsed.json")["unresolved"]}


def instructions(unit, function):
    assembly = pathlib.Path(units[unit]["assembly"]).read_text(errors="replace")
    pattern = r"; Begin code for function: (\?" + function + r"@[^\r\n]+)\s*([\s\S]+?); End code for function:"
    match = re.search(pattern, assembly)
    assert match, function
    symbol, body = match.groups()
    calls = re.findall(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+bl\s+(\S+)", body, re.MULTILINE)
    return symbol, body, calls


def verify_calls(calls, expected):
    offset = 0
    for prefix in expected:
        index = next(i for i, call in enumerate(calls[offset:], offset) if call.startswith(prefix))
        offset = index + 1


collision = "CgsCollisionGenerator"
streams = "CgsCollisionGenerator_CollideStreams"
object_rows = parse(pathlib.Path(units[collision]["object"]).read_bytes())
definitions = {row[0] for row in object_rows if row[1] in ("strong", "comdat")}
checks = {}
for function in ("CollidePrimitivePairList", "CollidePrimitiveListAgainstTriangleList",
                 "CollideSweptSphereListWithTriangleList"):
    symbol, body, calls = instructions(collision, function)
    assert symbol in definitions and symbol in unresolved
    verify_calls(calls, ("?PrepareNewPrimitiveTestResultsList@", "?CreateNewBatch@", "?Prepare@",
                         "?StartMonitor@", "?SetupJob@", "?JobManager@", "?AddJobs@", "?StopMonitor@"))
    assert "?ContactGeneratorEntry@" not in body
    assert re.search(r"\bli\s+r5,1\b", body), function
    if function == "CollidePrimitiveListAgainstTriangleList":
        assert "lbz" in body and "sbOptimisedBoxTestsEnabled" in body
    checks[function] = {"symbol": symbol, "calls": calls}

for function in ("RunCollideSphereListWithTriangleListStream", "RunCollideSweptSphereListWithTriangleListStream",
                 "RunCollideSphereListWithSphereListStream"):
    symbol, body, calls = instructions(streams, function)
    assert all(not call.startswith("?ContactGeneratorEntry@") for call in calls)
    assert "?ContactGeneratorEntry@" in body
    if function == "RunCollideSphereListWithTriangleListStream":
        verify_calls(calls, ("?DependsOn@", "?StartMonitor@", "?AddTree@", "?StopMonitor@"))
    else:
        verify_calls(calls, ("?StartMonitor@", "?DependsOn@", "?StopMonitor@", "?AddTree@"))
    checks[function] = {"symbol": symbol, "calls": calls}


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD") == "0967ded339da36eb921e2f373e2dcd5a56e54c6d"
receipt = {
    "head": git("rev-parse", "HEAD"),
    "base": git("rev-parse", "HEAD^"),
    "tree": git("rev-parse", "HEAD^{tree}"),
    "compile_sha256": sha(results),
    "compiled": len(rows),
    "checks": checks,
    "products": {row["source"]: {key: sha(row[key]) for key in ("assembly", "object")} for row in rows},
    "scope": "Native providers and scheduler edges. Integrated link and launch pending.",
}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(sha(output), len(checks), "scheduler paths verified")
