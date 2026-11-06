"""Verify R160's typed GUI bridge, Profile gate, and the two linked PPC callers."""
import argparse
import json
from pathlib import Path
import re
import runpy
import struct
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
DONOR = WAVE / "r160-game-flags"
DONOR_HEAD = "d9bd6a7e57538aba2d9e8bf45feb39601cd72a91"
DONOR_SHA = "9547a30b15ff20f8634fac8159de8577168ae750094d3c13bdf32501bb5eddd8"
HEAD = "d9fe6e6026013a76bb1fb987e7aa78a54affa569"
TREE = "8a22a7667f80a25663db24c362c61f019deab02e"
REPORT = WAVE / "r160-composed-preflight/compile-results.json"
REPORT_SHA = "1e1100b59387f6c23db570cb9633e0698db1f98f0c53ede0482304ba2043e3ad"
ARTIST_JSON = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x823DDB78.json")
TOOLS = runpy.run_path(str(WAVE / "r159-live-update-linked/verify_live.py"))
IMAGE, COFF, load, sha, digest, word, canonical, check_listing, pe_bytes, split_address, tree, blob, source_bytes = (
    TOOLS[key] for key in ("IMAGE", "COFF", "load", "sha", "digest", "word", "canonical", "check_listing",
                           "pe_bytes", "split_address", "tree", "blob", "source_bytes")
)
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import u32

GAME = "src/GameSource/Game/"
PROG = "src/GameSource/GameState/Progression/"
BRIDGE = GAME + "GameBridgeGUIToX_GameState.cpp"
GUI = "src/GameShared/GameClasses/Gui/CgsGuiModuleIO_OutputBuffer.cpp"
STATE_IO = "src/GameSource/GameState/BrnGameStateModuleIO.cpp"
BRIDGE_SIG = ("?BridgeGuiToGameState@BrnGameModule@BrnGame@@QAAHPAUPostWorldInputBuffer"
              "@GameStateModuleIO@BrnGameState@@PBUOutputBuffer@CgsGuiModuleIO@CgsGui@@@Z")
