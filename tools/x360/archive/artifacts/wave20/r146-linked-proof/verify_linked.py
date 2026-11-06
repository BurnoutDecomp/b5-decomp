"""Bind the frozen R146 GUI and renderer COFF proof to a sealed linked PE."""

import argparse
import json
import re
import runpy
import struct
import subprocess
from pathlib import Path

import pefile


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
TREE = "57da5aa21cbb1aa7959477b5ddd41cec69288541"
BASE = "43044d5e77737e18572b7848676c6691720d5b95"
PRIVATE = "199c405159ddc6dc5637111c1365b8e8b605c477"
MANIFEST_SHA = "45ac5262e4e502cbef5b1a810398924ca84d6aac925c4e55e6ff5d6d54a2269a"
COMPOSED_SHA = "24642060a7512d9ed7424387f843554d042f8d41db829913471c2168168a8820"
PREFLIGHT = WAVE / "r146-composed-preflight"
R145_READER = WAVE / "r145-linked-proof/verify_linked.py"
helper = runpy.run_path(str(R145_READER))
sha, load, coff, sealed_link = (helper[name] for name in ("sha", "load", "coff", "sealed_link"))
assert sha(R145_READER.read_bytes()) == "3f64e5a08255f1a266f74810f1ddf8331f1f0bcdfd724cf82699e355ab377e09"
prior = helper["old"]
word, expect, signed = (prior[name] for name in ("word", "expect_words", "signed"))

FUNCTIONS = {
    "BrnGuiCache": ("Construct@GuiCache", "RecEvent@GuiCache"),
    "BrnInGame": ("Update@InGame",),
    "BrnCrashNavEnterOnline_wI_05": ("HandleEnteringJunkyard@CrashNavEnterOnlineBase",),
    "BrnCarSelectUnlock": ("OnEnter@CarSelectUnlock", "Update@CarSelectUnlock"),
    "GameBridgeGameStateToX_EventFlowGuiEvents": ("TranslateEventFlowGameActionToGuiEvent@BrnGame",),
    "BrnFBurnMainHudState": ("Update@FBurnMainHudState",),
    "BrnLionBlendRenderer": ("EndRendering@LionBlendRenderer",),
    "LionParticleRender": ("EndRendering@LionParticleRender",),
    "CgsImRenderer": ("ConstructOnceOnly@ImRendererBase",),
}
sealed_link.__globals__["FUNCTIONS"] = FUNCTIONS
for methods in FUNCTIONS.values():
    for method in methods:
        coff.__globals__["PREFIX"][method] = f"?{method}@"

PRIVATE_REFS = {
    "RecEvent@GuiCache": ("KAU_DRIVE_THRU_ICON_POSITION_IDS", "?sbLogged@", "?sbGateLogged@",
                          "?siLastNumPlayers@", "?sbLastIsHost@"),
    "Update@InGame": ("?PostOverlayRequest@?A0x", "??$PostCommand16@"),
    "TranslateEventFlowGameActionToGuiEvent@BrnGame": ("??$PushGuiEvent@USetEasyDriveNotAllowedWire96@",
                                                      "??$PushGuiEvent@UNetworkShowFreeBurnIntroWire279@",
                                                      "??$PushGuiEvent@UNetworkSplashWire269@"),
    "Update@FBurnMainHudState": ("??_C@_0EB@MMGHGGJB@",),
    "EndRendering@LionParticleRender": ("?spDefaultDepthStencil@?A0x",),
}


def local_definitions(obj):
    table, count = struct.unpack_from("<II", obj, 8)
    strings = table + count * 18
    index = 0
    definitions = {}
    while index < count:
        position = table + index * 18
        name, value, section, _, _, aux = struct.unpack_from("<8sIhHBB", obj, position)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = obj[start:obj.index(b"\0", start)]
        if section > 0:
            header = 20 + 40 * (section - 1)
            kind = obj[header:header + 8].rstrip(b"\0")
            size, offset = struct.unpack_from("<II", obj, header + 16)
            raw = obj[offset + value:offset + size] if offset else b""
            definitions[name.rstrip(b"\0").decode("ascii", errors="replace")] = kind, raw
        index += 1 + aux
    return definitions


def image_sections(link):
    image = pefile.PE(str(link / "native-game.pe"), fast_load=True)
    base = image.OPTIONAL_HEADER.ImageBase

    def lookup(address):
        for section in image.sections:
            start = base + section.VirtualAddress
            if start <= address < start + max(section.Misc_VirtualSize, section.SizeOfRawData):
                return section.Name.rstrip(b"\0")
        raise AssertionError(f"private target {address:#x} is outside the image")

    return lookup


