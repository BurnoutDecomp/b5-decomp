
"""Check selected R164 GameData owners and original PPC route operands."""
import json
from pathlib import Path
import sys

OUT = Path(__file__).resolve().parent
sys.path.insert(0, str(OUT.parent / "r161-native-inputpads"))
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import build_focus as b
import verify_focus as v
from x360rd import rd, u32

v.OUT = OUT
b.REPO = v.REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
MODULE = "BrnGameDataModule"
COMPILE = OUT / "compile-receipt.json"
receipt = json.loads(COMPILE.read_bytes())
assert not v.git("status", "--porcelain=v1")
assert receipt["head"] == v.git("rev-parse", "HEAD")
assert receipt["parent"] == v.git("rev-parse", "HEAD^")
assert receipt["tree"] == v.git("rev-parse", "HEAD^{tree}")
for source, expected in receipt["input_sha256"].items():
    assert v.sha(v.REPO / source) == expected
for row in receipt["results"]:
    name = Path(row["source"]).stem
    assert row["exit_code"] == 0 and not row["warnings"] and row["machine"] == "0x01F2"
    assert v.sha(OUT / f"{name}.obj") == row["ppc_coff_sha256"]
    assert v.sha(OUT / f"{name}.asm") == row["listing_sha256"]

def word(code, offset):
    return int.from_bytes(code[offset:offset + 4], "big")

def branch(pc, opcode):
    width = 26 if opcode >> 26 == 18 else 16
    delta = opcode & ((1 << width) - 4)
    return pc + delta - ((1 << width) if delta >> (width - 1) else 0)

def body(method, edges, words, stem=MODULE):
    symbol = method if method.startswith("?") else f"?{method}@GameDataModule@BrnResource@@"
    rows = v.body(stem, symbol)
    code, targets = v.verify_coff_calls(stem, symbol, edges, edges.values(), rows)
    assert all(word(code, at) == op for at, op in words.items()), (method, words)
    return code, targets

artist_edges = {
    0x826725C0: 0x82670700, 0x826725EC: 0x82670880, 0x8267366C: 0x826712A0,
    0x82673698: 0x82671420, 0x82672B84: 0x82670880, 0x82673DF4: 0x82673F1C,
}
assert all(branch(pc, u32(pc)) == callee for pc, callee in artist_edges.items())
artist_words = {
    0x82670868: 0x38A00004, 0x8267086C: 0x38C00018, 0x82670978: 0x38A00004,
    0x8267097C: 0x38C00018, 0x82671408: 0x38A00003, 0x8267140C: 0x38C00090,
    0x82671518: 0x38A00003, 0x8267151C: 0x38C00090, 0x82673DF0: 0x38A00DBD,
    0x82673E3C: 0x38C00028, 0x82673E44: 0x38A0003A, 0x8266E4FC: 0x38A00E13,
    0x8266E578: 0x38C00020,
}
assert all(u32(at) == op for at, op in artist_words.items())
obj = (OUT / f"{MODULE}.obj").read_bytes()
def literal(address):
    raw = rd(address, 192).split(bytes(1))[0] + bytes(1)
    assert raw in obj, hex(address)
    return raw[:-1].decode("ascii")

files = {f"0x{address:X}": literal(u32(address))
         for address in (0x82F2A708, 0x82F2A70C, 0x82F2A728, 0x82F2A72C)}
assert u32(0x82F32264) == 256

result = {}
def check(method, edges, words, stem=MODULE):
    code, targets = body(method, edges, words, stem)
    result[method] = {"relocations": targets, "words": {hex(k): hex(val) for k, val in words.items()}}
    return code

check("ProcessGetGameDataEvent", {0x600: "?ProcessGetWorldCollisionRequest@", 0x654: "?ProcessGetPVSRequest@"},
      {0x5F0: 0x38C00039, 0x644: 0x38C0003A})
check("ProcessUnloadGameDataEvent", {0x3D0: "?ProcessUnloadWorldCollisionRequest@", 0x424: "?ProcessUnloadPVSRequest@"},
      {0x3C0: 0x38C0002D, 0x414: 0x38C0002E})

resource = "?GetResourceQueue@InputBuffer@ResourceIO@CgsResource@@"
request = "?AddEvent@?$VariableEventQueue@$0EAAA@"
queue = "?AddEvent@BaseEventReceiverQueue@"
push = "?PushIndex@?$IndexedPool@UGameDataEventSlot@"
get_hash = "?HashString@ID@CgsResource@@"
set_file = "?SetFileName@BundleLoaderEvent@"
check("ProcessGetPVSRequest", {0x68: "?FireAssert@", 0xB4: get_hash, 0xC4: resource, 0xD4: request},
      {0x24: 0x2F0B0003, 0x5C: 0x38A0135D, 0xA4: 0x934A0028, 0xAC: 0x817C0008,
       0xB8: 0xF8610070, 0xCC: 0x38A00004, 0xD0: 0x38C00018})