GUI_GET = "?GetOutEventQueue@OutputBuffer@CgsGuiModuleIO@CgsGui@@QBAPBV"
STATE_GET = "?GetGameEventQueue@PostWorldInputBuffer@GameStateModuleIO@BrnGameState@@QAAPAV"
PROFILE_GET = "?GetProfile@ProgressionManager@BrnProgression@@QAAPAV"
SEEN = "?HasPlayerSeenTrainingType@Profile@BrnProgression@@"
TRAIN = "?RequestTraining@TrainingManager@BrnGameState@@"
LOCK = "?LockBuffersForIO@CgsModule@@"
UNLOCK = "?UnlockBuffersForIO@CgsModule@@"
ADD = "?AddEvent@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@"
CHECKS = {
    "typed_bridge": (BRIDGE, BRIDGE_SIG, {
        0x1C: GUI_GET, 0x2C: STATE_GET,
        0x5C: "?GetFirstEvent@?$VariableEventQueue@$0EIAA@$0BA@@",
        0x3CC: PROFILE_GET, 0x3D4: SEEN, 0x3EC: TRAIN, 0x11B0: ADD,
        0x11C4: "?GetNextEvent@?$VariableEventQueue@$0EIAA@$0BA@@",
    }, {
        0x10: 0x7CA32B78, 0x14: 0x7C9F2378, 0x24: 0x7FE3FB78,
        0x30: 0x3ED90067, 0x3C: 0x3AD64B30, 0x44: 0x38810050, 0x60: 0x83810050,
        0x70: 0x3A400001, 0x118: 0x2F030148, 0x124: 0x2F0300C0,
        0x130: 0x3963FFBF, 0x134: 0x2B0B007E, 0x144: 0x5560103A,
        0x148: 0x7C0C002E, 0x14C: 0x7C0903A6, 0x150: 0x4E800420,
        0x3AC: 0x897C0000, 0x3B0: 0x7D6A0034, 0x3B4: 0x2B0B0000,
        0x3B8: 0x554BDFFE, 0x3BC: 0x996100A0, 0x3C0: 0x419A0030,
        0x3C4: 0x3C790067, 0x3C8: 0x38635030, 0x3D0: 0x38800029,
        0x3D8: 0x546B063E, 0x3DC: 0x2B0B0000, 0x3E0: 0x409A0010,
        0x3E4: 0x38800021, 0x3E8: 0x7EC3B378, 0x3F0: 0x3BE00063,
        0x3F4: 0x48000D98, 0x118C: 0x7E5E9378, 0x1190: 0x7E5D9378,
        0x119C: 0x419A0018, 0x11A0: 0x7FC6F378, 0x11A4: 0x7FE5FB78,
        0x11A8: 0x388100A0, 0x11AC: 0x7EE3BB78, 0x11C8: 0x83810050,
        0x11D0: 0x409AEF3C,
    }),
    "game_post_world_caller": (GAME + "BrnGameModule.cpp", "?DoUpdate_GameStatePostWorld@BrnGameModule@", {
        0xAC: LOCK, 0xBC: BRIDGE_SIG, 0x1E8: UNLOCK,
    }, {0xB0: 0x7F65DB78, 0xB4: 0x7FE4FB78, 0xB8: 0x7FC3F378}),
    "loading_scripted_caller": ("src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp", "?Update@LoadingScriptedState@@", {
        0x4A0: LOCK, 0x4B0: BRIDGE_SIG, 0x4BC: UNLOCK,
    }, {0x4A4: 0x7F45D378, 0x4A8: 0x7EE4BB78, 0x4AC: 0x7FA3EB78}),
    "profile_getter": (PROG + "BrnProgressionManager.cpp", PROFILE_GET, {}, {
        0x00: 0x38630170, 0x04: 0x4E800020,
    }),
    "training_seen_accessor": (PROG + "BrnProfile.cpp", SEEN, {}, {
        0x6C: 0x7BEA06A0, 0x70: 0x39200001, 0x74: 0x57EBD1BE, 0x78: 0x396B3998,
        0x7C: 0x556B1838, 0x80: 0x7D6BE82A, 0x84: 0x7D2A5036, 0x88: 0x7D4B5838,
        0x8C: 0x2B2B0000, 0x90: 0x39600001, 0x94: 0x409A0008, 0x98: 0x39600000,
    }),
}
PROVIDERS = {
    "gui_read_queue": (GUI, GUI_GET, {}, {0x18: 0x556BE7FE, 0x44: 0x387F0814}),
    "game_state_write_queue": (STATE_IO, STATE_GET, {}, {
        0x18: 0x556BEFFE, 0x44: 0x3C7F0001, 0x48: 0x3863A4B0,
    }),
}
ARTIST_WORDS = {
    0x823DDC4C: 0x3D600067, 0x823DDC50: 0x617E4B30, 0x823DDC6C: 0x3963FFBF,
    0x823DDC70: 0x2B0B007E, 0x823DDC78: 0x3D80823E, 0x823DDC7C: 0x398CDC90,
    0x823DDC80: 0x5560103A, 0x823DDC84: 0x7C0C002E, 0x823DDC8C: 0x4E800420,
    0x823DDD04: 0x823DDFA0, 0x823DDFA0: 0x897F0000, 0x823DDFA4: 0x7D6A0034,
    0x823DDFA8: 0x2B0B0000, 0x823DDFAC: 0x554BDFFE, 0x823DDFB0: 0x99610050,
    0x823DDFB8: 0x3D600069, 0x823DDFBC: 0x39800001, 0x823DDFC0: 0x616B1E60,
    0x823DDFC4: 0x798C4FE6, 0x823DDFC8: 0x7D6E582A, 0x823DDFCC: 0x7D6B6038,
    0x823DDFEC: 0x38800021, 0x823DDFF8: 0x38C00001, 0x823DDFFC: 0x38A00063,
    0x823DE000: 0x38810050, 0x827E5F98: 0x3C7F0067, 0x827E5F9C: 0x38639500,
    0x827E45FC: 0x3C7E0001, 0x827E4600: 0x3863BB30, 0x8237A6D4: 0x387F0170,
    0x8231C978: 0x578BD1BE, 0x8231C97C: 0x396B3998, 0x8231C980: 0x556B1838,
    0x8231C984: 0x7D6BD82A, 0x8231C988: 0x7C6A5036, 0x8231C98C: 0x7D4B5838,
    0x823B4144: 0x556BE7FE, 0x823B41C8: 0x387C0814, 0x823B91C4: 0x556BEFFE,
    0x823B9248: 0x3C7C0001, 0x823B924C: 0x3863A4B0,
}
ARTIST_EDGES = {
    0x823DDB94: 0x823B4130, 0x823DDBA4: 0x823B91B0, 0x823DDBB8: 0x82360FD8,
    0x823DDFB4: 0x823DDFF8, 0x823DDFE8: 0x823DDFF8, 0x823DDFF4: 0x82365B20,
    0x823DE004: 0x823DEE88, 0x823DEE8C: 0x822E5548, 0x823DEEA0: 0x8236A5E0,
    0x823E9358: 0x823B7620, 0x823E9368: 0x823DDB78, 0x823E9494: 0x823B7760,
    0x823F2778: 0x823B6FE0, 0x823F2788: 0x823DDB78, 0x823F2794: 0x823B7060,
}
BRANCHES = {0x3C0: 0x3F0, 0x3E0: 0x3F0, 0x3F4: 0x118C, 0x119C: 0x11B4, 0x11D0: 0x10C}


