"""Verify the R161 native title pad producer, consumer and linked ABI."""
import argparse
import json
from pathlib import Path
import runpy
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
HEAD = "6e200ee072882789c16287a0508d2fffc866b709"
TREE = "7b6cf504297b59020b4d1f39e2e55e932917280f"
DONOR_HEAD = "ba110aa17f76e57b33ecffa9d9f91e0477b46a77"
DONOR = WAVE / "r161-native-inputpads"
DONOR_SHA = "7108ade3309da8890eb643b30bf7175e19bf056fbd822a00484a7bde23509c00"
REPORT = WAVE / "r161-composed-preflight/compile-results.json"
REPORT_SHA = "5d30ff198d76fd8de4c95673169329b52cb49f00c782aa2e2b4d3cf0e8fda889"
H = runpy.run_path(str(WAVE / "r160-gui-linked/verify_gui.py"))
IMAGE, COFF, load, sha, digest, word, pe_bytes, split_address, tree, source_bytes, inspect = (
    H[key] for key in ("IMAGE", "COFF", "load", "sha", "digest", "word", "pe_bytes",
                       "split_address", "tree", "source_bytes", "inspect")
)
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd, u32

GAME = "src/GameSource/Game/BrnGameModule.cpp"
FLOW = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
BOOT = "src/GameSource/Gui/Flow/HUD/States/BrnBootLegal.cpp"
MODULE = "src/GameShared/GameClasses/Input/CgsInputModule.cpp"
PADS = "src/GameShared/GameClasses/System/Input/CgsInputPads.cpp"
VTABLE = "??_7InputModule@CgsInput@@6B@"
PRE = "?PreWorldUpdate@InputModule@CgsInput@@"
UPDATE = "?Update@InputPads@CgsInput@@"
ENTRY = "?DoUpdate_InputPreWorld@BrnGameModule@BrnGame@@"
LOCK_W, LOCK_R = "?LockForWrite@IOBuffer@", "?LockForRead@IOBuffer@"
UNLOCK_W, UNLOCK_R = "?UnlockForWrite@IOBuffer@", "?UnlockForRead@IOBuffer@"

