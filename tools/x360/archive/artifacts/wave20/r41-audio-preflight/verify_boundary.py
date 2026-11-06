import collections
import json
import pathlib
import runpy

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
helpers = runpy.run_path(str(wave / "r39-link-repaired/prepare_seal.py"))
read, parse, archive, sha = (helpers[key] for key in ("read", "parse", "archive", "sha"))
old = wave / "r39-link-repaired"
seal = read(old / "seal.json")
assert sha(old / "seal.json") == "0b8240e026765c33a5a6b84227a9de342ee87a43b82af304fbc0d951b37a3639"
baseline = {row["source"]: row for row in seal["selected"]}
compile_results = here / "native/compile-results.json"
compiled = read(compile_results)["results"]
fresh = {row["source"]: row for row in compiled}

snd = "src/SDKs/EATech/include/snd/"
new_sources = {snd + name + ".cpp" for name in ("sndfilter", "sndmix", "sndpacket", "sndsys", "sndvoice", "snd_panning_x360", "snd_startup_x360", "sndmix_audio", "sndcontrol", "sndresample", "sndservice")}
assert len(fresh) == 22 and fresh.keys() - baseline.keys() == new_sources
assert all(row["passed"] and row["machine"] == 0x1F2 for row in compiled)
defined = {"strong", "comdat", "common"}
raw_retired = {"byte_83277838", "dword_83277830", "dword_83277834"}
old_unresolved = {row["symbol"] for row in read(old / "diagnostic-parsed.json")["unresolved"]}
lobby = "vendor/dirtysdk/src/x360/lobbylogin.cpp"
expected = {
    "MIX_audio": snd + "sndmix_audio.cpp",
    "SNDVOICEI_free": snd + "sndvoice.cpp",
    "SNDI_aztospkrvol": snd + "snd_panning_x360.cpp",
    "SNDSYS_restore": snd + "snd_startup_x360.cpp",
    "SNDSYSI_100hzserver": snd + "sndservice.cpp",
    "SNDSYSI_variabletimerservice": snd + "sndservice.cpp",
    "SNDSYS_service": snd + "sndservice.cpp",
    "MIX_create": snd + "sndmix.cpp",
    "MIX_destroy": snd + "sndmix.cpp",
    "SNDPKTPLAYI_flushcallbackdata": snd + "sndpacket.cpp",
    "?gpfnSndAlloc@Snd@@3P6APAXI@ZA": snd + "sndmix.cpp",
    "?gpfnSndFree@Snd@@3P6AXPAX@ZA": snd + "sndmix.cpp",
    "?computeFrameTextureMap@CVideoFrameAnalyst@@QAAXPAE@Z": "src/SDKs/EATech/rwmovie/CVideoFrameAnalyst.cpp",
    "sub_82B1FD80": lobby,
    "sub_82B20070": lobby,
}
proof = set(expected)
targets = proof | raw_retired
assert len(targets) == 18 and targets <= old_unresolved

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
    assert not {name for name, kind, *_ in object_symbols(row) if kind == "undefined"} & raw_retired
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
    "private_head": "e04b2e3fb34756b02898b7b88f634d9e488896f4",
    "projected_tree": "0bb46c7302f8d24a762987e3f6fc3450abedee6a",
    "compile_results_sha256": sha(compile_results),
    "baseline_selected": len(baseline),
    "projected_selected": len(selected),
    "fresh": len(fresh),
    "expected_removed": sorted(targets),
    "provider_definitions": {name: sorted(providers[name]) for name in sorted(proof)},
    "added_reference_providers": {name: sorted(providers[name]) for name in sorted(new_references & providers.keys())},
    "unproved_added_references": sorted(missing - old_unresolved),
    "remaining_old_references": sorted(missing & old_unresolved),
}
output = here / "COFF_BOUNDARY.json"
with output.open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(sha(output), "new references", len(new_references), "unproved", sorted(missing))
