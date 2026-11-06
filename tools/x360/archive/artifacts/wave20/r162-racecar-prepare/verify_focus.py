
"""Verify original PPC operands and the two clean R162 RaceCar donor objects."""
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "r161-native-inputpads"))
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import verify_focus as v
from x360rd import rd, u32

C = "BrnRaceCarEntityModule"
R = C + "_Range"
S = "BrnRaceCarComponentStreamers"
P = "?Prepare@RaceCarEntityModule@BrnWorld@@"
G = "?mRandom@BrnWorld@@3VRandom@CgsNumeric@@A"
LIFECYCLE = "de860910247c327292982d4362a7a04d5e0a09e3"

def compiled(scope):
    v.OUT = ROOT / scope
    path = v.OUT / "compile-receipt.json"
    record = json.loads(path.read_bytes())
    assert record["tree"] == v.git("rev-parse", record["commit"] + "^{tree}")
    assert record["parent"] == v.git("rev-parse", record["commit"] + "^")
    for row in record["results"]:
        name = Path(row["source"]).stem
        assert row["exit_code"] == 0 and not row["warnings"] and row["machine"] == "0x01F2"
        assert v.sha(v.OUT / f"{name}.obj") == row["ppc_coff_sha256"]
        assert v.sha(v.OUT / f"{name}.asm") == row["listing_sha256"]
    return record, v.sha(path)

def word(code, offset):
    return int.from_bytes(code[offset:offset+4], "big")

def expect(code, expected):
    assert all(word(code, offset) == value for offset, value in expected.items()), expected

def direct(stem, signature, edges):
    return v.verify_coff_calls(stem, signature, edges, edges.values(), v.body(stem, signature))

def signed_branch(address, instruction):
    width = 26 if instruction >> 26 == 18 else 16
    delta = instruction & ((1 << width) - 4)
    return address + delta - ((1 << width) if delta >> (width - 1) else 0)

def refs(stem, method, target):
    data = (v.OUT / f"{stem}.obj").read_bytes()
    machine, _, _, symtab, count, opt, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x01F2
    strings = symtab + 18 * count
    section = None
    target_index = None
    index = 0
    while index < count:
        entry = symtab + 18 * index
        name = data[entry:entry+8]
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(bytes(1), start)]
        name = name.rstrip(bytes(1)).decode("ascii")
        value, number, kind, _, aux = struct.unpack_from("<IhHBB", data, entry+8)
        if number > 0 and kind == 0x20 and name.startswith(method):
            assert section is None and value == 0
            section = number
        if name == target:
            assert target_index is None
            target_index = index
        index += 1 + aux
    assert section is not None and target_index is not None
    header = 20 + opt + 40 * (section - 1)
    rel_ptr = struct.unpack_from("<I", data, header + 24)[0]
    rel_count = struct.unpack_from("<H", data, header + 32)[0]
    result = {}
    for offset in range(rel_ptr, rel_ptr + 10 * rel_count, 10):
        at, index, typ = struct.unpack_from("<IIH", data, offset)
        if index == target_index:
            result[f"0x{at:X}"] = typ
    return result

assert not v.git("status", "--porcelain=v1")
prep, prep_hash = compiled("prepare")
assert prep["parent"] == LIFECYCLE
code, prep_edges = direct(C, P, {
    0x244: "?Prepare@PowerParkingManager@", 0x250: "?Register@DebugComponent@",
    0x260: "?SetSeed@Random@", 0x26C: "?Register@DebugComponent@", 0x278: "?Register@DebugComponent@",
    0x284: "?Prepare@ModuleSingleBuffered@", 0x2AC: "?LoadGlobalResources@",
    0x2EC: "?Prepare@RaceCar@", 0x30C: "?Prepare@ActiveRaceCar@",
    0x32C: "?SetSeed@Random@", 0x33C: "?SetSeed@Random@",
    0x464: "?ClearAllActiveRaceCarToPlayerScoringMappings@", 0x488: "?Prepare@RaceCarStreamer@",
})
expect(code, {
    0xD4: 0x39206DC2, 0xD8: 0x3AA00001, 0xE8: 0x65378FE0,
    0x214: 0x396B80D8, 0x21C: 0x394A80E8, 0x24C: 0x386380F0, 0x258: 0x7EE4BB78,
    0x268: 0x38637E50, 0x274: 0x38637CF0,
    0x2CC: 0x92BF022C, 0x2D0: 0x3B60FFFF, 0x2D4: 0x93DF0230,
    0x2D8: 0x616B82F8, 0x2E0: 0x3BA00023, 0x2E4: 0x7F7F592E, 0x304: 0x3BA00008,
    0x324: 0x7EE4BB78, 0x328: 0x38638460, 0x334: 0x7EE4BB78, 0x338: 0x38638490,
    0x34C: 0x614884C4, 0x364: 0x614784C8, 0x37C: 0x61468371, 0x38C: 0x616982F0,
    0x394: 0x61458394, 0x3BC: 0x396B83F0, 0x3D4: 0x614A8348, 0x3DC: 0x61298346,
    0x40C: 0x614583E4, 0x424: 0x614483E8, 0x434: 0x39200007, 0x43C: 0x394A83A8,
    0x450: 0xFBCA0000, 0x454: 0x394A0008, 0x458: 0x4200FFF8, 0x460: 0x93CA0000,
    0x470: 0x616B8438, 0x474: 0x614A8434, 0x47C: 0x38631100,
    0x480: 0x7CBF582E, 0x484: 0x7C9F502E, 0x48C: 0x38600001,
})
load, _ = v.coff_body(C, "?LoadGlobalResources@RaceCarEntityModule@BrnWorld@@")
expect(load, {0x37C: 0x614A8438, 0x380: 0x7D7F512E, 0x384: 0x38600001})
stream, streamer_edges = direct(S, "?Prepare@RaceCarStreamer@BrnWorld@@",
    {0x24: "?BeginAssert@", 0x3C: "?FireAssert@", 0x40: "?EndAssert@"})