CHECKS = {
    "input_vptr_install": (GAME, "??0BrnGameModule@BrnGame@@", {}, {
        0xB4: 0x3FDF006F, 0xBC: 0x3BDE9630, 0xF0: 0x917E0000,
    }),
    "default_action_mapping": (GAME, "?PrepareInitialInputMapping@BrnGameModule@", {
        0x44: LOCK_W, 0x58: "?PostMappingRequest@PostWorldInputBuffer@InputIO@CgsInput@@", 0x80: UNLOCK_W,
    }, {0x4C: 0x38A0FFFF}),
    "title_start_press": (GAME, ENTRY, {
        0xDC: LOCK_R, 0x138: "?GetPadInfo@OutputBuffer@InputIO@CgsInput@@QBAPBU",
        0x16C: UNLOCK_R,
    }, {
        0x88: 0x3C7F006F, 0x90: 0x38639630, 0xA8: 0x7D6B0034, 0xAC: 0x5568DFFE,
        0xB0: 0x81630000, 0xB4: 0x816B0044, 0xB8: 0x7D6903A6, 0xBC: 0x4E800421,
        0xC4: 0x3B800004, 0xD0: 0x2F0B0001, 0xD4: 0x409A009C,
        0x13C: 0x8163005C, 0x140: 0x556B07BC, 0x144: 0x2B0B0000,
        0x148: 0x409A0014, 0x150: 0x2B1E0004, 0x154: 0x4198FFDC,
        0x15C: 0x7FDCF378, 0x160: 0x39600002, 0x164: 0x917D0000, 0x18C: 0x7F83E378,
    }),
    "input_pre_world": (MODULE, PRE, {
        0x20: LOCK_W, 0x28: LOCK_R, 0x38: "?ProcessRumbleRequests@InputModule@",
        0x40: UNLOCK_R, 0x58: UPDATE, 0x64: "?GetBindResultQueue@OutputBuffer@",
        0x78: "?GetUnbindResultQueue@OutputBuffer@", 0x94: UNLOCK_W,
    }, {0x44: 0x578B063E, 0x48: 0x2B0B0000, 0x4C: 0x419A0010, 0x50: 0x7FC4F378, 0x54: 0x387F0230}),
    "pad_action_edges": (PADS, UPDATE, {
        0x18: "?UpdateConnectedDevices@ManagerX360@", 0x88: "?Update@ManagerX360@",
        0xF4: "?GetPadInfo@OutputBuffer@InputIO@CgsInput@@QAAPAU", 0x1A0: "?FillRawData@InputPads@",
        0x1FC: "?FillRawData@InputPads@", 0x2FC: "?FillKeyBindings@AutoTestManager@",
    }, {
        0x94: 0x220B0001, 0x308: 0x1D6B0070, 0x318: 0x38CA097D, 0x330: 0x813B0978,
        0x338: 0x8906FFFF, 0x354: 0x9949097C, 0x368: 0x38A00000,
        0x378: 0x554A07FA, 0x388: 0x554A003C, 0x39C: 0x38A00000, 0x3A4: 0x554A07B8,
        0x558: 0x38E70004, 0x560: 0x396B0020, 0x564: 0x38C60004, 0x568: 0x2B070070,
        0x56C: 0x4198FDB0, 0x574: 0x98BA03A0, 0x578: 0x2F1C0004, 0x57C: 0x4198FB54,
        0x580: 0x921B0978,
    }),
    "loading_event_143": (FLOW, "?Update@LoadingScriptedState@@", {
        0x41C: ENTRY, 0x688: "?GetGuiEvents@InputBuffer@CgsGuiModuleIO@",
        0x698: "?AddEvent@?$VariableEventQueue@$0IAAA@$0BA@@CgsModule@@",
        0x6BC: "?BridgeControllerToGui@BrnGameModule@",
    }, {
        0x420: 0x7C711B78, 0x43C: 0x922100B4, 0x678: 0x2B110004, 0x67C: 0x419A0020,
        0x684: 0x3A4100B4, 0x68C: 0x7E449378, 0x690: 0x38A0008F, 0x694: 0x38C00004,
    }),
    "boot_legal_observer": (BOOT, "?Update@BootLegal@BrnGui@@", {}, {
        0x98: 0x3963FFFA, 0x9C: 0x2B0B0089, 0xAC: 0x5560103A, 0xB8: 0x4E800420,
        0x32C: 0x7F13C378, 0x8CC: 0x566B063E, 0x8D4: 0x419A0030,
        0x8D8: 0x39600008, 0x8DC: 0x917F0048,
    }),
}
V2_HEAD = "dfbb33b7725438f842ecec86f8968dcb94683380"
V2_TREE = "5ca848d87b3f96b324a3fce6e296a911313064c7"
V2_REPORT = WAVE / "r161-composed-preflight-v2/compile-results.json"
V2_REPORT_SHA = "84664f2a770b809926cefe60a3f7e1f4d45eab5ba990e6c566d1b691d05f6a53"
IO = "src/GameShared/GameClasses/System/Input/CgsInputModuleIO.cpp"
PAD_INFO = "?GetPadInfo@OutputBuffer@InputIO@CgsInput@@"
V2_IO = {
    "mutable_pad_record": (IO, PAD_INFO + "QAAPAU", {}, {
        0x20: 0x556BEFFE, 0xFC: 0x1D7A03A4, 0x100: 0x7D6BDA14, 0x104: 0x386B0128,
    }),
    "const_pad_record": (IO, PAD_INFO + "QBAPBU", {}, {
        0x18: 0x556BE7FE, 0x90: 0x1D7D03A4, 0x94: 0x7D6BF214, 0x98: 0x386B0128,
    }),
}
ARTIST_WORDS = {
    0x820CF68C: 0x82903328, 0x820CF690: 0x828F8478,
    0x823C570C: 0x3C7F006F, 0x823C5714: 0x38639630, 0x823C5738: 0x816B0044,
    0x823C5740: 0x4E800421, 0x823C57CC: 0x8163005C, 0x823C57D0: 0x556BFFFE,
    0x823C57E0: 0x2B1E0004, 0x823C57F0: 0x39600002,
    0x8290336C: 0x578B063E, 0x82903370: 0x2B0B0000, 0x82903374: 0x419A0010,
    0x82903378: 0x7FC4F378, 0x8290337C: 0x387F0230, 0x820F78E4: 0x3DCCCCCD,
    0x828F89CC: 0xFF0D0000, 0x828F89E4: 0x41980050, 0x828F89F4: 0x61290001,
    0x828F8A2C: 0x61290002, 0x828F8A70: 0x61290004, 0x828F8C98: 0x394A0004,
    0x828F8CA0: 0x396B0020, 0x828F8CA4: 0x2F0A0070, 0x828F8CB0: 0x98F903A0,
    0x828F8CB4: 0x2F1E0004, 0x828F8CC0: 0x216B0001, 0x828F8CC4: 0x917F0978,
    0x823F26F8: 0x7C711B78, 0x823F2974: 0x2B110004, 0x823F2980: 0x922100B8,
    0x823F2988: 0x38C00004, 0x823F298C: 0x38A0008F, 0x82CDBEC8: 0x082DFFFF,
    0x82477314: 0x3963FFFA, 0x82477318: 0x2B0B0089, 0x8247755C: 0x82477638,
    0x82477638: 0x9AC10050,
}
ARTIST_EDGES = {
    0x823F26F4: 0x823C5650, 0x823C57C8: 0x823B1230, 0x823C57D8: 0x823C57EC,
    0x82903348: 0x82204A18, 0x82903350: 0x82204C88, 0x82903360: 0x828FFE50,
    0x82903368: 0x82204D50, 0x82903380: 0x828F8690, 0x82903388: 0x828E6D28,
    0x82903398: 0x828E6DD0, 0x829033B4: 0x82204BB8,
    0x828F8CA8: 0x828F89C4, 0x828F8CB8: 0x828F8784,
    0x823F2978: 0x823F2998, 0x823F2994: 0x823C79F0, 0x823F29B8: 0x823E6B18,
}


