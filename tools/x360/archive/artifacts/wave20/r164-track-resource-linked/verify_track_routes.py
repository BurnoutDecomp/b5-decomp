
"""Verify final pinned R164 GameData track routing in selected COFF and the linked PE."""
import argparse
import json
from pathlib import Path
import re
import runpy
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
CANDIDATE = "4da663341e39dd897eecce5642712dbd7b2c4b5e"
TREE = "4db7a0e79c33f4ebc6ed8058ee2e63edbc4b3cd3"
PARENT = "b21b1aaaaf795d81aaf304b808ac47d7b98fe59c"
COMPOSED_SHA = "9d2f60665d2184a352f06f338e29e729ab6c9de99d18ee8ecb9ec2ab72a648ab"
DONOR_SHA = "3919e876d72101bbca42db6116f008bddb0553e9aa4b28719961fa943d352fb1"
BINDING_SHA = "0a5333e4bc037d508f7f123635e75148df4d98d45ced47df777a6837acf5a173"
H = runpy.run_path(str(WAVE / "r160-gui-linked/verify_gui.py"))
IMAGE, COFF, load, sha, digest, word, canonical, pe_bytes, split_address, tree, blob, inspect = (
    H[key] for key in ("IMAGE", "COFF", "load", "sha", "digest", "word", "canonical",
                       "pe_bytes", "split_address", "tree", "blob", "inspect")
)
map_symbols = H["TOOLS"]["PRIOR"]["HELPER"]["map_symbols"]
git = H["TOOLS"]["git"]
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd, u32

LITERALS = {
    "ProcessGetPVSRequest": (0x8C, 0x98, 0x82F2A72C),
    "ProcessGetWorldCollisionRequest": (0xE0, 0xEC, 0x82F2A728),
    "ProcessUnloadPVSRequest": (0x8C, 0x94, 0x82F2A70C),
    "ProcessUnloadWorldCollisionRequest": (0xE0, 0xE8, 0x82F2A708),
}
TABLES = {
    "ProcessInternalLoadBundleResponse": (0x58, 0x5C, 0x70, 27, {32: 0x2E8, 33: 0x368}),
    "ProcessInternalAcquireResponse": (0xA8, 0xAC, 0xC0, 40, {57: 0x378, 58: 0x2EC}),
    "PostGameDataResponse": (0x30, 0x34, 0x48, 27, {58: 0x274}),
}
EXTRA_CALLS = {
    "ProcessInternalAcquireResponse": {0x84: "?PushIndex@", 0x94: "?LockForWrite@IOBuffer@"},
    "?FireTrackResourceAssert@": {0x2C: "??0StrStream@CgsDev@@", 0x38: "??6StrStreamBase@CgsDev@@"},
}


def prefix(label):
    return label if label.startswith("?") else f"?{label}@GameDataModule@BrnResource@@"


def normal_symbol(name):
    return re.sub(r"\?A0x[0-9a-fA-F]+@", "?ANONYMOUS@", name)


def string_at(address):
    return rd(address, 192).split(bytes(1))[0] + bytes(1)


