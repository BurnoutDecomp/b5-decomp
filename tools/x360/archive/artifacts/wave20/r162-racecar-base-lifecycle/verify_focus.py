
"""Verify the R162 RaceCar lifecycle targets, state stores and retry edges in raw PPC COFF."""
import hashlib
import json
from pathlib import Path
import sys

OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT.parent / "r161-native-inputpads"))
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import verify_focus as v
from x360rd import u32

v.OUT = OUT
C = "BrnRaceCarEntityModule"
T = "BrnRaceCarEntityModule_wS34_00"
AUDIT = OUT.parent / "r162-post-title-audit"
compile_path = OUT / "compile-receipt.json"
compiled = json.loads(compile_path.read_bytes())
assert compiled["commit"] == v.git("rev-parse", "HEAD")
assert compiled["tree"] == v.git("rev-parse", "HEAD^{tree}")
assert compiled["base"] == v.git("rev-parse", "HEAD^")
assert v.git("rev-parse", "refs/heads/codex/private-x360-r161-native-inputpads") == (
    "5b53cad93bf4c94845c45a38bb37a1bf4709e792")
assert not v.git("status", "--porcelain=v1")
for source, digest in compiled["source_sha256"].items():
    assert v.sha(v.REPO / source) == digest
for row in compiled["results"]:
    stem = Path(row["source"]).stem
    assert row["exit_code"] == 0 and not row["warnings"] and row["machine"] == "0x01F2"
    assert v.sha(OUT / f"{stem}.obj") == row["ppc_coff_sha256"]
    assert v.sha(OUT / f"{stem}.asm") == row["listing_sha256"]


def target(pc, instruction):
    opcode = instruction >> 26
    assert opcode in (16, 18)
    mask, sign = (0xFFFC, 0x8000) if opcode == 16 else (0x03FFFFFC, 0x02000000)
    delta = instruction & mask
    return pc + delta - (2 * sign if delta & sign else 0)


def word(code, offset):
    return int.from_bytes(code[offset:offset + 4], "big")


def expect(code, expected):
    assert all(word(code, offset) == value for offset, value in expected.items()), expected


def race_body(stem, method, calls):
    signature = f"?{method}@RaceCarEntityModule@BrnWorld@@"
    rows = v.body(stem, signature)
    return v.verify_coff_calls(stem, signature, calls, calls.values(), rows)


c, c_calls = race_body(C, "Construct", {
    0x14: "?Construct@ModuleSingleBuffered@",
    0x7C: "?Construct@RaceCarStreamer@",
    0x8C: "?Construct@PlaceOnTrackManager@",
})
expect(c, {0x18: 0x39600002, 0x2C: 0x917F0230, 0x80: 0x3C7F0001, 0x84: 0x7FE4FB78,
           0x88: 0x38637850, 0x178: 0x3BA00001, 0x3D8: 0x9BBF0004})
p, p_calls = race_body(C, "Prepare", {
    0x224: "?Prepare@ModuleSingleBuffered@",
    0x240: "?LockForWrite@IOBuffer@",
    0x24C: "?LoadGlobalResources@RaceCarEntityModule@",
    0x258: "?UnlockForWrite@IOBuffer@",
})
expect(p, {0x218: 0x39600001, 0x220: 0x917F022C, 0x228: 0x546B063E, 0x22C: 0x2B0B0000,
           0x230: 0x419A00D8, 0x234: 0x39600002, 0x23C: 0x917F022C, 0x308: 0x38600000})
assert target(0x230, word(p, 0x230)) == 0x308
p_rows = v.body(C, "?Prepare@RaceCarEntityModule@BrnWorld@@")
assert all("Construct@PlaceOnTrackManager@" not in inst for _, inst in p_rows.values())

r, r_calls = race_body(T, "Release", {0x5C: "?Release@ModuleSingleBuffered@"})
expect(r, {0x18: 0x3BC00001, 0x1C: 0x817F0230, 0x20: 0x2B0B0002,
           0x24: 0x41980030, 0x28: 0x419A0044, 0x58: 0x93DF0230, 0x60: 0x546B063E,
           0x64: 0x2B0B0000, 0x68: 0x419AFFE4, 0x4C: 0x38600000, 0x70: 0x93DF0230,
           0xA0: 0x917F022C})
assert target(0x24, word(r, 0x24)) == 0x54 and target(0x28, word(r, 0x28)) == 0x6C
assert target(0x68, word(r, 0x68)) == 0x4C
_, d_calls = race_body(T, "Destruct", {
    0x18: "?Destruct@ModuleSingleBuffered@",
    0x24: "?Destruct@RaceCarEntityModuleDebugComponent@",
    0x30: "?Destruct@RaceCarStreamer@",
})

world = next(Path(row["selected_object"]) for path, row in compiled["inherited_provider_objects"].items()
             if path.endswith("/BrnWorldModule.cpp"))
v.OUT = world.parent
world_symbol = "?Prepare@WorldModule@BrnWorld@@"
w_rows = v.body(world.stem, world_symbol)
_, world_calls = v.verify_coff_calls(world.stem, world_symbol, [0x4B0],
                                     ["?Prepare@RaceCarEntityModule@"], w_rows)
v.OUT = OUT

artist = {0x822FD8BC: 0x8286E768, 0x822FD940: 0x822EA188, 0x823040D8: 0x8286E7A0,
          0x823040E4: 0x8230442C, 0x822BD3D4: 0x8286E970, 0x822BD3E0: 0x822BD3EC,
          0x822F3DD0: 0x8286E040, 0x827D58C4: 0x82303E78}
assert all(target(address, u32(address)) == callee for address, callee in artist.items())
assert u32(0x822FD8D4) == 0x917E0230 and u32(0x822FE3D4) == 0x997F0004
assert u32(0x820D1268 + 0x38) == 0x827DCBB8 and u32(0x820D1268 + 0x3C) == 0x827DCC58

receipt = {
    "commit": compiled["commit"], "tree": compiled["tree"], "parent": compiled["base"],
    "compile_receipt_sha256": v.sha(compile_path),
    "artist_ppc_sha256": v.sha(AUDIT / "ARTIST_RACECAR_LIFECYCLE_PPC.txt"),
    "historical_witness_sha256": v.sha(AUDIT / "HISTORICAL_WITNESS.json"),
    "artist_branches": {f"0x{address:X}": f"0x{callee:X}" for address, callee in artist.items()},
    "coff_relocations": {"construct": c_calls, "prepare": p_calls, "release": r_calls,
                         "destruct": d_calls, "inherited_world_prepare": world_calls},
    "coff_retry_targets": {"prepare_base_false": "0x308", "release_base_false": "0x4C",
                           "release_done_bypass": "0x6C"},
    "coff_layout": {"release_cursor": "0x230", "prepare_cursor": "0x22C", "new_module_flag": "0x4",
                    "place_on_track_owner": "0x17850"},
    "scope": "Two focused XDK owners and an inherited normal caller. Strict link and runtime pending.",
}
(OUT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
print("Verified native RaceCar lifecycle calls, exact state stores, and early-out branches")