expect(stream, {0x14: 0x7C9F2378, 0x20: 0x409A0024, 0x2C: 0x38A00057, 0x44: 0x93FE0000})
assert signed_branch(0x20, word(stream, 0x20)) == 0x44
obj = (v.OUT / f"{S}.obj").read_bytes()
assert rd(0x820167A0, 256).split(bytes(1))[0] + bytes(1) in obj
assert rd(0x82016784, 64).split(bytes(1))[0] + bytes(1) in obj

colour = "?GetRandomCarColour@RaceCarEntityModule@BrnWorld@@"
range_update = "?UpdateInAndOutOfRangeCars@RaceCarEntityModule@BrnWorld@@"
_, colour_edges = direct(R, colour, {0x3C: "?RandomUInt@", 0x5C: "?RandomUInt@"})
_, range_edges = direct(R, range_update, {0x220: "?RandomUInt@"})
global_refs = {"prepare": refs(C, P, G), "colour": refs(R, colour, G), "range": refs(R, range_update, G)}
assert all(len(entries) == 2 for entries in global_refs.values()), global_refs

abi, abi_hash = compiled("handle-abi")
assert abi["parent"] == prep["commit"] and abi["commit"] == v.git("rev-parse", "HEAD")
for source, digest in abi["source_sha256"].items():
    assert v.sha(v.REPO / source) == digest
caller, abi_edges = direct("BrnWorldModule", "?Prepare@WorldModule@BrnWorld@@",
                          {0x4B4: "UResourceHandle@CgsResource@@"})
expect(caller, {0x4A8: 0x616B1B88, 0x4AC: 0x7FA4EB78, 0x4B0: 0x7CBF582A})
callee, _ = v.coff_body(C, P)
expect(callee, {0x10: 0x7CB82B78, 0x24: 0xFB010120, 0x154: 0xFB1F023C})
assert u32(0x827D58C0) == word(caller, 0x4B0)
assert signed_branch(0x827D58C4, u32(0x827D58C4)) == 0x82303E78
assert u32(0x82303E88) == 0x7CBC2B78 and u32(0x82303E9C) == 0xFB810110
assert u32(0x82303F7C) == 0xFB9E023C

assert u32(0x82303ED0) == 0x39206DC2 and u32(0x82303ED8) == 0x65388FE0
assert u32(0x82304120) == 0x933E022C and u32(0x82304128) == 0x93FE0230
assert u32(0x823041D4) == 0x396B8460 and u32(0x82304230) == 0x396B8490
assert u32(0x82304398) == 0x38A00007 and u32(0x823043A8) == 0xF88A0000
assert u32(0x823043B8) == 0x908A0000 and u32(0x82304400) == 0x7FFE592E

seed = 0x8FE06DC2
ring = []
for _ in range(8):
    ring.append(0x3F800000 | (seed >> 41))
    seed = (seed * 0x5851F42D4C957F2D + 1) & 0xFFFFFFFFFFFFFFFF
record = {
    "prepare": {"commit": prep["commit"], "parent": prep["parent"], "tree": prep["tree"],
                "compile_receipt_sha256": prep_hash, "coff_edges": prep_edges,
                "streamer_assert_edges": streamer_edges, "range_edges": range_edges,
                "colour_edges": colour_edges, "shared_random_relocations": global_refs,
                "random_seed": "0x8FE06DC2", "primed_ring": [f"0x{x:08X}" for x in ring],
                "primed_seed": f"0x{seed:016X}", "primed_cursor": 0},
    "handle_abi": {"commit": abi["commit"], "parent": abi["parent"], "tree": abi["tree"],
                   "compile_receipt_sha256": abi_hash, "coff_call": abi_edges,
                   "caller_r5_load": "0x4B0", "callee_r5_home": "0x24", "callee_handle_store": "0x154"},
    "scope": "Selected XDK PPC COFF and ARTIST operand proof. Strict link and runtime pending.",
}
(ROOT / "RECEIPT.json").write_text(json.dumps(record, indent=2) + "\n")
print("Verified RaceCar RNG ownership and reset operands, streamer assertion, and by-value handle ABI")