def inspect(label, spec, rows, head, linked=None):
    source, prefix, edges, words = spec
    row = rows[source]
    obj_path = Path(row["object"])
    payload = obj_path.read_bytes()
    object_sha = digest(payload)
    assert row.get("sha256", object_sha) == object_sha
    obj = COFF(payload)
    symbol, code, refs = obj.function(prefix)
    record = {"source": source, "git_blob": blob(head, source), "object": str(obj_path),
              "object_sha256": object_sha, "symbol": symbol.name,
              "coff_masked_body_sha256": digest(canonical(code, refs)), "calls": {},
              "words": {hex(offset): hex(value) for offset, value in words.items()}}
    for offset, expected in words.items():
        assert offset not in refs and word(code, offset) == expected, (label, hex(offset), hex(word(code, offset)))
    if linked:
        image, entries, sources = linked
        src = sources[source.lower()]
        materialized = Path(src["path"]).read_bytes()
        assert digest(materialized) == src["sha256"]
        assert materialized.replace(b"\r\n", b"\n") == source_bytes(head, source).replace(b"\r\n", b"\n")
        [va] = {address for address, owner in entries[symbol.name] if owner.lower() == obj_path.name.lower()}
        linked_code = pe_bytes(image, va, len(code))
        assert canonical(linked_code, refs) == canonical(code, refs), label
        record.update(linked_va=hex(va), linked_body_sha256=digest(linked_code), source_sha256=src["sha256"])
    else:
        listing = Path(row["assembly"])
        check_listing(listing, symbol.name, code, refs)
        record.update(listing=str(listing), listing_sha256=sha(listing))
    for offset, prefix in edges.items():
        kind, target = refs[offset]
        assert kind == 6 and target.name.startswith(prefix), (label, hex(offset), target.name)
        if prefix == BRIDGE_SIG:
            assert target.name == BRIDGE_SIG
        call = {"symbol": target.name}
        if linked:
            site = va + offset
            raw = image.word(site)
            assert raw >> 26 == 18 and raw & 1
            address = image.target(site, raw)
            owners = sorted({owner for destination, owner in entries[target.name] if destination == address})
            assert owners, (label, hex(site), target.name, hex(address))
            call.update(site_va=hex(site), target_va=hex(address), owners=owners)
        record["calls"][hex(offset)] = call
    if label == "typed_bridge":
        assert symbol.name == BRIDGE_SIG
        assert [row.name for row in obj.symbols.values()
                if row.section > 0 and row.name.startswith("?BridgeGuiToGameState@")] == [BRIDGE_SIG]
        assert refs[0x13C][0] == 16 and refs[0x140][0] == 17
        assert refs[0x13C][1] == refs[0x140][1] and refs[0x13C][1].value == 0x154
        slot = 0x154 + 4 * (94 - 65)
        kind, target = refs[slot]
        assert kind == 2 and target.section == symbol.section and target.value == 0x3AC
        record["tag_94"] = {"table_offset": hex(slot), "target_offset": hex(target.value)}
        for site, target in BRANCHES.items():
            assert IMAGE.target(None, site, word(code, site)) == target
            if linked:
                assert image.target(va + site, image.word(va + site)) == va + target
        if linked:
            assert split_address(linked_code, 0x13C, 0x140) == va + 0x154
            assert image.word(va + slot) == va + 0x3AC
            assert [name for name in entries if name.startswith("?BridgeGuiToGameState@")] == [BRIDGE_SIG]
            record["tag_94"].update(table_va=hex(va + slot), target_va=hex(va + 0x3AC))
    return record


