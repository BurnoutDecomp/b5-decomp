import collections
import json
import pathlib
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
helpers = runpy.run_path(str(wave / "r37-link/prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
old = wave / "r37-link"
seal = read(old / "seal.json")
assert sha(old / "seal.json") == "327612daa19620f4462a8377fc63090c11f78644a017cb46d23a78c405085d38"
baseline = {row["source"]: row for row in seal["selected"]}
compile_results = here / "native/compile-results.json"
compiled = read(compile_results)["results"]
fresh = {row["source"]: row for row in compiled}
tunnel = "vendor/dirtysdk/src/x360/prototunnel_x360.cpp"
assert len(fresh) == 5 and fresh.keys() - baseline.keys() == {tunnel}
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
defined = {"strong", "comdat", "common"}
targets = {"ProtoTunnel" + suffix for suffix in ("Create", "Destroy", "Free", "Control", "Alloc", "Update")}
assert targets <= {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
verbose = "NetPrintfVerboseCode"
proof = targets | {verbose}

def object_symbols(row, verify=False):
    path = pathlib.Path(row["object"])
    if verify:
        assert sha(path) == row["sha256"], path
    symbols = parse(path.read_bytes())
    assert symbols is not None, path
    return symbols

new_references = set()
for path, row in fresh.items():
    prior = {name for name, kind, *_ in object_symbols(baseline[path], True) if kind == "undefined"} if path in baseline else set()
    new_references.update({name for name, kind, *_ in object_symbols(row) if kind == "undefined"} - prior)

providers = collections.defaultdict(set)
selected = baseline | fresh
wanted = new_references | proof
for path, row in selected.items():
    for name, kind, *_ in object_symbols(row):
        if name in wanted and kind in defined:
            if path not in fresh:
                assert sha(row["object"]) == row["sha256"], path
            providers[name].add(path)
for name in targets:
    assert providers[name] == {tunnel}, (name, providers[name])
assert providers[verbose] == {"vendor/dirtysdk/src/dirtylib.cpp"}

missing = new_references - providers.keys()
for row in seal["libraries"]:
    if not missing:
        break
    path = pathlib.Path(row["path"])
    assert sha(path) == row["sha256"]
    for member, data in archive(path):
        for name, kind, *_ in parse(data) or []:
            if name in missing and kind in defined:
                providers[name].add(f"{path.name}({member})")
                missing.remove(name)

result = {
    "base": seal["head"],
    "private_head": "9f53ea57697ea2afba7350b8b3772baeeb768d36",
    "projected_tree": "f7b15aee23e41e8f1086f225e9b5c09dea5f8703",
    "compile_results_sha256": sha(compile_results),
    "baseline_selected": len(baseline),
    "projected_selected": len(selected),
    "fresh": len(fresh),
    "expected_removed": sorted(targets),
    "provider_definitions": {name: sorted(providers[name]) for name in sorted(proof)},
    "added_reference_providers": {name: sorted(providers[name]) for name in sorted(new_references & providers.keys())},
    "unproved_added_references": sorted(missing),
}
output = here / "COFF_BOUNDARY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), "new references", len(new_references), "unproved", sorted(missing))
