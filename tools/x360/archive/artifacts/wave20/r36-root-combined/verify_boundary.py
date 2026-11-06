import collections
import json
import pathlib
import runpy

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
TOOLS = runpy.run_path(str(WAVE / "r35-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
seal = read(WAVE / "r35-link/seal.json")
baseline = {row["source"]: row for row in seal["selected"]}
compiled = read(HERE / "native/compile-results.json")["results"]
fresh = {row["source"]: row for row in compiled}
assert len(fresh) == 16 and set(fresh) <= baseline.keys()
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
defs = {"strong", "comdat", "common"}
unresolved = {row["symbol"] for row in read(WAVE / "r35-link/diagnostic-parsed.json")["unresolved"]}
prefixes = ("?OnActivate@AIDebugComponent@", "?LoadProp@PropZoneManager@", "?Prepare@EnvironmentManager@",
            "?StreamIn@EnvironmentManager@", "?StreamOut@EnvironmentManager@")
targets = {name for name in unresolved if name.startswith(prefixes)}
assert len(targets) == 5


def symbols(row, verify=False):
    path = pathlib.Path(row["object"])
    if verify:
        assert sha(path) == row["sha256"], path
    result = parse(path.read_bytes())
    assert result is not None, path
    return result


new_references = set()
for path, row in fresh.items():
    prior = {name for name, kind, *_ in symbols(baseline[path], True) if kind == "undefined"}
    new_references.update({name for name, kind, *_ in symbols(row) if kind == "undefined"} - prior)

providers = collections.defaultdict(list)
wanted = new_references | targets
for path, row in baseline.items():
    current = fresh.get(path, row)
    for name, kind, *_ in symbols(current):
        if name in wanted and kind in defs:
            if path not in fresh:
                assert sha(current["object"]) == row["sha256"]
            providers[name].append(path)
for name in targets:
    assert len(providers[name]) == 1 and providers[name][0] in fresh, (name, providers[name])

missing = new_references - providers.keys()
for row in seal["libraries"]:
    if not missing:
        break
    path = pathlib.Path(row["path"])
    assert sha(path) == row["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in missing and kind in defs:
                providers[name].append(f"{path.name}({member})")
                missing.remove(name)
result = {"base": seal["head"], "combined_receipt_sha256": sha(HERE / "DELIVERY.json"),
          "fresh": len(fresh), "baseline_selected": len(baseline), "expected_removed": sorted(targets),
          "new_symbol_providers": {name: providers[name] for name in sorted(targets)},
          "added_reference_providers": {name: providers[name][:3] for name in sorted(new_references) if name in providers},
          "unproved_added_references": sorted(missing)}
output = HERE / "COFF_BOUNDARY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), "new references", len(new_references), "unproved", sorted(missing))