check("ProcessGetWorldCollisionRequest",
      {0x7C: "?FireAssert@", 0xC4: "?FireAssert@", 0x108: get_hash, 0x118: resource, 0x128: request},
      {0x14: 0x616B4324, 0x28: 0x7D7B58AE, 0x70: 0x38A0133F, 0x8C: 0x2F0B0001,
       0xB8: 0x38A01340, 0xF8: 0x92EA0028, 0x100: 0x81790008, 0x10C: 0xF8610080,
       0x120: 0x38A00004, 0x124: 0x38C00018})
check("ProcessUnloadPVSRequest",
      {0x68: "?FireAssert@", 0xB4: set_file, 0xCC: resource, 0xDC: request},
      {0x5C: 0x38A01510, 0xA4: 0x934A0028, 0xA8: 0x83FC0008, 0xBC: 0x93E100EC,
       0xC8: 0x99610068, 0xD4: 0x38A00003, 0xD8: 0x38C00090})
check("ProcessUnloadWorldCollisionRequest",
      {0x7C: "?FireAssert@", 0xC4: "?FireAssert@", 0x108: set_file, 0x120: resource, 0x130: request},
      {0x14: 0x616B4324, 0x70: 0x38A014F3, 0xB8: 0x38A014F4, 0xF8: 0x92EA0028,
       0xFC: 0x83F90008, 0x110: 0x93E100FC, 0x11C: 0x99610078, 0x128: 0x38A00003, 0x12C: 0x38C00090})
check("ProcessInternalLoadBundleResponse",
      {0x344: "?PostGameDataResponse@", 0x35C: push, 0x3A8: "?ProcessGetPVSRequest@"},
      {0x31C: 0x396B4320, 0x32C: 0x38A00020, 0x33C: 0x394A0001, 0x340: 0x914B0000,
       0x398: 0x38C0003A, 0x39C: 0x38BF0008})
acquire = check("ProcessInternalAcquireResponse", {
    0x308: "?PostGameDataResponse@", 0x320: push, 0x328: "?UnlockForWrite@IOBuffer@",
    0x384: "?FireTrackResourceAssert@", 0x38C: "?UnlockForWrite@IOBuffer@",
}, {0x2EC: 0x38E00003, 0x2F0: 0x813D001C, 0x2F8: 0x811D0018, 0x2FC: 0x38A0003A,
    0x37C: 0x38800DBD, 0x390: 0x382100B0})
assert word(acquire, 0x394) >> 26 == 18 and word(acquire, 0x394) & 1 == 0
check("ProcessInternalUnloadResponse",
      {0xE4: "?FireTrackResourceAssert@", 0x1A4: queue, 0x1BC: push},
      {0xBC: 0x2F0B002D, 0xC8: 0x3BBD4320, 0xDC: 0x38800E13,
       0xEC: 0x396BFFFF, 0xF0: 0x917D0000, 0x158: 0x80BF0028, 0x164: 0x38C00020})
post = check("PostGameDataResponse", {0x284: "??$SendHandleResponse@UGetPVSResponse@"},
             {0x20: 0x3944FFE5, 0x44: 0x4E800420})
_, post_refs = v.coff_body(MODULE, "?PostGameDataResponse@GameDataModule@")
assert post_refs[0x48 + 4 * (58 - 27)] == (2, "$LN12")
check("??$SendHandleResponse@UGetPVSResponse@", {0x64: queue},
      {0x50: 0x4E800421, 0x54: 0x38C00028, 0x5C: 0x807F000C})
check("?SetPVSHandle@GetPVSResponse@", {},
      {0x0: 0x81640000, 0x4: 0x39430020, 0x8: 0x916A0000,
       0xC: 0x81640004, 0x10: 0x916A0004})
check("?FireTrackResourceAssert@", {0x14: "?BeginAssert@", 0x4C: "?FireAssert@", 0x50: "?EndAssert@"}, {})
check("?Prepare@PVSModule@", {0xF0: "?LoadPVS@"}, {}, "BrnPVSModule")
check("?PrepareWorldCollision@WorldEntityModule@", {0xB8: "?LoadWorldCollision@"}, {}, "BrnWorldEntityModule")

result = {"head": receipt["head"], "tree": receipt["tree"], "parent": receipt["parent"],
          "compile_receipt_sha256": v.sha(COMPILE), "original_resource_literals": files,
          "artist_branches": {hex(a): hex(b) for a, b in artist_edges.items()}, "compiled": result,
          "scope": "Original XDK COFF and ARTIST operand evidence. Strict link and runtime pending."}
(OUT / "RECEIPT.json").write_text(json.dumps(result, indent=2) + "\n")
print("Verified direct Get/unload edges, 24- and 144-byte requests, load/response routes, and collision diagnostics")

