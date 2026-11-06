import collections
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
baseline = {row["source"]: row for row in seal["selected"]}
report = HERE / "native/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 6 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
fresh = {row["source"]: row for row in rows}
source = "src/GameSource/World/AI/BrnAIDebugComponent.cpp"
owners = {
    "?gfMarkedManMaxOvertakeSpeed@BrnAI@@3MA": "src/GameSource/World/AI/BrnAIAggression.cpp",
    "?gfMarkedManMinOvertakeSpeed@BrnAI@@3MA": "src/GameSource/World/AI/BrnAIAggression.cpp",
    "?gfMarkedManMinAggression@BrnAI@@3MA": "src/GameSource/World/AI/BrnAICar_Update.cpp",
    "?gfMarkedManMaxAggression@BrnAI@@3MA": "src/GameSource/World/AI/BrnAICar_Update.cpp",
}
activation = "?OnActivate@AIDebugComponent@BrnAI@@MAAXXZ"
assert activation in {row["symbol"] for row in read(base / "diagnostic-parsed.json")["unresolved"]}


def symbols(row):
    values = parse(pathlib.Path(row["object"]).read_bytes())
    assert values is not None, row["object"]
    return {name: kind for name, kind, *_ in values}


native = {path: symbols(row) for path, row in fresh.items()}
assert native[source][activation] in ("strong", "comdat")
assert all(native[source][name] == "undefined" and native[owner][name] == "strong"
           for name, owner in owners.items())
listing = pathlib.Path(fresh[source]["assembly"]).read_text(errors="replace")
body = listing.split("; Begin code for function: " + activation, 1)[1].split("; End code for function:", 1)[0]
calls = re.findall(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+bl\s+(\S+)", body, re.MULTILINE)
counts = collections.Counter(call.split("@")[0] for call in calls)
artist = (HERE / "artist.txt").read_text()
original = artist.split("Function 82767428", 1)[0]
registrations = len(re.findall(r"\bbl\s+(?:CgsDev__DebugComponent__RegisterVariable|sub_8282D(?:640|800|720|6B0))\b", original))
assert counts["?RegisterVariable"] == registrations == 52
assert counts["?SetStep"] == 2 and counts["?SetRange"] == 3
assert counts["?RegisterFunction"] == 1 and counts["?AddMenuItem"] == 2 and counts["?AddFloat"] == 8
callback = listing.split("; Begin code for function: ?DeactiveateDriversCallback@", 1)[1].split("; End code for function:", 1)[0]
assert len(re.findall(r"\bstb\s+r\d+,1D69h\(r\d+\)", callback)) == 8

new_refs = set()
for path in (source, *sorted(set(owners.values()))):
    prior = baseline[path]
    assert sha(prior["object"]) == prior["sha256"]
    prior_undefined = {name for name, kind in symbols(prior).items() if kind == "undefined"}
    new_refs.update({name for name, kind in native[path].items() if kind == "undefined"} - prior_undefined)
providers = {name: path for path, sy in native.items() for name, kind in sy.items()
             if name in new_refs and kind in ("strong", "comdat", "common")}
missing = new_refs - providers.keys()
for path, row in baseline.items():
    if not missing:
        break
    if path in fresh:
        continue
    for name, kind in symbols(row).items():
        if name in missing and kind in ("strong", "comdat", "common"):
            assert sha(row["object"]) == row["sha256"]
            providers[name] = path
            missing.remove(name)
for row in seal["libraries"]:
    if not missing:
        break
    path = pathlib.Path(row["path"])
    assert sha(path) == row["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in missing and kind in ("strong", "comdat", "common"):
                providers[name] = f"{path.name}({member})"
                missing.remove(name)
assert not missing, sorted(missing)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


assert git("rev-parse", "HEAD") == "290b48336fba5857f1e11f57d2198f7a0d2334f7"
assert git("rev-parse", "HEAD^") == seal["head"] and not git("status", "--porcelain")
result = {"base": seal["head"], "head": git("rev-parse", "HEAD"), "tree": git("rev-parse", "HEAD^{tree}"),
          "compiled": len(rows), "compile_sha256": sha(report), "provided_symbol": activation,
          "artist_sha256": sha(HERE / "artist.txt"), "data_sha256": sha(HERE / "data.txt"),
          "registrations": registrations, "callback_driver_toggles": 8,
          "tuning_owners": owners, "new_reference_providers": providers,
          "scope": "Native targeted compile and COFF provider checks. Integrated link and runtime pending."}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), len(providers), "providers verified")
