import json
import pathlib
import re
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WORKTREE = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(HERE.parent / "r35-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
wave = HERE.parent / "r35-link"
seal = read(wave / "seal.json")
report = HERE / "native-verified/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 4 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
source = "src/GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.cpp"
fresh = next(row for row in rows if row["source"] == source)
old = next(row for row in seal["selected"] if row["source"] == source)
assert sha(old["object"]) == old["sha256"]


def symbols(row):
    result = parse(pathlib.Path(row["object"]).read_bytes())
    assert result is not None, row
    return {name: kind for name, kind, *_ in result}


new_sy, old_sy = symbols(fresh), symbols(old)
symbol, = [name for name, kind in new_sy.items() if name.startswith("?LoadProp@") and kind in ("strong", "comdat")]
assert old_sy[symbol] == "undefined"
assert symbol in {row["symbol"] for row in read(wave / "diagnostic-parsed.json")["unresolved"]}
added = {name for name, kind in new_sy.items() if kind == "undefined" and old_sy.get(name) != "undefined"}
listing = pathlib.Path(fresh["assembly"]).read_text(errors="replace")
body = listing.split("; Begin code for function: " + symbol, 1)[1].split("; End code for function:", 1)[0]
assert re.search(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+vmsum3fp\S*\s", body, re.MULTILINE)
assert re.search(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+vcmpgtfp\.\s", body, re.MULTILINE)
for operation in ("GetRespawnTypeForProp", "WasPropPreviouslyHit", "HasPropBeenHit", "AddPropToScene"):
    assert re.search(r"\bbl\s+\?" + operation + "@", body), operation
assert "BRN_PROPPROG_DIAG" not in body and "getenv" not in body

providers = {}
unmet = added.copy()
for row in seal["selected"]:
    if not unmet:
        break
    if row["source"] == source:
        continue
    for name, kind in symbols(row).items():
        if name in unmet and kind in ("strong", "comdat", "common"):
            assert sha(row["object"]) == row["sha256"]
            providers[name] = row["source"]
            unmet.remove(name)
for row in seal["libraries"]:
    if not unmet:
        break
    path = pathlib.Path(row["path"])
    assert sha(path) == row["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in unmet and kind in ("strong", "comdat", "common"):
                providers[name] = f"{path.name}({member})"
                unmet.remove(name)
assert not unmet, sorted(unmet)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


head = git("rev-parse", "HEAD")
assert head == "46574a905535982ece8e3f8b19e4c15ac3e35e8e" and not git("status", "--porcelain")
source_base = "290b48336fba5857f1e11f57d2198f7a0d2334f7"
assert git("diff", "--name-only", source_base, head) == source
result = {"base": source_base, "head": head, "tree": git("rev-parse", "HEAD^{tree}"),
          "compiled": len(rows), "compile_sha256": sha(report), "provided_symbol": symbol,
          "artist_strings_sha256": sha(HERE / "artist-strings.txt"),
          "object_sha256": sha(fresh["object"]), "listing_sha256": sha(fresh["assembly"]),
          "new_reference_providers": providers,
          "scope": "Native targeted compile, VMX branch and COFF provider checks. Integrated link and runtime pending."}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), len(providers), "providers verified")