def direct_callers(image, target):
    [text] = [row for row in image.sections if row[0].rstrip(b"\0") == b".text"]
    _, size, start, _, offset = text
    sites = []
    for i, (raw,) in enumerate(struct.iter_unpack(">I", image.data[offset:offset + size])):
        if raw >> 26 in (16, 18):
            pc = image.base + start + 4 * i
            if image.target(pc, raw) == target:
                sites.append(hex(pc))
    return sites


def preflight():
    donor_path = DONOR / "RECEIPT.json"
    assert sha(donor_path) == DONOR_SHA
    donor = load(donor_path)
    assert donor["commit"] == DONOR_HEAD and donor["tree"] == tree(DONOR_HEAD)
    compile_path = DONOR / "compile-receipt.json"
    assert sha(compile_path) == donor["compile_receipt_sha256"]
    original = load(ARTIST_JSON)
    assert sha(ARTIST_JSON) == donor["artist_bridge_sha256"]
    assert "result == 192" in original["pseudocode"] and "result == 328" in original["pseudocode"]
    compiled = load(compile_path)
    for source, expected in compiled["input_sha256"].items():
        raw = source_bytes(DONOR_HEAD, source)
        assert expected in {digest(raw), digest(raw.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))}, source
    donor_rows = {}
    assert len(compiled["results"]) == donor["focused_ppc_compiles"] == 5
    for item in compiled["results"]:
        stem = Path(item["source"]).stem
        obj, asm = DONOR / (stem + ".obj"), DONOR / (stem + ".asm")
        assert item["exit_code"] == 0
        assert sha(obj) == item["ppc_coff_sha256"] and sha(asm) == item["listing_sha256"]
        donor_rows[item["source"]] = {"object": obj, "assembly": asm, "sha256": item["ppc_coff_sha256"]}

    assert tree(HEAD) == TREE and sha(REPORT) == REPORT_SHA
    composed = load(REPORT)
    assert composed["requested"] == composed["passed"] == 7 and composed["failed"] == 0
    rows = {item["source"]: item for item in composed["results"]}
    assert all(row["machine"] == 0x1F2 and row["passed"] and row["returncode"] == 0 for row in rows.values())
    source = source_bytes(HEAD, BRIDGE).decode()
    cases = lambda text: {int(value) for value in re.findall(r"^\s*case (\d+):", text, re.MULTILINE)}
    assert cases(source) == cases(original["pseudocode"]) | {192, 328}
    assert len(cases(source)) == donor["case_count_including_nested_selectors"] == 71
    assert all(marker not in source for marker in ("TranslateGuiEventsToGameState", "liResult == 40",
                                                     "lpuRecord", "lu64GameFlags"))
    assert "switch (liResult)" in source
    for address, expected in ARTIST_WORDS.items():
        assert u32(address) == expected, hex(address)
    for address, target in ARTIST_EDGES.items():
        assert IMAGE.target(None, address, u32(address)) == target
    assert 0x669500 + 0xBB30 + 0x170 + 0x3998 * 8 == 0x691E60

    checked = {}
    for label, spec in CHECKS.items():
        first = inspect(label, spec, donor_rows, DONOR_HEAD)
        final = inspect(label, spec, rows, HEAD)
        assert first["coff_masked_body_sha256"] == final["coff_masked_body_sha256"], label
        assert first["git_blob"] == final["git_blob"], label
        checked[label] = final
    return {
        "head": HEAD, "tree": TREE, "donor_receipt_sha256": DONOR_SHA,
        "composed_report_sha256": REPORT_SHA, "artist_bridge_export_sha256": donor["artist_bridge_sha256"],
        "artist_words": {hex(address): hex(value) for address, value in ARTIST_WORDS.items()},
        "artist_edges": {hex(address): hex(target) for address, target in ARTIST_EDGES.items()},
        "case_selectors": sorted(cases(source)), "functions": checked,
        "case_94": {"profile_word": "0x691e60", "seen_bit": 41, "training_request": 33,
                    "event_tag": 99, "event_size": 1},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins", type=Path)
    parser.add_argument("--root-pins-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    receipt = preflight()
    if args.root_pins:
        assert sha(args.root_pins) == args.root_pins_sha256
        root_pins = load(args.root_pins)
        head = root_pins["head"]
        assert root_pins["tree"] == tree(head) == TREE
        files = root_pins["pins"]
        for key in ("seal", "invocation", "result", "pe", "map", "xex", "image", "completion"):
            assert sha(Path(files[key]["path"])) == files[key]["sha256"], key
        seal, invocation, result, completion, image_check = (
            load(Path(files[key]["path"])) for key in ("seal", "invocation", "result", "completion", "image"))
        assert seal["head"] == completion["head"] == image_check["head"] == head
        assert invocation["sealed_sha256"] == files["seal"]["sha256"] and invocation["objects"] == seal["selected"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        outputs = {Path(path).name: row for path, row in result["outputs"].items()}
        for key in ("pe", "map", "xex"):
            assert outputs[Path(files[key]["path"]).name]["sha256"] == files[key]["sha256"]
        assert image_check["magic"] == "XEX2" and image_check["image_sha256"] == files["xex"]["sha256"]
        assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
        rows = {row["source"]: row for row in invocation["objects"]}
        map_path, pe_path = (Path(files[key]["path"]) for key in ("map", "pe"))
        entries = TOOLS["PRIOR"]["HELPER"]["map_symbols"](map_path.read_bytes())
        image = IMAGE(pe_path, map_path)
        source_root = Path(seal["source_root"])
        sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
                   if Path(row["path"]).is_relative_to(source_root)}
        final = {name: inspect(name, spec, rows, head, (image, entries, sources))
                 for name, spec in (CHECKS | PROVIDERS).items()}
        for name, before in receipt["functions"].items():
            assert final[name]["coff_masked_body_sha256"] == before["coff_masked_body_sha256"], name
            assert final[name]["git_blob"] == before["git_blob"], name
        callers = direct_callers(image, int(final["typed_bridge"]["linked_va"], 16))
        assert set(callers) == {final["game_post_world_caller"]["calls"]["0xbc"]["site_va"],
                                final["loading_scripted_caller"]["calls"]["0x4b0"]["site_va"]}
        assert len(callers) == 2
        receipt["functions"] = final
        receipt["linked"] = {"head": head, "tree": TREE, "root_pins_sha256": args.root_pins_sha256,
                             "pins": files, "direct_text_callers": callers}
    receipt["scope"] = "Static typed native queue dispatch and Profile case-94 gate. Runtime GUI behavior remains unverified."
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(receipt["functions"]), "functions", sha(args.output))


if __name__ == "__main__":
    main()

