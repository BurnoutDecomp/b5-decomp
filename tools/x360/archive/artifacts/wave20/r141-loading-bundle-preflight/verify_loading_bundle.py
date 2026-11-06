"""Verify three ARTIST loading and bundle sequences in SDK COFF or a strict link."""
from pathlib import Path
import json
import struct
import sys

here = Path(__file__).resolve().parent
wave = here.parent
helper = wave / "r128-batch/verify_matches.py"
ns = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], ns)
link = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None
flow = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
bundle = "src/GameShared/GameClasses/System/Resource/CgsBundleLoaderModule.cpp"
initial = "?OnEnter@MainGameFlowStateInitialLoadingScreen@@UAAXXZ"
complete = "?OnEnter@MainGameFlowStateCompleteLoading@@UAAXXZ"
header = "?ProcessBundleHeader@BundleLoaderModule@CgsResource@@QAAXXZ"

def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def objects(folder):
    rows = read(folder / "compile-results.json")["results"]
    assert all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
    return {row["source"]: Path(row["object"]) for row in rows}

def inspect(obj, symbol):
    data = obj.read_bytes()
    body, relocs = ns["collect"](obj, symbol)
    refs = [(offset, kind, ns["object_symbol"](data, index)[0])
            for offset, index, kind in relocs if kind != 18]
    return body, relocs, refs

def words(body, start, expected):
    data = bytes.fromhex(expected)
    assert body[start:start + len(data)] == data, (hex(start), expected)

target = objects(here)
if link:
    selected = {row["source"]: Path(row["object"])
                for row in read(link / "invocation.json")["objects"]}
    address, linked = ns["linked_reader"](link)
else:
    selected = target
original = ns["original"]
records = {}

for label, source, symbol, artist, size in (
    ("initial_loading_entry", flow, initial, 0x823AA950, 0x94),
    ("complete_loading_entry", flow, complete, 0x823AABD0, 0x44),
    ("bundle_header", bundle, header, 0x828D7A90, 0xD4),
):
    obj = selected[source]
    body, relocs, refs = inspect(obj, symbol)
    prior_body, _, prior_refs = inspect(target[source], symbol)
    assert body == prior_body and refs == prior_refs
    ref = original(artist, size)
    calls = {offset: name for offset, kind, name in refs if kind == 6}
    if label == "initial_loading_entry":
        words(ref, 0x70, "7feA492e 39400001 93fe0008 93fe0004 9bfe000c")
        words(body, 0x6C, "7fea492e 93fe0008 93fe0004 9bfe000c")
        assert calls[0x88].startswith("?SetSaveLoadState@")
        assert not any("StrStreamBase" in name for name in calls.values())
    elif label == "complete_loading_entry":
        words(ref, 0x20, "996ae28e 3d608301 814b02d0 7d0a492e 816b02d0 7ccb39ae 39600000 99630004")
        baseline = objects(wave / "r141-complete-loading")[source]
        before, _, before_refs = inspect(baseline, symbol)
        assert body == before and refs == before_refs
        assert calls[0x48].startswith("?SetVideoState@")
    else:
        words(ref, 0x48, "81780668")
        words(ref, 0x54, "2b0b0002")
        words(ref, 0x70, "419a005c")
        words(ref, 0xCC, "81780664")
        original_literal = ns["data_target"](ref, 0x8C, 0xA0)
        assert original(original_literal, 17) == b"Invalid platform\0"
        words(body, 0x3C, "817e0668 2b0b0002")
        words(body, 0x4C, "419a0020")
        words(body, 0x6C, "817e0664")
        assert [calls[n].split("@")[0] for n in (0x50, 0x64, 0x68)] == [
            "?BeginAssert", "?FireAssert", "?EndAssert"]
        literal, = [ix for offset, ix, kind in relocs if offset == 0x54 and kind == 16]
        assert ns["symbol_literal"](obj.read_bytes(), literal) == "Invalid platform"
    record = {"artist": hex(artist), "object": str(obj), "object_sha256": ns["sha"](obj),
              "body_bytes": len(body), "direct_calls": calls}
    if link:
        va = address(symbol)
        actual = linked(va, len(body))
        ns["fixed_equal"](body, actual, relocs)
        for offset, name in calls.items():
            dest = ns["branch_target"](va + offset, struct.unpack_from(">I", actual, offset)[0])
            assert dest == address(name), (symbol, hex(offset), name)
        if label == "bundle_header":
            string = ns["data_target"](actual, 0x54, 0x5C)
            assert linked(string, 17) == b"Invalid platform\0"
        record["linked_address"] = hex(va)
    records[label] = record

out = (link or here) / ("R141_LOADING_BUNDLE_LINKED.json" if link else "R141_LOADING_BUNDLE_COFF.json")
with out.open("x", encoding="utf-8") as stream:
    json.dump({"scope": "linked PPC" if link else "SDK PPC COFF", "records": records}, stream, indent=2)
    stream.write("\n")
print(out, ns["sha"](out))
