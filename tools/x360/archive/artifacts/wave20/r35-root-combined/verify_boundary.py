import collections
import json
import pathlib
import runpy

HERE = pathlib.Path(__file__).resolve().parent
WAVE = HERE.parent
TOOLS = runpy.run_path(str(WAVE / "r34-link/prepare_seal.py"))
read, parse, archive, sha = (TOOLS[key] for key in ("read", "parse", "archive", "sha"))
seal = read(WAVE / "r34-link/seal.json")
baseline = {row["source"]: row for row in seal["selected"]}
compiled = read(HERE / "native/compile-results.json")["results"]
fresh = {row["source"]: row for row in compiled}
assert len(fresh) == 16 and set(fresh) <= baseline.keys()
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
defs = {"strong", "comdat", "common"}
unresolved = {row["symbol"] for row in read(WAVE / "r34-link/diagnostic-parsed.json")["unresolved"]}
prefixes = ("?UpdateActiveRaceCars@", "?Update@OnlineInviteMessageComponent@", "?FFT_ForwardReal@", "?FFT_InverseReal@")
targets = {name for name in unresolved if name.startswith(prefixes)}
assert len(targets) == 4
new_symbols = {name for name in targets if not name.startswith("?FFT_")}
old_fft = targets - new_symbols


def symbols(row, verify=False):
    path = pathlib.Path(row["object"])
    if verify:
        assert sha(path) == row["sha256"], path
    result = parse(path.read_bytes())
    assert result is not None, path
    return result


new_references = set()
new_fft = set()
for source, row in fresh.items():
    prior = {name for name, kind, *_ in symbols(baseline[source], True) if kind == "undefined"}
    rows = symbols(row)
    new_references.update({name for name, kind, *_ in rows if kind == "undefined"} - prior)
    new_fft.update(name for name, kind, *_ in rows if kind in defs and name.startswith(prefixes[2:]))
assert len(new_fft) == 2 and not new_fft & old_fft
new_symbols.update(new_fft)

providers = collections.defaultdict(list)
leftover = collections.defaultdict(list)
wanted = new_references | new_symbols | old_fft
for source, row in baseline.items():
    current = fresh.get(source, row)
    rows = symbols(current)
    for name, kind, *_ in rows:
        if name not in wanted:
            continue
        if source not in fresh:
            assert sha(current["object"]) == row["sha256"]
        if kind in defs:
            providers[name].append(source)
        if name in old_fft and kind == "undefined":
            leftover[name].append(source)
assert not leftover and not old_fft.intersection(providers), leftover
for name in new_symbols:
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
          "new_symbol_providers": {name: providers[name] for name in sorted(new_symbols)},
          "added_reference_providers": {name: providers[name][:3] for name in sorted(new_references) if name in providers},
          "unproved_added_references": sorted(missing)}
output = HERE / "COFF_BOUNDARY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), "new references", len(new_references), "unproved", sorted(missing))