def infer_private(method, entries, read, section, address, code, refs, definitions):
    """Account for only known local operands omitted by LINK's public map."""
    body = read(address, len(code))
    missing = {symbol for kind, symbol, local, alias in refs.values()
               if local is None and (alias or symbol) not in entries}
    inferred, details = {}, {}
    for symbol in missing:
        assert symbol.startswith(PRIVATE_REFS.get(method, ())), (method, symbol)
        assert symbol in definitions, (method, symbol, "not defined by the selected object")
        kind, payload = definitions[symbol]
        uses = {off: binding[0] for off, binding in refs.items() if binding[1] == symbol}
        targets = set()
        if kind == b".text":
            for off, reloc in uses.items():
                op = word(body, off)
                assert reloc == 6 and op & 0xFC000002 == 0x48000000
                targets.add(address + off + signed(op & 0x03FFFFFC, 26))
            [target] = targets
            assert section(target) == b".text"
            status = "private definition in selected COFF; linked .text destination observed"
        else:
            for off, reloc in uses.items():
                if reloc != 0x11:
                    assert reloc == 0x10
                    continue
                lo = word(body, off)
                prior_hi = [hi for hi, k in uses.items() if k == 0x10 and hi < off
                            and (word(body, hi) >> 21 & 31) == (lo >> 16 & 31)]
                assert prior_hi, (method, symbol, hex(off))
                hi = word(body, max(prior_hi))
                assert hi >> 26 == 15 and hi >> 16 & 31 == 0
                targets.add((((hi & 0xFFFF) << 16) + signed(lo & 0xFFFF, 16)) & 0xFFFFFFFF)
            [target] = targets
            if kind == b".rdata":
                if symbol == "KAU_DRIVE_THRU_ICON_POSITION_IDS":
                    payload = payload[:45 * 8]  # 45 CgsIDs, excluding section padding
                elif symbol.startswith("??_C@"):
                    payload = payload[:payload.index(b"\0") + 1]
                assert payload and section(target) == b".rdata" and read(target, len(payload)) == payload
                status = "map-local readonly payload matched selected COFF bytes"
            else:
                assert kind in (b".data", b".bss") and section(target) in (b".data", b".bss")
                status = "private definition in selected COFF; linked writable address observed"
        inferred[symbol] = {(target, "map-local")}
        details[symbol] = {"target": hex(target), "relocations": sorted(hex(off) for off in uses), "status": status}
    return inferred, details


def bind(objects, frozen, entries, read, section):
    linked, proofs = {}, {}
    for stem, methods in FUNCTIONS.items():
        path, obj, _ = objects[stem]
        definitions = local_definitions(obj)
        baseline = coff(frozen[stem], methods)
        for method, (name, code, refs) in coff(obj, methods).items():
            old_name, old_code, old_refs = baseline[method]
            assert prior["stable_name"](name) == prior["stable_name"](old_name) and code == old_code, method
            assert prior["canonical_bindings"](refs) == prior["canonical_bindings"](old_refs), method
            address = prior["map_address"](entries, name, path.name)
            owned = {}
            for _, symbol, local, alias in refs.values():
                target = alias or symbol
                locations = entries.get(target, set())
                if local is None and len({loc for loc, _ in locations}) > 1 and target in definitions:
                    same_owner = {row for row in locations if row[1].endswith(path.name)}
                    assert len({loc for loc, _ in same_owner}) == 1, (method, target, locations)
                    owned[target] = same_owner
            scoped = entries | owned
            inferred, details = infer_private(method, scoped, read, section, address, code, refs, definitions)
            linked[method], proofs[method] = prior["verify_function"](scoped | inferred, read, address, code, refs)
            proofs[method]["symbol"] = name
            if details:
                proofs[method]["map_local_operands"] = details
    return linked, proofs


def branch_target(code, offset, prefix):
    op = word(code, offset)
    assert op >> 16 == prefix
    return offset + signed(op & 0xFFFC, 16)


