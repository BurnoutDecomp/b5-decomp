import collections
import json
import pathlib
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
helpers = runpy.run_path(str(wave / "r38-link/prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
old = wave / "r38-link"
seal = read(old / "seal.json")
assert sha(old / "seal.json") == "07f13d2677a81f1274f58f5d0fb1e6bef5b5dd98cf0c07cdc8615ba95e6b7350"
baseline = {row["source"]: row for row in seal["selected"]}
compile_results = here / "native/compile-results.json"
compiled = read(compile_results)["results"]
fresh = {row["source"]: row for row in compiled}

srp = "vendor/dirtysdk/src/x360/commsrp_native_x360.cpp"
tcp = "vendor/dirtysdk/src/x360/commtcp_native_x360.cpp"
codec = "vendor/renderware/src/rw/audio/core/EaLayer3Dec.cpp"
address = "vendor/dirtysdk/src/x360/dirtynet_addr_x360.cpp"
ai = "src/GameSource/World/AI/BrnAIDebugComponent.cpp"
assert len(fresh) == 19 and fresh.keys() - baseline.keys() == {srp, tcp, codec}
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
old_codec = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@SAHHE@Z"
new_codec = "?CreateInstance@EaLayer3DecBase@core@audio@rw@@QAAHE@Z"
render = "?RenderWorld@AIDebugComponent@BrnAI@@UAAXPAUDebug3DImmediateRender@CgsDev@@@Z"
targets = {"CommSRPConstruct", "CommTCPConstruct", old_codec, render}
assert targets <= {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
defined = {"strong", "comdat", "common"}
expected = {"CommSRPConstruct": srp, "CommTCPConstruct": tcp, new_codec: codec,
            render: ai, "SockaddrInParse": address}
proof = set(expected)

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

for name, owner in expected.items():
    assert providers[name] == {owner}, (name, providers[name])
for row in selected.values():
    assert all(name != old_codec or kind != "undefined" for name, kind, *_ in object_symbols(row))
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
    "private_head": "d6f0c484d9c0dd914513b5925bb840f9d7e275eb",
    "projected_tree": "040b63b56f4f46ce4be1ff93022d35460d20e672",
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

