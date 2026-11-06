import json
import pathlib
import re
import runpy
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
WORKTREE = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
TOOLS = runpy.run_path(str(WAVE / "r35-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
report = HERE / "native-assert-repair/compile-results.json"
rows = read(report)["results"]
assert len(rows) == 4 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
source = "src/GameSource/World/EntityModules/PropEntityModule/BrnPropZoneManager.cpp"
row = next(row for row in rows if row["source"] == source)
old_report = WAVE / "r36-root-combined/native/compile-results.json"
old = next(row for row in read(old_report)["results"] if row["source"] == source)


def symbols(path):
    result = parse(pathlib.Path(path).read_bytes())
    assert result is not None, path
    return {name: kind for name, kind, *_ in result}


new_sy, old_sy = symbols(row["object"]), symbols(old["object"])
symbol, = [name for name, kind in new_sy.items() if name.startswith("?LoadProp@") and kind in ("strong", "comdat")]
listing = pathlib.Path(row["assembly"]).read_text(errors="replace")
body = listing.split("; Begin code for function: " + symbol, 1)[1].split("; End code for function:", 1)[0]
assert " Cellid: " in body and " Prop pos: " in body and "(%f, %f, %f)" in body
calls = re.findall(r"^ +[0-9a-f]+\s+[0-9a-f]+\s+bl\s+(\S+)", body, re.MULTILINE)
expected = ("?BeginAssert@", "??0StrStream@", "??6Props@BrnPhysics@", "?AppendFormat@",
            "?FireAssert@", "?EndAssert@")
offset = 0
for prefix in expected:
    offset = next(index + 1 for index, call in enumerate(calls[offset:], offset) if call.startswith(prefix))
assert re.search(r"\bli\s+r5,841\b", body)

new_refs = {name for name, kind in new_sy.items() if kind == "undefined" and old_sy.get(name) != "undefined"}
missing = new_refs.copy()
providers = {}
seal = read(WAVE / "r35-link/seal.json")
for retained in seal["selected"]:
    if not missing:
        break
    if retained["source"] == source:
        continue
    for name, kind in symbols(retained["object"]).items():
        if name in missing and kind in ("strong", "comdat", "common"):
            assert sha(retained["object"]) == retained["sha256"]
            providers[name] = retained["source"]
            missing.remove(name)
for record in seal["libraries"]:
    if not missing:
        break
    path = pathlib.Path(record["path"])
    assert sha(path) == record["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in missing and kind in ("strong", "comdat", "common"):
                providers[name] = f"{path.name}({member})"
                missing.remove(name)
assert not missing, sorted(missing)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=WORKTREE, text=True).strip()


assert git("rev-parse", "HEAD") == "6b2d858dede965dbc9133a409881f69b560d3fe0"
assert git("diff", "--name-only", "HEAD^", "HEAD") == source and not git("status", "--porcelain")
result = {"base": git("rev-parse", "HEAD^"), "head": git("rev-parse", "HEAD"),
          "tree": git("rev-parse", "HEAD^{tree}"), "paths": [source], "compiled": len(rows),
          "compile_sha256": sha(report), "object_sha256": sha(row["object"]),
          "assembly_sha256": sha(row["assembly"]), "new_reference_providers": providers}
output = HERE / "ASSERT_REPAIR.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), providers)