def linked_witnesses(linked, proofs, entries, read, composed):
    def address(method):
        return int(proofs[method]["address"], 16)

    def binding(method, off, prefix):
        row = proofs[method]["bindings"][hex(off)]
        assert row["symbol"].startswith(prefix), (method, hex(off), row)
        assert row["symbol"] not in proofs[method].get("map_local_operands", {})
        return row

    ctor_name, event_name = "Construct@GuiCache", "RecEvent@GuiCache"
    ctor, events = linked[ctor_name], linked[event_name]
    expect(ctor, 0x11C, 0x9BDF4B74, 0x616B3B9C, 0xFBDF4B68)
    expect(ctor, 0x148, 0x93BF4B70)
    expect(events, 0x88, 0x3965FFF2)
    expect(events, 0x1F40, 0x3965FE8B)
    gui_events = {}
    for event, target in composed["gui"]["dispatch"].items():
        event = int(event)
        base, anchor = (14, 0x94) if event < 245 else (373, 0x1F4C)
        table = int(proofs[event_name]["bindings"][hex(anchor)]["target"], 16)
        assert int(proofs[event_name]["bindings"][hex(anchor + 4)]["target"], 16) == table
        slot = table + (event - base) * 4
        dest = word(read(slot, 4), 0)
        assert dest == address(event_name) + int(target["coff_target"], 16)
        gui_events[str(event)] = {"table_slot": hex(slot), "target": hex(dest)}
    producer = "TranslateEventFlowGameActionToGuiEvent@BrnGame"
    for offset, event, size in ((0x3E4, 73, 8), (0x414, 74, 1)):
        expect(linked[producer], offset, 0x38A00000 | event, 0x38C00000 | size)
        gui_events[str(event)]["producer_call"] = binding(producer, offset + 8, "?AddEvent@?$VariableEventQueue@")
    crash_name, driving_name = "HandleEnteringJunkyard@CrashNavEnterOnlineBase", "Update@InGame"
    expect(linked[crash_name], 0xA8, 0x896B4B74, 0x2B0B0000, 0x419A0010)
    expect(linked[driving_name], 0x708, 0x7F24CB78, 0x896B4B74, 0x2B0B0000, 0x409A0008, 0x7F44D378)
    assert word(linked[driving_name], 0x1E4) >> 16 == 0x3B2B  # r25 = TO_CUNLOCK
    assert word(linked[driving_name], 0x1DC) >> 16 == 0x3B4B  # r26 = TO_CSELECT
    junkyard = {}
    for method, call, unlock, select in ((crash_name, 0xCC, 0xB8, 0xC4), (driving_name, 0x720, 0x1E4, 0x1DC)):
        routes = {}
        for off, value in ((unlock, b"TO_CUNLOCK\0"), (select, b"TO_CSELECT\0")):
            target = int(binding(method, off, "??_C@")["target"], 16)
            assert read(target, len(value)) == value
            routes[value[:-1].decode()] = hex(target)
        junkyard[method] = {"routes": routes, "call": binding(method, call, "?SendStateEvent@State@CgsGui@@")}

    hud_name = "Update@FBurnMainHudState"
    hud = linked[hud_name]
    hud_calls = {off: binding(hud_name, int(off, 16), f"?{callee}@")
                 for off, callee in composed["freeburn_hud"]["call_order_offsets"].items()}
    hud_table = int(proofs[hud_name]["bindings"]["0x28"]["target"], 16)
    for phase, entry in enumerate((0x54, 0x70, 0x8C, 0xA8, 0xBC)):
        assert word(read(hud_table + phase * 4, 4), 0) == address(hud_name) + entry
        expect(hud, entry, 0x39600000 | phase)
        expect(hud, entry + (4 if phase == 4 else 8), 0x917E0038)
    for off in (0x6C, 0x88, 0xA4):
        assert branch_target(hud, off, 0x419A) == 0x124
    expect(hud, 0x104, 0x38A00502)
    expect(hud, 0x124, 0x817E0038, 0x2F0B0004, 0x419A000C)
    expect(hud, 0x138, 0x807E0018)
    assertion = int(proofs[hud_name]["bindings"]["0x100"]["target"], 16)
    expected_file = b"..\\..\\..\\GameSource\\Gui/Flow/HUD/States/BrnFBurnMainHudState.cpp\0"
    assert read(assertion, len(expected_file)) == expected_file
    message = int(proofs[hud_name]["bindings"]["0xe4"]["target"], 16)
    expected_prefix = b"Should never call update in the following state"
    assert read(message, len(expected_prefix)) == expected_prefix

    lion_name, provider_name = "EndRendering@LionBlendRenderer", "ConstructOnceOnly@ImRendererBase"
    library = composed["lion_blend"]["provider_symbol"]
    library_address = prior["map_address"](entries, library, "CgsImRenderer-1742b410.obj")
    for method, offsets in ((lion_name, (0xC, 0x1C)), (provider_name, (0x28, 0x30))):
        for off in offsets:
            assert int(binding(method, off, library)["target"], 16) == library_address
    assert "dword_83010F20" not in entries
    expect(linked[lion_name], 0x60, 0x7F63DB78)
    assert word(linked[lion_name], 0x1C) >> 16 == 0x836B
    binding(lion_name, 0x64, "?SetState@Device@shadow@@")
    expect(linked[provider_name], 0x14, 0x38C00000, 0x38A00007, 0x38800006)
    binding(provider_name, 0x24, "?ConstructBlendState@ImRendererBase@")
    expect(linked[provider_name], 0x3C, 0x907E0000)
    return {"gui_cache": {"initializes": ["0x4b74", "0x4b68"], "events": gui_events,
                          "junkyard_consumer_offset": "0x4b74", "junkyard_routes": junkyard},
            "freeburn_hud": {"jump_table": hex(hud_table), "calls": hud_calls, "assert_line": 1282},
            "lion_blend": {"symbol": library, "address": hex(library_address), "slot": 0,
                           "initializer": hex(address(provider_name)), "shadow_symbol_absent": True}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path, nargs="?")
    parser.add_argument("--expected-head")
    parser.add_argument("--self-test", action="store_true", help="bind the R145 baseline and inspect R146 COFF")
    args = parser.parse_args()
    manifest = load(WAVE / "r146-batch/MANIFEST.json", MANIFEST_SHA)
    assert (manifest["base"], manifest["candidate"], manifest["projected_tree"]) == (BASE, PRIVATE, TREE)
    composed = load(WAVE / "r146-composed-proof/R146_COMPOSED_COFF.json", COMPOSED_SHA)
    assert (composed["head"], composed["tree"], composed["parent"]) == (PRIVATE, TREE, BASE)
    report = load(PREFLIGHT / "compile-results.json", composed["compile_report_sha256"])
    assert report["passed"] == 8 and report["failed"] == 0
    base_link = WAVE / "r145-link"
    _, base_objects, base_entries, base_read, _ = sealed_link(base_link, BASE)
    _, provider, provider_row = base_objects["CgsImRenderer"]
    assert provider_row["sha256"] == composed["lion_blend"]["provider_r145_selected_sha256"]
    frozen = {"CgsImRenderer": provider}
    for row in report["results"]:
        stem = Path(row["source"]).stem
        frozen[stem] = Path(row["object"]).read_bytes()
        assert sha(frozen[stem]) == composed["composed_objects_sha256"][stem]
    if args.self_test:
        linked, proofs = bind(base_objects, {stem: item[1] for stem, item in base_objects.items()},
                              base_entries, base_read, image_sections(base_link))
        count = sum(len(coff(frozen[stem], methods)) for stem, methods in FUNCTIONS.items())
        private = sum(len(row.get("map_local_operands", {})) for row in proofs.values())
        print(f"R145 actual linked fixture: {len(linked)} bodies, {private} private operands; R146 COFF: {count} bodies parsed")
        return

    if not args.link_dir or not re.fullmatch(r"[0-9a-f]{40}", args.expected_head or ""):
        parser.error("provide link_dir and --expected-head, or use --self-test")
    link = args.link_dir.resolve()
    seal, objects, entries, read, hashes = sealed_link(link, args.expected_head)
    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]
    revision = subprocess.check_output([*git, "rev-parse", f"{args.expected_head}^{{tree}}", f"{args.expected_head}^"],
                                       text=True).splitlines()
    assert revision == [TREE, BASE], revision
    diff = subprocess.check_output([*git, "diff", "--name-only", BASE, args.expected_head], text=True).splitlines()
    assert set(diff) == {row["path"] for row in manifest["final_paths"]}
    for stem in ("LionParticleRender", "BrnLionBlendRenderer"):
        assert not any("dword_83010F20" in name for name in local_definitions(objects[stem][1]))
    linked, proofs = bind(objects, frozen, entries, read, image_sections(link))
    witnesses = linked_witnesses(linked, proofs, entries, read, composed)
    receipt = {"head": args.expected_head, "tree": TREE, "base": BASE, "manifest_sha256": MANIFEST_SHA,
               "composed_coff_sha256": COMPOSED_SHA, "link_sha256": hashes,
               "providers": {stem: row for stem, (_, _, row) in objects.items()}, "witnesses": witnesses,
               "functions": proofs, "runtime": "not run"}
    output = ROOT / "R146_GUI_RENDER_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
