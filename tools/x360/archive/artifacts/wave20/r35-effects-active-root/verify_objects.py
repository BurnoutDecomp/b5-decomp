import json
import pathlib
import re
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORKTREE = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(HERE.parent / "r34-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
base = HERE.parent / "r34-link"
seal = read(base / "seal.json")
report = HERE / "native/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 5 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
source = "src/GameSource/Effects/EffectsModule.cpp"
current = next(row for row in rows if row["source"] == source)
prior = next(row for row in seal["selected"] if row["source"] == source)
assert sha(prior["object"]) == prior["sha256"]
new = parse(pathlib.Path(current["object"]).read_bytes())
old = parse(pathlib.Path(prior["object"]).read_bytes())
symbol, = [name for name, kind, *_ in new if name.startswith("?UpdateActiveRaceCars@") and kind in ("strong", "comdat")]
assert symbol in {row["symbol"] for row in read(base / "diagnostic-parsed.json")["unresolved"]}
old_refs = {name for name, kind, *_ in old if kind == "undefined"}
new_refs = {name for name, kind, *_ in new if kind == "undefined"} - old_refs
assert symbol in old_refs and not any(name == symbol and kind == "undefined" for name, kind, *_ in new)

listing = pathlib.Path(current["assembly"]).read_text(errors="replace")
body = listing.split("; Begin code for function: " + symbol, 1)[1].split("; End code for function:", 1)[0]
calls = re.findall(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+bl\s+(\S+)", body, re.MULTILINE)
expected = ("?GetFirstEvent@", "?GetNextEvent@", "?GetCarModelId@", "?Reset@", "?GetRaceCarState@",
            "?GetRaceCarColour@", "?GetDeformationModelResourcePtr@", "?Initialise@", "?GetBoostData@",
            "?WriteBoostData@", "?Tick@ActiveRaceCarData@", "?HandleWheels@", "?Tick@EffectsStateMachine@",
            "?GetRaceCarState@", "?GetRaceCarColour@", "?HandleCrashingTrail@")
offset = 0
for prefix in expected:
    offset = next(index + 1 for index, call in enumerate(calls[offset:], offset) if call.startswith(prefix))
assert len(re.findall(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+fsel\s", body, re.MULTILINE)) == 2
assert "HandleJumpAndLandingEffects(lCarState" in body  # The XDK inlines this call.
assert "getenv" not in body and "snprintf" not in body

providers = {}
missing = new_refs.copy()
for row in seal["selected"]:
    if not missing:
        break
    if row["source"] == source:
        continue
    for name, kind, *_ in parse(pathlib.Path(row["object"]).read_bytes()):
        if name in missing and kind in ("strong", "comdat", "common"):
            assert sha(row["object"]) == row["sha256"]
            providers[name] = row["source"]
            missing.remove(name)
for row in seal["libraries"]:
    if not missing:
        break
    path = pathlib.Path(row["path"])
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in missing and kind in ("strong", "comdat", "common"):
                assert sha(path) == row["sha256"]
                providers[name] = f"{path.name}({member})"
                missing.remove(name)
assert not missing, sorted(missing)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


assert git("rev-parse", "HEAD") == "82db45ae570514cc4f396c66f39bd51d92842b7a"
assert git("rev-parse", "HEAD^") == seal["head"] and not git("status", "--porcelain")
result = {
    "base": seal["head"], "head": git("rev-parse", "HEAD"), "tree": git("rev-parse", "HEAD^{tree}"),
    "compiled": len(rows), "compile_sha256": sha(report), "provided_symbol": symbol,
    "products": {row["source"]: {key: sha(row[key]) for key in ("object", "assembly")} for row in rows},
    "new_reference_providers": providers, "calls": calls,
    "scope": "Native targeted compile and COFF provider checks. Full link and runtime pending.",
}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), len(providers), "providers verified")
