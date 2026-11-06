"""Check that the six-path R141 composition retains its verified PPC bodies."""
from pathlib import Path
import json
import re

here = Path(__file__).resolve().parent
wave = here.parent
helper = wave / "r128-batch/verify_matches.py"
ns = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], ns)
ns["masks"][2] = 0xFFFFFFFF

def read(folder):
    rows = json.loads((folder / "compile-results.json").read_text())["results"]
    assert all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
    return {Path(row["source"]).stem: row for row in rows}

def symbol_for(row, prefix):
    text = Path(row["assembly"]).read_text(errors="replace")
    symbols = re.findall(r"(?m)^; Begin code for function: (\S+)", text)
    symbol, = [name for name in symbols if name.startswith(prefix)]
    return symbol

def collect(row, symbol):
    obj = Path(row["object"])
    code, relocs = ns["collect"](obj, symbol)
    names = []
    for offset, index, kind in relocs:
        if kind == 18:
            continue
        data = obj.read_bytes()
        name, value, _ = ns["object_symbol"](data, index)
        if kind in (16, 17) and name.startswith("??_C@") and (
                "@C?3?2Users?2adri1?2?4codex?2worktrees?2@" in name
                or "@D?3?2Reverse?2BP?9X360?9Experimental?9@" in name):
            path = ns["symbol_literal"](data, index).replace(chr(92), "/").lower()
            roots = ("c:/users/adri1/.codex/worktrees/bp360/native-replay-collision/",
                     "d:/reverse/bp-x360-experimental-worktrees/r41-voip-send/")
            root, = [root for root in roots if path.startswith(root + "src/")]
            name = "diagnostic:" + path[len(root):]
        else:
            name = f"local+{value:#x}" if name.startswith("$LN") else re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)
        names.append((offset, kind, name))
    return code, relocs, names

reports = {
    "BrnGameMainFlowStates": ("r141-loading-bundle-preflight", [
        "?OnEnter@MainGameFlowStateInitialLoadingScreen@@",
        "?OnEnter@MainGameFlowStateCompleteLoading@@",
    ]),
    "CgsBundleLoaderModule": ("r141-loading-bundle-preflight", ["?ProcessBundleHeader@BundleLoaderModule@"]),
    "BrnBootVideos": ("r141-boot-videos/compile", ["?Update@BootVideos@"]),
    "AptActionInterpreterCompare": ("r141-apt-equals2/native", ["?_FunctionAptActionEquals2@"]),
    "BrnRaceCarEntityModule": ("r141-racecar-readiness/candidate", [
        "?UpdateStreaming@RaceCarEntityModule",
        "?OnRaceCarResourcesLoaded@RaceCarEntityModule",
        "?PreSceneUpdate@RaceCarEntityModule",
        "?PostPhysicsUpdate@RaceCarEntityModule",
        "?SendGameEvents@RaceCarEntityModule",
    ]),
}
composed = read(here)
assert len(composed) == 7
matched = {}
for stem, (location, prefixes) in reports.items():
    prior = read(wave / location)[stem]
    current = composed[stem]
    assert prior["source"] == current["source"]
    for prefix in prefixes:
        name = symbol_for(prior, prefix)
        assert symbol_for(current, prefix) == name
        before, _, expected_refs = collect(prior, name)
        body, relocs, refs = collect(current, name)
        ns["fixed_equal"](before, body, relocs)
        assert refs == expected_refs, name
        matched[name] = {"bytes": len(body), "relocations": len(refs)}
proof = {
    "head": "36e3fb04c83170ea446fe3cc6503f0db0d34a50c",
    "tree": "341f7e7ee638d2dc398afb6075cb6e3045cdf466",
    "targeted_native": len(composed),
    "compile_report_sha256": ns["sha"](here / "compile-results.json"),
    "objects": {row["source"]: ns["sha"](Path(row["object"])) for row in composed.values()},
    "functions": matched,
}
out = here / "R141_COMPOSED_COFF.json"
with out.open("x", encoding="utf-8") as stream:
    json.dump(proof, stream, indent=2)
    stream.write("\n")
print(out, ns["sha"](out), len(matched))
