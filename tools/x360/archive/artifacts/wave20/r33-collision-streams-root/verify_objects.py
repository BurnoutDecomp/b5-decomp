import hashlib
import json
import pathlib
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORKTREE = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(HERE.parent / "r32-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
baseline = read(HERE.parent / "r31-link/seal.json")
compile_results = HERE / "native/compile-results.json"
compiled = read(compile_results)["results"]
assert len(compiled) == 8 and all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
objects = {pathlib.Path(row["source"]).stem: pathlib.Path(row["object"]) for row in compiled}
listing = {pathlib.Path(row["source"]).stem: pathlib.Path(row["assembly"]) for row in compiled}
owners = {
    "CgsCollisionGenerator": "RunCollidePrimitiveListWithTriangleListStream",
    "CgsCollisionGenerator_LineStream": "RunCollideLineAgainstPolySoupStream",
    "job_scheduler": "WakeThreads@JobScheduler",
}


def symbols(path):
    rows = parse(path.read_bytes())
    assert rows is not None, path
    return {name: kind for name, kind, *_ in rows}


verified = {}
retained = {row["source"]: row for row in baseline["selected"]}
added_refs = set()
for unit, owner in owners.items():
    obj = objects[unit]
    native_symbols = symbols(obj)
    providers = [name for name, kind in native_symbols.items()
                 if name.startswith("?" + owner + "@") and kind in ("strong", "comdat")]
    assert len(providers) == 1, providers
    previous = retained[next(row["source"] for row in compiled if pathlib.Path(row["source"]).stem == unit)]
    old_obj = pathlib.Path(previous["object"])
    assert sha(old_obj) == previous["sha256"]
    old_symbols = symbols(old_obj)
    refs = {name for name, kind in native_symbols.items()
            if kind == "undefined" and old_symbols.get(name) != "undefined"}
    added_refs.update(refs)
    verified[unit] = {"definition": providers[0], "object": str(obj),
                      "object_sha256": sha(obj), "listing_sha256": sha(listing[unit]),
                      "added_references": sorted(refs)}

line = verified["CgsCollisionGenerator_LineStream"]["definition"]
primitive = verified["CgsCollisionGenerator"]["definition"]
wake = verified["job_scheduler"]["definition"]
assert symbols(objects["CgsSceneManagerModule"])[line] == "undefined"
assert symbols(objects["CgsCollisionGenerator_LineStream"])[wake] == "undefined"
assert {line, primitive} <= {row["symbol"] for row in read(HERE.parent / "r31-link/diagnostic-parsed.json")["unresolved"]}
assert "?SetupJob@CollisionBatch@CgsCollision@CgsSceneManager@@QAAPAUJob@Jobs@EA@@XZ" in symbols(objects["CgsCollisionGenerator"])
line_asm = listing["CgsCollisionGenerator_LineStream"].read_text(errors="replace")
line_asm = line_asm.split("; Begin code for function: " + line, 1)[1].split("; End code for function:", 1)[0]
ordered = ["stbx", "bl           ?StartMonitor@", "bl           ?AddTree@", "bl           ?StopMonitor@",
           "lbz", "bl           ?WakeThreads@", "li           r6,-1", "bl           ?WaitOn@"]
offset = 0
for instruction in ordered:
    offset = line_asm.index(instruction, offset) + len(instruction)
assert "816b001c" in listing["job_scheduler"].read_text(errors="replace")
assert "4e800420" in listing["job_scheduler"].read_text(errors="replace")

providers = {}
unmet = added_refs.copy()
for unit, evidence in verified.items():
    name = evidence["definition"]
    if name in unmet:
        providers[name] = unit
        unmet.remove(name)
for record in baseline["selected"]:
    if not unmet:
        break
    path = pathlib.Path(record["object"])
    for name, kind in symbols(path).items():
        if name in unmet and kind in ("strong", "comdat", "common"):
            assert sha(path) == record["sha256"]
            providers[name] = record["source"]
            unmet.remove(name)
for record in baseline["libraries"]:
    if not unmet:
        break
    path = pathlib.Path(record["path"])
    assert sha(path) == record["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in unmet and kind in ("strong", "comdat", "common"):
                providers[name] = f"{path.name}({member})"
                unmet.remove(name)
assert not unmet, sorted(unmet)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


assert not git("status", "--porcelain")
assert git("rev-parse", "HEAD") == "a1a8bbbd2f9740c42d84307bc6f3e55ac9a3b8de"
receipt = {
    "head": git("rev-parse", "HEAD"),
    "base": git("rev-parse", "HEAD^"),
    "tree": git("rev-parse", "HEAD^{tree}"),
    "compile_sha256": sha(compile_results),
    "passed": len(compiled),
    "objects": verified,
    "added_reference_providers": providers,
    "scope": "PPC object and listing verification. Integrated link and launch pending.",
}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(sha(output), len(providers), "providers verified")