def object_function(rows, label):
    source, prefix, _, _ = CHECKS[label]
    obj = COFF(Path(rows[source]["object"]).read_bytes())
    return obj, obj.function(prefix)


def extras(rows, donor, records, image=None, entries=None):
    native = {}
    obj, (_, _, refs) = object_function(rows, "input_vptr_install")
    assert refs[0xE0][0] == 16 and refs[0xE8][0] == 17
    assert refs[0xE0][1] == refs[0xE8][1] and refs[0xE0][1].name == VTABLE
    table_symbol = obj.definition(VTABLE)
    _, table_bytes, _, count = obj.section(table_symbol.section)
    assert len(table_bytes) == 0x4C and count == 19
    native["input_vtable"] = {}
    if image:
        label = records["input_vptr_install"]
        va = int(label["linked_va"], 16)
        table = split_address(pe_bytes(image, va, 0xF4), 0xE0, 0xE8)
        owner = Path(label["object"]).name
        assert (table, owner) in entries[VTABLE]
        native["input_vtable_va"] = hex(table)
    for key, expected in donor["input_module_virtual_slots"].items():
        offset = int(key, 16)
        assert obj.vtable_slot(VTABLE, offset) == expected
        item = {"coff_symbol": expected}
        if image:
            destination = image.word(table + offset)
            assert destination in {address for address, _ in entries[expected]}
            if offset == 0x44:
                assert destination == int(records["input_pre_world"]["linked_va"], 16)
            item.update(slot_va=hex(table + offset), target_va=hex(destination))
        native["input_vtable"][key] = item

    obj, (_, code, refs) = object_function(rows, "pad_action_edges")
    assert refs[0x90][0] == 16 and refs[0xA8][0] == 17 and refs[0x90][1] == refs[0xA8][1]
    assert obj.literal(refs[0x90][1], 4) == bytes.fromhex("3dcccccd")
    for lane in range(4):
        base = 0x31C + lane * 0x90
        compare = 8 if lane == 0 else 4
        for offset, value in ((compare, 0xFF00F800), (0xC, 0x40980008), (0x48, 0x614A0001),
                              (0x64, 0x614A0002), (0x7C, 0x614A0004)):
            assert word(code, base + offset) == value, (lane, hex(base + offset))
    native["edge_loop"] = {"actions": 112, "ports": 4, "threshold_word": "0x3dcccccd",
                           "held_mask": 1, "pressed_mask": 2, "released_mask": 4}
    if image:
        va = int(records["pad_action_edges"]["linked_va"], 16)
        threshold = split_address(pe_bytes(image, va, 0xAC), 0x90, 0xA8)
        assert image.word(threshold) == 0x3DCCCCCD
        native["edge_loop"]["threshold_va"] = hex(threshold)

    obj, (_, _, refs) = object_function(rows, "default_action_mapping")
    assert refs[0x48][0] == 16 and refs[0x50][0] == 17 and refs[0x48][1] == refs[0x50][1]
    mapping = rd(0x82CDBEB8, 112)
    assert obj.literal(refs[0x48][1], len(mapping)) == mapping
    native["default_mapping"] = {"size": 112, "sha256": digest(mapping), "start_row": mapping[16:20].hex()}
    if image:
        va = int(records["default_action_mapping"]["linked_va"], 16)
        address = split_address(pe_bytes(image, va, 0x54), 0x48, 0x50)
        assert pe_bytes(image, address, 112) == mapping
        native["default_mapping"]["va"] = hex(address)

    _, (symbol, _, refs) = object_function(rows, "boot_legal_observer")
    kind, target = refs[0xBC + 4 * (143 - 6)]
    assert kind == 2 and target.section == symbol.section and target.value == 0x32C
    native["boot_event_143"] = {"table_offset": "0x2e0", "target_offset": "0x32c"}
    if image:
        va = int(records["boot_legal_observer"]["linked_va"], 16)
        assert split_address(pe_bytes(image, va, 0xAC), 0xA4, 0xA8) == va + 0xBC
        assert image.word(va + 0x2E0) == va + 0x32C
        native["boot_event_143"].update(table_va=hex(va + 0x2E0), target_va=hex(va + 0x32C))
    return native


