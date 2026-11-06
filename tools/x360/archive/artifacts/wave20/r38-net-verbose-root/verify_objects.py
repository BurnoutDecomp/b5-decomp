import json
import pathlib
import re
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
WORK = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(WAVE / "r37-link/prepare_seal.py"))
read, parse, sha = (TOOLS[key] for key in ("read", "parse", "sha"))
seal = read(WAVE / "r37-link/seal.json")
report = HERE / "native/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 3 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
source = "vendor/dirtysdk/src/dirtylib.cpp"
current = next(row for row in rows if row["source"] == source)
old = next(row for row in seal["selected"] if row["source"] == source)
provider = next(row for row in seal["selected"] if row["source"] == "vendor/dirtysdk/src/plat-str.cpp")
assert sha(old["object"]) == old["sha256"] and sha(provider["object"]) == provider["sha256"]


def symbols(row):
    return {name: kind for name, kind, *_ in parse(pathlib.Path(row["object"]).read_bytes())}


before, after = symbols(old), symbols(current)
assert "NetPrintfVerboseCode" not in before and after["NetPrintfVerboseCode"] in ("strong", "comdat")
assert {name for name, kind in after.items() if kind == "undefined" and before.get(name) != "undefined"} == {"ds_vsnzprintf"}
assert symbols(provider)["ds_vsnzprintf"] in ("strong", "comdat")
listing = pathlib.Path(current["assembly"]).read_text(errors="replace")
body = listing.split("; Begin code for function: NetPrintfVerboseCode", 1)[1].split("; End code for function:", 1)[0]
assert re.search(r"\bli\s+r4,1024\b", body)
assert re.search(r"\bbl\s+ds_vsnzprintf[\s\S]*?\bcmpw\s+[\s\S]*?\bbl\s+NetPrintfCode", body)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORK, text=True).strip()


assert git("rev-parse", "HEAD") == "d45af1d15eb19afdd167d7c484c879fc1cd4b7fa"
assert git("rev-parse", "HEAD^") == seal["head"] and not git("status", "--porcelain")
result = {"base": seal["head"], "head": git("rev-parse", "HEAD"), "tree": git("rev-parse", "HEAD^{tree}"),
          "compiled": len(rows), "compile_sha256": sha(report), "symbol": "NetPrintfVerboseCode",
          "object_sha256": sha(current["object"]), "listing_sha256": sha(current["assembly"]),
          "native_provider": provider, "scope": "Native targeted compile and COFF verification. No network execution."}
output = HERE / "DELIVERY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output))
