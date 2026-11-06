"""Verify R158 GameData startup stages in the composed COFF and the sealed PPC PE."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
GIT = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
HEAD = "b492f1163ba4988f7244cd7dd5f63824d4d7c964"
TREE = "7794ea26f68bd5170fcf9a64f06cb9f82304271a"
COMPILE = WAVE / "r158-composed-preflight/compile-results.json"
COMPILE_SHA = "8df69d0d81c935eab452d83b233a209c5007ca96cdea3384e65b87e8091601b7"
HELPER = runpy.run_path(str(WAVE / "r148-boot-presentation-linked/verify_linked.py"))
IMAGE = runpy.run_path(str(WAVE / "r154-racecar-prescene-linked/verify_linked.py"))["Image"]
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd, u32

RESOURCE = "src/GameSource/Resource/"
SDK = "src/GameShared/GameClasses/System/Resource/"
DATA = RESOURCE + "BrnGameDataModule.cpp"
GAME = "src/GameSource/Game/BrnGameModule.cpp"
DLC = RESOURCE + "BrnDLCManager.cpp"
QUEUE = SDK + "CgsResourceModuleIO_InputBuffer_GetResourceQueue.cpp"
EVENTS = SDK + "CgsResourceIOEvents.cpp"
PREPARE = "?Prepare@GameDataModule@BrnResource@@"
DLC_PREPARE = "?Prepare@DLCManager@BrnResource@@"
GET_PACKAGE = "?GetPackage@DLCManager@BrnResource@@"
GET_QUEUE = "?GetResourceQueue@InputBuffer@ResourceIO@CgsResource@@QAAPAU"
CONSTRUCT = "?Construct@AddPatchRequest@Events@CgsResource@@"
LOCK = "?LockForWrite@IOBuffer@"
UNLOCK = "?UnlockForWrite@IOBuffer@"
PUBLISH = "?SetFileSystemStatusInterface@OutputBuffer@GameDataIO@BrnResource@@"
IO_CONSTRUCT = "??0?$IOHelper@UInputBuffer@ResourceIO@CgsResource@@"
IO_DESTRUCT = "??1?$IOHelper@UInputBuffer@ResourceIO@CgsResource@@"
PUMP = "?UpdateResourceModule@GameDataModule@BrnResource@@"
CLEAR = "?Clear@BaseEventReceiverQueue@CgsModule@@"
ADD = "?AddEvent@?$VariableEventQueue@$0EAAA@$0BA@@CgsModule@@"
VTABLE = "??_7GameDataModule@BrnResource@@6B@"
MASKS = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 16: 0xFFFF, 17: 0xFFFF}

COFF_CHECKS = {
    "game_data_prepare": (DATA, PREPARE, {
        0x1BC: DLC_PREPARE, 0x1F4: IO_CONSTRUCT, 0x200: LOCK,
        0x210: GET_PACKAGE, 0x224: GET_PACKAGE, 0x240: CONSTRUCT,
        0x24C: GET_QUEUE, 0x25C: ADD, 0x264: CLEAR, 0x26C: UNLOCK,
        0x280: PUMP, 0x288: IO_DESTRUCT, 0x2C8: IO_CONSTRUCT, 0x2DC: PUMP,
        0x2E4: IO_DESTRUCT, 0x594: LOCK, 0x5A0: PUBLISH, 0x5A8: UNLOCK,
    }, {
        0x30: 0x817F0228, 0x34: 0x2B0B0013, 0x4C: 0x3D400006, 0x50: 0x3AA00011,
        0x54: 0x615481A0, 0x58: 0x3D400007, 0x5C: 0x61584990, 0x60: 0x3D400007,
        0x64: 0x61564958, 0x68: 0x3D400005, 0x6C: 0x615A8A58, 0x70: 0x3D400007,
        0x74: 0x615C4338, 0x80: 0x5560103A, 0x84: 0x7C0C002E, 0x8C: 0x4E800420,
        0x1AC: 0x39600010, 0x1B0: 0x7C9FA214, 0x1B4: 0x7C7FE214, 0x1B8: 0x917F0228,
        0x1C4: 0x2B0B0000, 0x1C8: 0x419A03C8, 0x1CC: 0x7E7FC12E, 0x1D4: 0x7D7FB02E,
        0x1D8: 0x92BF0228, 0x1E0: 0x7F0A5800, 0x1E4: 0x409800A8, 0x1EC: 0x7EE5BB78,
        0x220: 0x838B0044, 0x228: 0x80BD0000, 0x22C: 0x7FBFD214, 0x230: 0x7C661B78,
        0x234: 0x7F87E378, 0x238: 0x38610060, 0x23C: 0x7FA4EB78, 0x254: 0x38A00019,
        0x258: 0x38C0010C, 0x260: 0x7FA3EB78, 0x28C: 0x39200012, 0x290: 0x7D5FB02E,
        0x298: 0x913F0228, 0x2A0: 0x7F095000, 0x2A4: 0x40980060, 0x2A8: 0x3D000005,
        0x2AC: 0x61088A60, 0x2B0: 0x7D1F402E, 0x2B4: 0x2F080000, 0x2B8: 0x409A0034,
        0x2C0: 0x7EE5BB78, 0x2E8: 0x480002A8, 0x2EC: 0x39290001, 0x2F4: 0x912B0000,
        0x2F8: 0x4098000C, 0x2FC: 0x92BF0228, 0x300: 0x48000290, 0x304: 0x39600006,
        0x314: 0x917F0228, 0x5AC: 0x38600000,
    }),
    "game_startup_dispatch": (GAME, "?Prepare@BrnGameModule@BrnGame@@", {}, {
        0x148: 0x3C7F005F, 0x158: 0x38634B00, 0x168: 0x81630000,
        0x16C: 0x816B0040, 0x170: 0x7D6903A6, 0x174: 0x4E800421,
        0x178: 0x546B063E, 0x17C: 0x2B0B0000, 0x180: 0x419A01B0,
    }),
    "package_provider": (DLC, GET_PACKAGE, {}, {
        0x14: 0x2F1F0000, 0x1C: 0x817D0620, 0x7C: 0x1D7F004C, 0x80: 0x7C6BEA14,
    }),
    "writable_queue_provider": (QUEUE, GET_QUEUE, {}, {
        0x14: 0x897F0000, 0x18: 0x556BEFFE, 0x1C: 0x2B0B0000, 0x20: 0x409A0024,
        0x44: 0x387F0004,
    }),
}
LINK_CHECKS = {
    "patch_request_provider": (EVENTS, CONSTRUCT, {}, {
        0x1C: 0x909E0000, 0x20: 0x90BE0004, 0x44: 0x2F0B0100,
        0x6C: 0x397E0008, 0x8C: 0x93BE0108,
    }),
    "dlc_prepare_provider": (DLC, DLC_PREPARE, {}, {}),
}

ARTIST_BRANCHES = {
    0x826740F0: 0x8266AF50, 0x826740FC: 0x826744C4, 0x82674118: 0x826741BC,
    0x82674128: 0x82665DC8, 0x82674134: 0x82204A18, 0x82674148: 0x82661550,
    0x8267415C: 0x82661550, 0x82674174: 0x82661200, 0x8267417C: 0x82663B38,
    0x8267418C: 0x822F2290, 0x82674194: 0x821F1D50, 0x8267419C: 0x82204BB8,
    0x826741B0: 0x826663B0, 0x826741B8: 0x82665E30, 0x826741D4: 0x82674234,
    0x826741E8: 0x8267421C, 0x826741F8: 0x82665DC8, 0x8267420C: 0x826663B0,
    0x82674214: 0x82665E30, 0x82674218: 0x826744C4, 0x82674228: 0x82674234,
    0x82674230: 0x826744C4, 0x826744C8: 0x82204A18, 0x826744D4: 0x82663EE0,
    0x826744DC: 0x82204BB8,
}
ARTIST_WORDS = {
    0x82673F84: 0x615481A0, 0x82673F90: 0x61594990, 0x82673F98: 0x61564958,
    0x82673FA0: 0x615B8A58, 0x82673FA8: 0x615C4338, 0x82673FDC: 0x82674234,
    0x82674004: 0x826740E0, 0x82674008: 0x82674104, 0x8267400C: 0x826741BC,
    0x826740E0: 0x39600010, 0x826740EC: 0x917F0228, 0x82674100: 0x7E7FC92E,
    0x8267410C: 0x92BF0228, 0x82674158: 0x838B0044, 0x82674160: 0x7C661B78,
    0x82674168: 0x7F64DB78, 0x82674170: 0x7F87E378, 0x82674180: 0x38C0010C,
    0x82674184: 0x38A00019, 0x826741BC: 0x39200012, 0x826741C8: 0x913F0228,
    0x826741DC: 0x61088A60, 0x8267421C: 0x39290001, 0x82674224: 0x912B0000,
    0x8267422C: 0x92BF0228, 0x8267423C: 0x39600006, 0x82674250: 0x917F0228,
    0x826744E0: 0x38600000, 0x82661614: 0x1D7D004C, 0x82661618: 0x7C6BDA14,
    0x82663B4C: 0x556BEFFE, 0x82663BD0: 0x387C0004, 0x8266121C: 0x909B0000,
    0x82661224: 0x90BB0004, 0x8266137C: 0x395B0008, 0x8266139C: 0x935B0108,
}
STAGE_TABLE = {6: 0x304, 16: 0x1AC, 17: 0x1D0, 18: 0x28C}
NATIVE_BRANCHES = {
    0x1C8: 0x590, 0x1E4: 0x28C, 0x2A4: 0x304, 0x2B8: 0x2EC,
    0x2E8: 0x590, 0x2F8: 0x304, 0x300: 0x590,
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(path.read_bytes())


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(GIT), *args])


def word(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def check_listing(path, symbol, code, refs):
    inside = False
    seen = set()
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line == "; Begin code for function: " + symbol:
            inside = True
            continue
        if inside and line.startswith("; End code for function: "):
            break
        if inside and (row := re.match(r"^\s+([\da-f]{5})\s+([\da-f]{8})\b", line)):
            offset, raw = (int(value, 16) for value in row.groups())
            assert offset not in seen
            seen.add(offset)
            mask = MASKS[refs[offset][0]] if offset in refs else 0
            assert not ((raw ^ word(code, offset)) & ~mask), (path, hex(offset))
    assert seen == set(range(0, len(code), 4)), (path, symbol, len(seen))


def check_source(head, path, source, expected=None):
    actual = sha(path)
    if expected:
        assert actual == expected
    assert path.read_bytes().replace(b"\r\n", b"\n") == git("show", f"{head}:{source}").replace(b"\r\n", b"\n")
    return {"blob": git("rev-parse", f"{head}:{source}").decode().strip(), "sha256": actual}


def pe_bytes(image, address, count):
    rva = address - image.base
    _, _, start, _, file_at = next(row for row in image.sections if row[2] <= rva < row[2] + row[3])
    return image.data[file_at + rva - start:file_at + rva - start + count]


def inspect(label, check, rows, linked=None):
    source, prefix, edges, words = check
    row = rows[source]
    path = Path(row["object"])
    payload = path.read_bytes()
    obj = HELPER["Coff"](payload)
    assert digest(payload) == row.get("sha256", digest(payload))
    symbol, code, refs = obj.function(prefix)
    result = {"source": source, "symbol": symbol.name, "object": str(path), "object_sha256": digest(payload),
              "body_sha256": digest(code), "calls": {}, "words": {}}
    for offset, expected in words.items():
        assert offset not in refs and word(code, offset) == expected, (label, hex(offset), hex(word(code, offset)))
        result["words"][hex(offset)] = hex(expected)
    if linked:
        image, entries, sources, head = linked
        src = sources[source.lower()]
        result["source_identity"] = check_source(head, Path(src["path"]), source, src["sha256"])
        [va] = {va for va, owner in entries[symbol.name] if owner.lower() == path.name.lower()}
        linked_code = pe_bytes(image, va, len(code))
        for offset in range(0, len(code), 4):
            mask = MASKS[refs[offset][0]] if offset in refs else 0
            assert not ((word(code, offset) ^ word(linked_code, offset)) & ~mask), (label, hex(offset))
        result.update(linked_va=hex(va), linked_body_sha256=digest(linked_code))
    else:
        listing = Path(row["assembly"])
        check_listing(listing, symbol.name, code, refs)
        result.update(listing=str(listing), listing_sha256=sha(listing),
                      source_identity=check_source(HEAD, GIT / source, source))

    for offset, expected in edges.items():
        kind, target = refs[offset]
        assert kind == 6 and target.name.startswith(expected), (label, hex(offset), target.name, expected)
        call = {"target": target.name}
        if linked:
            site = va + offset
            raw = image.word(site)
            assert raw >> 26 == 18 and raw & 1
            destination = image.target(site, raw)
            owners = sorted({owner for address, owner in entries[target.name] if address == destination})
            assert owners, (label, hex(site), target.name, hex(destination))
            call.update(site_va=hex(site), target_va=hex(destination), owners=owners)
        result["calls"][hex(offset)] = call

    if label == "game_data_prepare":
        assert obj.vtable_slot(VTABLE, 0x40) == symbol.name
        assert refs[0x40][0] == 16 and refs[0x48][0] == 17 and refs[0x40][1] == refs[0x48][1]
        assert obj.literal(refs[0x40][1], 9) == b"Resource\0"
        result["stage_table"] = {}
        for stage, target_offset in STAGE_TABLE.items():
            entry = 0x90 + 4 * stage
            kind, target = refs[entry]
            assert kind == 2 and target.section == symbol.section and target.value == target_offset
            result["stage_table"][stage] = {"entry": hex(entry), "target_offset": hex(target_offset)}
            if linked:
                assert image.word(va + entry) == va + target_offset
                result["stage_table"][stage]["target_va"] = hex(va + target_offset)
        result["local_branches"] = {}
        for offset, destination in NATIVE_BRANCHES.items():
            assert IMAGE.target(None, offset, word(code, offset)) == destination
            if linked:
                assert image.target(va + offset, image.word(va + offset)) == va + destination
            result["local_branches"][hex(offset)] = hex(destination)
        if linked:
            [vtable] = {address for address, owner in entries[VTABLE] if owner.lower() == path.name.lower()}
            assert image.word(vtable + 0x40) == va
            high = (image.word(va + 0x40) & 0xFFFF) << 16
            low = image.word(va + 0x48) & 0xFFFF
            addr = high + (low - 0x10000 if low & 0x8000 else low)
            assert pe_bytes(image, addr, 9) == b"Resource\0"
            result.update(vtable_slot_va=hex(vtable + 0x40), resource_literal_va=hex(addr))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--link-head")
    for pin in ("seal", "invocation", "result", "pe", "map", "xex", "image-verification", "completion"):
        parser.add_argument(f"--{pin}-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assert git("rev-parse", f"{HEAD}^{{tree}}").decode().strip() == TREE
    assert sha(COMPILE) == COMPILE_SHA
    compile_report = load(COMPILE)
    assert compile_report["requested"] == compile_report["passed"] == 13 and compile_report["failed"] == 0
    rows = {row["source"]: row for row in compile_report["results"]}
    assert all(row["passed"] and row["returncode"] == 0 and row["machine"] == 0x1F2 for row in rows.values())
    for address, expected in ARTIST_WORDS.items():
        assert u32(address) == expected
    for address, target in ARTIST_BRANCHES.items():
        assert IMAGE.target(None, address, u32(address)) == target
    receipt = {
        "head": HEAD, "tree": TREE, "focused_report": {"path": str(COMPILE), "sha256": COMPILE_SHA},
        "artist_span_sha256": digest(rd(0x82673F38, 0x826744EC - 0x82673F38)),
        "artist_words": {hex(address): hex(word) for address, word in ARTIST_WORDS.items()},
        "artist_branches": {hex(address): hex(target) for address, target in ARTIST_BRANCHES.items()},
    }
    prior = {name: inspect(name, check, rows) for name, check in COFF_CHECKS.items()}
    receipt["functions"] = prior
    if args.link_head:
        link = WAVE / "r158-link"
        files = {
            "seal": link / "seal.json", "invocation": link / "invocation.json", "result": link / "result.json",
            "pe": link / "native-game.pe", "map": link / "native-game.map", "xex": link / "native-game.exe",
            "image_verification": link / "image-verification.json",
            "completion": WAVE / "r158-intake/completion.json",
        }
        pins = {key: getattr(args, key + "_sha256") for key in files}
        for key, path in files.items():
            assert pins[key] and sha(path) == pins[key], key
        assert git("rev-parse", f"{args.link_head}^{{tree}}").decode().strip() == TREE
        seal, invocation, result = (load(files[key]) for key in ("seal", "invocation", "result"))
        completion, image_verification = (load(files[key]) for key in ("completion", "image_verification"))
        assert seal["head"] == completion["head"] == image_verification["head"] == args.link_head
        assert invocation["sealed_sha256"] == pins["seal"] and invocation["objects"] == seal["selected"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        outputs = {Path(path).name: row for path, row in result["outputs"].items()}
        for key in ("pe", "map", "xex"):
            assert outputs[files[key].name]["sha256"] == pins[key]
        assert image_verification["magic"] == "XEX2" and image_verification["image_sha256"] == pins["xex"]
        assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
        objects = {row["source"]: row for row in invocation["objects"]}
        entries = HELPER["map_symbols"](files["map"].read_bytes())
        image = IMAGE(files["pe"], files["map"])
        root = Path(seal["source_root"])
        sources = {Path(row["path"]).relative_to(root).as_posix().lower(): row for row in seal["sources"]
                   if Path(row["path"]).is_relative_to(root)}
        receipt["linked"] = {"head": args.link_head, "tree": TREE, "pins": pins}
        receipt["functions"] = {name: inspect(name, check, objects, (image, entries, sources, args.link_head))
                                for name, check in (COFF_CHECKS | LINK_CHECKS).items()}
        for name, before in prior.items():
            now = receipt["functions"][name]
            assert now["body_sha256"] == before["body_sha256"], name
            assert now["source_identity"]["blob"] == before["source_identity"]["blob"], name
    receipt["scope"] = "Static GameData startup stages, selected callers and providers. Runtime startup and DLC behavior remain unverified."
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(receipt["functions"]), "startup functions", sha(args.output))


if __name__ == "__main__":
    main()
