import argparse
import collections
import json
import pathlib
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
old = wave / "r40-link-repaired"
helpers = runpy.run_path(str(old / "prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
parser = argparse.ArgumentParser()
parser.add_argument("--compiled", required=True, type=pathlib.Path)
parser.add_argument("--head", required=True)
parser.add_argument("--tree", required=True)
parser.add_argument("--output", required=True, type=pathlib.Path)
args = parser.parse_args()

seal = read(old / "seal.json")
assert sha(old / "seal.json") == "fed9eb3508f52daea6d1d046a692ccd66cde8aaa4764d2c1fb6ecddceae85193"
baseline = {row["source"]: row for row in seal["selected"]}
report = read(args.compiled)
fresh = {row["source"]: row for row in report["results"]}
assert len(fresh) == report["passed"] == report["requested"] == 26 and report["failed"] == 0
assert all(row["passed"] and row["machine"] == 0x1f2 for row in fresh.values())
snd = "src/SDKs/EATech/include/snd/"
voip = "vendor/dirtysdk/src/x360/voip_x360.cpp"
assert fresh.keys() - baseline.keys() == {voip} | {snd + name + ".cpp" for name in ("sndcontrol", "sndservice", "sndresample")}
targets = {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
assert len(targets) == 15
service = {"SNDSYSI_100hzserver", "SNDSYSI_variabletimerservice", "SNDSYS_service"}
assert all(name.startswith("Voip") for name in targets - service)
expected = {name: snd + "sndservice.cpp" if name in service else voip for name in targets}
defined = {"strong", "comdat", "common", "absolute", "import"}
sdk = helpers["SDK"] / "lib/xbox/xhv.lib"
sdk_hash = "d4cff97c603e15bce167559bca2e7d02f6676c8a958f0e66b37e5c873497485e"
assert sha(sdk) == sdk_hash
xhv_expected = {"_xhv_voicechat_mode", "_xhv_loopback_mode", "XHVCreateEngine"}

def object_symbols(row, verify=False):
    path = pathlib.Path(row["object"])
    if verify:
        assert sha(path) == row["sha256"], path
    result = parse(path.read_bytes())
    assert result is not None, path
    return result

new_references = set()
for source, row in fresh.items():
    prior = {name for name, kind, *_ in object_symbols(baseline[source], True)
             if kind == "undefined"} if source in baseline else set()
    current = {name for name, kind, *_ in object_symbols(row) if kind == "undefined"}
    new_references.update(current - prior)
    if source == voip:
        assert xhv_expected <= current

wanted = new_references | targets
providers = collections.defaultdict(set)
selected = baseline | fresh
for source, row in selected.items():
    for name, kind, *_ in object_symbols(row):
        if name in wanted and kind in defined:
            if source not in fresh:
                assert sha(row["object"]) == row["sha256"]
            providers[name].add(source)
for name, source in expected.items():
    assert providers[name] == {source}, (name, providers[name])
for name in xhv_expected:
    assert name not in providers

missing = new_references - providers.keys()
for lib in [*seal["libraries"], {"path": str(sdk), "sha256": sdk_hash}]:
    if not missing:
        break
    path = pathlib.Path(lib["path"])
    assert sha(path) == lib["sha256"]
    for member, data in archive(path):
        symbols = parse(data)
        if symbols is None and len(data) >= 20 and data[:4] == b"\0\0\xff\xff":
            name = data[20:].split(b"\0")[0].decode("latin1")
            symbols = [(name, "import"), ("__imp_" + name, "import")]
        for name, kind, *_ in symbols or ():
            if name in missing and kind in defined:
                providers[name].add(f"{path.name}({member})")
                missing.remove(name)
assert all(len(providers[name]) == 1 and next(iter(providers[name])).startswith("xhv.lib(")
           for name in xhv_expected)

output = {
    "base": seal["head"], "private_head": args.head, "projected_tree": args.tree,
    "compile_report": {"path": str(args.compiled.resolve()), "sha256": sha(args.compiled)},
    "targeted_compile_passes": len(fresh),
    "expected_cleared": sorted(targets),
    "target_providers": {name: sorted(providers[name]) for name in sorted(targets)},
    "added_reference_providers": {name: sorted(providers[name]) for name in sorted(new_references & providers.keys())},
    "unproved_added_references": sorted(missing),
    "projected_selected": len(selected),
}
with args.output.open("x", encoding="utf-8") as stream:
    json.dump(output, stream, indent=2)
    stream.write("\n")
print("targeted", len(fresh), "targets", len(targets), "new refs", len(new_references),
      "unproved", sorted(missing), "sha", sha(args.output))
assert not missing