def raw_pair(high, low):
    return split_address(rd(high, low - high + 4), 0, low - high)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins", type=Path, required=True)
    parser.add_argument("--root-pins-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assert sha(args.root_pins) == args.root_pins_sha256.lower()
    root = load(args.root_pins)
    head, pins = root["head"], root["pins"]
    assert root["tree"] == tree(head) == tree(CANDIDATE) == TREE
    assert root["parent"] == git("rev-parse", head + "^").decode().strip() == PARENT
    for item in pins.values():
        assert sha(Path(item["path"])) == item["sha256"]
    seal, invocation, result, completion, image_check, diagnostics = (
        load(Path(pins[key]["path"])) for key in ("seal", "invocation", "result", "completion", "image", "diagnostics"))
    assert seal["head"] == completion["head"] == image_check["head"] == head
    assert invocation["sealed_sha256"] == pins["seal"]["sha256"] and invocation["objects"] == seal["selected"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
    assert diagnostics["unresolved_messages"] == 0 and not diagnostics["duplicate_messages"]
    assert image_check["magic"] == "XEX2" and image_check["image_sha256"] == pins["xex"]["sha256"]
    outputs = {Path(path).name: item for path, item in result["outputs"].items()}
    for key in ("pe", "map", "xex"):
        assert outputs[Path(pins[key]["path"]).name]["sha256"] == pins[key]["sha256"]

    composed_path = WAVE / "r164-batch/COMPOSED_PPC_CHECK.json"
    binding_path = WAVE / "r164-batch/TARGETED_OBJECT_BINDING.json"
    assert sha(composed_path) == COMPOSED_SHA and sha(binding_path) == BINDING_SHA
    composed, binding = load(composed_path), load(binding_path)
    assert composed["head"] == binding["head"] == CANDIDATE and composed["tree"] == binding["tree"] == TREE
    assert composed["targeted_binding_sha256"] == BINDING_SHA and binding["parent"] == PARENT
    donor_path = Path(composed["track_donor_receipt"]["path"])
    assert composed["track_donor_receipt"]["sha256"] == sha(donor_path) == DONOR_SHA
    donor = load(donor_path)
    assert set(composed["track_routes"]) == set(donor["compiled"])
    for site, target in donor["artist_branches"].items():
        address = int(site, 16)
        assert IMAGE.target(None, address, u32(address)) == int(target, 16)

    before_rows = {row["source"]: row for row in binding["results"]}
    selected = {row["source"]: row for row in invocation["objects"]}
    source_root = Path(seal["source_root"])
    sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
               if Path(row["path"]).is_relative_to(source_root)}
    map_path, pe_path = (Path(pins[key]["path"]) for key in ("map", "pe"))
    entries = map_symbols(map_path.read_bytes())
    image = IMAGE(pe_path, map_path)
    out, objects, source_path_relocations = {}, {}, {}

    for label, expected in donor["compiled"].items():
        composition = composed["track_routes"][label]
        source = composition["source"]
        bound = before_rows[source]
        previous = bound["artifacts"]["object"]
        assert bound["git_blob"] == blob(CANDIDATE, source) == blob(head, source)
        assert sha(Path(previous["path"])) == previous["sha256"]
        previous_obj = COFF(Path(previous["path"]).read_bytes())
        _, before, before_refs = previous_obj.function(prefix(label))
        assert len(before) == composition["bytes"]
        assert len(expected["words"]) == composition["operand_count"]
        assert len(expected["relocations"]) == composition["relocation_count"]

        obj = objects.setdefault(source, COFF(Path(selected[source]["object"]).read_bytes()))
        symbol, code, refs = obj.function(prefix(label))
        assert canonical(code, refs) == canonical(before, before_refs)
        for offset, (kind, dest) in before_refs.items():
            assert refs[offset][0] == kind
            resolved = refs[offset][1]
            if normal_symbol(resolved.name) != normal_symbol(dest.name):
                assert label == "?Prepare@PVSModule@" and offset in (0x38, 0x40)
                assert kind == (16 if offset == 0x38 else 17)
                suffix = r"\src\GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h"
                original = previous_obj.section(dest.section)[1][dest.value:].split(bytes(1))[0].decode("ascii")
                retained = obj.section(resolved.section)[1][resolved.value:].split(bytes(1))[0].decode("ascii")
                assert original.endswith(r"\native_resources" + suffix)
                assert retained.endswith(r"\wave4-intake" + suffix)
                source_path_relocations[f"{label}+{hex(offset)}"] = {"composed": original, "selected": retained}
        exact_edges = {}
        for at, edge in expected["relocations"].items():
            offset = int(at, 16)
            kind, target = refs[offset]
            assert kind == edge["type"] == 6
            assert normal_symbol(target.name) == normal_symbol(edge["symbol"])
            exact_edges[offset] = target.name
        for offset, name in EXTRA_CALLS.get(label, {}).items():
            assert refs[offset][0] == 6 and refs[offset][1].name.startswith(name)
            exact_edges[offset] = refs[offset][1].name
        words = {int(at, 16): int(value, 16) for at, value in expected["words"].items()}
        out[label] = inspect(label, (source, prefix(label), exact_edges, words),
                             selected, head, (image, entries, sources))

    def linked_body(label):
        source = out[label]["source"]
        obj = objects[source]
        symbol, code, refs = obj.function(prefix(label))
        va = int(out[label]["linked_va"], 16)
        return obj, symbol, va, pe_bytes(image, va, len(code)), refs

    def linked_pair(label, high, low):
        obj, _, _, code, refs = linked_body(label)
        assert refs[high][0] == 16 and refs[low][0] == 17 and refs[high][1] == refs[low][1]
        return obj, refs[high][1], split_address(code, high, low)

    def prove_literal(label, high, low, raw):
        obj, target, address = linked_pair(label, high, low)
        assert obj.literal(target, len(raw)) == pe_bytes(image, address, len(raw)) == raw
        return {"va": hex(address), "text": raw[:-1].decode("ascii")}

    literals = {}
    for label, (high, low, original) in LITERALS.items():
        raw = string_at(u32(original))
        assert donor["original_resource_literals"][f"0x{original:X}"] == raw[:-1].decode("ascii")
        literals[label] = prove_literal(label, high, low, raw)
    for label, high, low, art_hi, art_lo in (
        ("ProcessInternalAcquireResponse", 0x378, 0x380, 0x82673DDC, 0x82673DE4),
        ("ProcessInternalUnloadResponse", 0xD8, 0xE0, 0x8266E4EC, 0x8266E4F4),
        ("?FireTrackResourceAssert@", 0x3C, 0x48, 0x8266E4B0, 0x8266E4B4),
    ):
        literals[label] = prove_literal(label, high, low, string_at(raw_pair(art_hi, art_lo)))

    switch_cases = {}
    for label, (high, low, table, first, cases) in TABLES.items():
        _, symbol, va, _, refs = linked_body(label)
        _, table_symbol, table_va = linked_pair(label, high, low)
        assert table_symbol.section == symbol.section and table_symbol.value == table
        assert table_va == va + table
        for case, target in cases.items():
            offset = table + 4 * (case - first)
            kind, address = refs[offset]
            assert kind == 2 and address.section == symbol.section and address.value == target
            assert image.word(va + offset) == va + target
            switch_cases[f"{label}:{case}"] = {"slot_va": hex(va + offset), "target_va": hex(va + target)}

    _, setter, setter_va, _, _ = linked_body("?SetPVSHandle@GetPVSResponse@")
    _, pair, setter_reference = linked_pair("PostGameDataResponse", 0x274, 0x27C)
    assert pair.name == setter.name and setter_reference == setter_va
    _, _, va, code, _ = linked_body("ProcessInternalAcquireResponse")
    assert IMAGE.target(None, va + 0x6C, word(code, 0x6C)) == va + 0x90
    assert word(code, 0x394) >> 26 == 18 and word(code, 0x394) & 1 == 0
    assert IMAGE.target(None, va + 0x394, word(code, 0x394)) in {
        address for address, _ in entries["__restgprlr_25"]}
    continuation = {"receiver_present_57": [hex(va + n) for n in (0x378, 0x384, 0x38C, 0x394)],
                    "no_receiver_recycle_call_va": hex(va + 0x84), "pvs_handle_setter_va": hex(setter_va)}

    receipt = {"head": head, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
               "root_pins_sha256": args.root_pins_sha256.lower(), "pins": pins,
               "binding_sha256": BINDING_SHA, "composed_ppc_sha256": COMPOSED_SHA,
               "donor_receipt_sha256": DONOR_SHA, "functions": out, "literal_operands": literals,
               "linked_switch_cases": switch_cases, "continuations": continuation,
               "retained_diagnostic_path_relocations": source_path_relocations,
               "scope": "Selected PPC COFF and pinned linked PE. Runtime unverified."}
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(out), "GameData track-route functions", sha(args.output))


if __name__ == "__main__":
    main()