def preflight(v2):
    path = DONOR / "RECEIPT.json"
    assert sha(path) == DONOR_SHA
    donor = load(path)
    assert donor["commit"] == DONOR_HEAD and donor["tree"] == tree(DONOR_HEAD)
    compile_path = DONOR / "compile-receipt.json"
    assert sha(compile_path) == donor["compile_receipt_sha256"]
    report = load(compile_path)
    assert report["head"] == DONOR_HEAD and len(report["results"]) == donor["focused_ppc_compiles"] == 10
    for source, expected in report["input_sha256"].items():
        raw = source_bytes(DONOR_HEAD, source)
        assert expected in {digest(raw), digest(raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))}, source
    donor_rows = {}
    for item in report["results"]:
        assert item["exit_code"] == 0 and item["machine"] == "0x01F2"
        stem = Path(item["source"]).stem
        obj, asm = DONOR / (stem + ".obj"), DONOR / (stem + ".asm")
        assert sha(obj) == item["ppc_coff_sha256"] and sha(asm) == item["listing_sha256"]
        donor_rows[item["source"]] = {"object": obj, "assembly": asm, "sha256": item["ppc_coff_sha256"]}

    head, expected_tree, report_path, report_sha = (
        (V2_HEAD, V2_TREE, V2_REPORT, V2_REPORT_SHA) if v2 else (HEAD, TREE, REPORT, REPORT_SHA)
    )
    assert tree(head) == expected_tree and sha(report_path) == report_sha
    composed = load(report_path)
    assert composed["requested"] == composed["passed"] == 12 and composed["failed"] == 0
    rows = {item["source"]: item for item in composed["results"]}
    assert all(row["machine"] == 0x1F2 and row["passed"] and row["returncode"] == 0 for row in rows.values())
    for address, expected in ARTIST_WORDS.items():
        assert u32(address) == expected, hex(address)
    for address, target in ARTIST_EDGES.items():
        assert IMAGE.target(None, address, u32(address)) == target, hex(address)
    checked = {}
    for name, spec in CHECKS.items():
        before = inspect(name, spec, donor_rows, DONOR_HEAD)
        now = inspect(name, spec, rows, head)
        if name == "default_action_mapping" or (v2 and name == "pad_action_edges"):
            _, (_, old_code, old_refs) = object_function(donor_rows, name)
            _, (_, new_code, new_refs) = object_function(rows, name)
            old = H["canonical"](old_code, old_refs)
            new = H["canonical"](new_code, new_refs)
            assert len(old) == len(new)
            changes = {hex(i): [hex(word(old, i)), hex(word(new, i))]
                       for i in range(0, len(old), 4) if word(old, i) != word(new, i)}
            if name == "default_action_mapping":
                assert changes == {"0x2c": ["0x38a00bed", "0x38a00bf1"], "0x68": ["0x38a00bf0", "0x38a00bf4"]}
            else:
                assert changes == {"0x134": ["0x38a000d0", "0x38a00112"],
                                   "0x160": ["0x38a000d1", "0x38a00113"],
                                   "0x1b8": ["0x38a000da", "0x38a00120"]}
            now["donor_assert_line_changes"] = changes
        else:
            assert now["coff_masked_body_sha256"] == before["coff_masked_body_sha256"], name
        if spec[0] != GAME and not (v2 and spec[0] == PADS):
            assert now["git_blob"] == before["git_blob"], name
        now["donor_git_blob"] = before["git_blob"]
        checked[name] = now
    assert extras(donor_rows, donor, checked) == extras(rows, donor, checked)
    if v2:
        assert u32(0x828E7080) == u32(0x823B1390) == 0x1D7603A4
        assert u32(0x828E7088) == u32(0x823B1398) == 0x386B0128
        checked.update({name: inspect(name, spec, rows, head) for name, spec in V2_IO.items()})
    receipt = {"head": head, "tree": expected_tree, "donor_receipt_sha256": DONOR_SHA,
               "composed_report_sha256": report_sha,
               "artist_words": {hex(a): hex(v) for a, v in ARTIST_WORDS.items()},
               "artist_edges": {hex(a): hex(v) for a, v in ARTIST_EDGES.items()},
               "functions": checked, "native_details": extras(rows, donor, checked)}
    return receipt, donor


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v2", action="store_true")
    parser.add_argument("--root-pins", type=Path)
    parser.add_argument("--root-pins-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    receipt, donor = preflight(args.v2)
    checks = CHECKS | V2_IO if args.v2 else CHECKS
    if args.root_pins:
        assert sha(args.root_pins) == args.root_pins_sha256
        root = load(args.root_pins)
        head, pins = root["head"], root["pins"]
        assert root["tree"] == tree(head) == receipt["tree"]
        for key in ("seal", "invocation", "result", "pe", "map", "xex", "image", "completion"):
            assert sha(Path(pins[key]["path"])) == pins[key]["sha256"], key
        seal, invocation, result, completion, image_check = (
            load(Path(pins[key]["path"])) for key in ("seal", "invocation", "result", "completion", "image"))
        assert seal["head"] == completion["head"] == image_check["head"] == head
        assert invocation["sealed_sha256"] == pins["seal"]["sha256"] and invocation["objects"] == seal["selected"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        outputs = {Path(path).name: row for path, row in result["outputs"].items()}
        for key in ("pe", "map", "xex"):
            assert outputs[Path(pins[key]["path"]).name]["sha256"] == pins[key]["sha256"]
        assert image_check["magic"] == "XEX2" and image_check["image_sha256"] == pins["xex"]["sha256"]
        assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
        rows = {row["source"]: row for row in invocation["objects"]}
        map_path, pe_path = (Path(pins[key]["path"]) for key in ("map", "pe"))
        entries = H["TOOLS"]["PRIOR"]["HELPER"]["map_symbols"](map_path.read_bytes())
        image = IMAGE(pe_path, map_path)
        source_root = Path(seal["source_root"])
        sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
                   if Path(row["path"]).is_relative_to(source_root)}
        final = {name: inspect(name, spec, rows, head, (image, entries, sources)) for name, spec in checks.items()}
        for name, before in receipt["functions"].items():
            assert final[name]["coff_masked_body_sha256"] == before["coff_masked_body_sha256"], name
            assert final[name]["git_blob"] == before["git_blob"], name
        receipt["functions"] = final
        receipt["native_details"] = extras(rows, donor, final, image, entries)
        receipt["linked"] = {"head": head, "tree": receipt["tree"], "root_pins_sha256": args.root_pins_sha256, "pins": pins}
    receipt["scope"] = "Static native title input and event routing. Physical input and title progression remain unverified."
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(receipt["functions"]), "title input functions", sha(args.output))


if __name__ == "__main__":
    main()





