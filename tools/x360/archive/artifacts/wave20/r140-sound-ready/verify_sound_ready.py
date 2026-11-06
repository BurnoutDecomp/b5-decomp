from pathlib import Path
import hashlib
import json
import struct
import sys

import pefile

root = Path(__file__).parent
common = (root.parent / "r137-intake/verify_netconn.py").read_text()
exec(common[common.index("def coff("):common.index("worker=")])
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd, u32

obj = root / "native-selected/BrnSoundLogicModule-a535b7a3.obj"
symbols, sections = coff(obj.read_bytes())
prepare = "?Prepare@SoundLogicModule@Module@BrnSound@@UAA_NPAUIResourceAllocator@rw@@PAUIOBuffer@CgsModule@@1@Z"
ready = "?ResourcesAreReady@SoundLogicModule@Module@BrnSound@@UAAXXZ"
vtable = "??_7SoundLogicModule@Module@BrnSound@@6BIResourceRequester@Logic@2@@"

def body(symbol):
    (data, relocs), start = find(symbol, symbols, sections)
    assert start == 0
    return data, relocs

def check_bytes(symbol, offset, expected):
    expected = bytes.fromhex(expected)
    data, _ = body(symbol)
    assert data[offset:offset + len(expected)] == expected, (symbol, hex(offset))

def check_rel(symbol, offset, target):
    _, relocs = body(symbol)
    assert any(off == offset and name.startswith(target) for off, _, name in relocs), (symbol, hex(offset), target)

assert u32(0x820D0E98) == 0x826978D0
assert rd(0x820AEC08, 56).split(b"\0")[0] == b"mePrepareStage == E_PREPARESTAGE_LOADING_GLOBAL_DATA"
assert rd(0x826978E8, 16).hex() == "3fdf00013bdee86c817e00002f0b0003"
assert rd(0x8269791C, 12).hex() == "394000043d6082f3915e0000"
assert rd(0x82703D50, 20).hex() == "3d40000139600002614a3500917d00007f9f512e"
assert rd(0x82703E44, 4).hex() == "7f9f512e"

pe = pefile.PE(str(root.parent / "r137-link/native-game.pe"))
def linked(address, size):
    return pe.get_data(address - 0x82000000, size)
assert struct.unpack_from(">I", pe.__data__, 0xCDF8C)[0] == 0x82601FC8
assert linked(0x825E58CC, 20).hex() == "807d000081630000816b00007d6903a64e800421"
assert linked(0x825E686C, 4).hex() == "4bffee15"
assert linked(0x8260310C, 4).hex() == "4bfe36cd"
assert linked(0x82603B20, 8).hex() == "39600004917e0000"

check_bytes(ready, 0x10, "3fbe0001 3bbde86c 817d0000 2f0b0003")
check_bytes(ready, 0x48, "39600004 3c7e0001 3863e878 917d0000")
check_rel(ready, 0x58, "?ResolveLoadedCollection@burnoutglobaldata")
check_rel(vtable, 0, ready)
check_bytes(prepare, 0x148, "3d400001 39600002 614a3500 917d0000 7f7f512e")
check_bytes(prepare, 0x160, "3b600002")
check_bytes(prepare, 0x228, "616a3500")
check_bytes(prepare, 0x230, "387f4c90")
check_bytes(prepare, 0x23c, "7f7f512e")
check_rel(prepare, 0x244, "?LoadAsset@IResourceRequester")
check_bytes(prepare, 0x248, "39600003 7f83e378 917d0000")
check_rel(prepare, 0x254, "?LockForWrite@IOBuffer")
check_rel(prepare, 0x25c, "?ResourceBridging@SoundLogicModule")
check_rel(prepare, 0x264, "?UnlockForWrite@IOBuffer")
check_bytes(prepare, 0x268, "48000070")
check_bytes(prepare, 0x2d8, "817f0000")
results = json.loads((root / "native-selected/compile-results.json").read_text())["results"]
assert len(results) == 3 and all(x["passed"] and x["machine"] == 0x1f2 for x in results)

report = {
    "baseline": "ef3f9b543b9b2482e11d5c77914da04595d2b884",
    "scope": "Static ARTIST and linked R137 comparison, Xbox 360 SDK PPC COFF verification",
    "artist": {
        "prepare": "0x82703D50–60 writes prepare 2 and release 1. 0x82703E44 writes release 2 before LoadAsset. 0x82703E50–70 leaves prepare 3 through unlock and false exit.",
        "requester_vtable": "0x820D0E98 slot 0 -> 0x826978D0",
        "callback": "Assert prepare 3 at 0x826978E8–F8 and store 4 at 0x8269791C–24, then resolve the collection. Secondary this +0xE86C is complete-object +0x134FC."
    },
    "linked_r137": {
        "completion": "ResourceBridging 0x8260310C -> Registrar::Update 0x825E67D8 -> UpdateRequests 0x825E5680. 0x825E58CC–DC loads requester and calls primary requester vtable slot 0. SoundLogic requester vtable 0x820CDF8C slot 0 -> 0x82601FC8.",
        "premature_transition": "0x82603B20–24 writes prepare 4 after bridging."
    },
    "candidate": {
        "callback": "Secondary requester vtable slot 0 relocates to callback. Callback +0x10–1C checks 3 at secondary +0xE86C; +0x48–58 writes 4 and calls existing collection resolver.",
        "prepare": "+0x148–158 writes release 1 after base success. +0x228–244 writes release 2 before LoadAsset with requester subobject +0x4C90. +0x248–268 sets prepare 3, locks, bridges, unlocks and branches over stage 4 to detach/false.",
        "compiled": [x["source"] for x in results],
        "changed_object_sha256": hashlib.sha256(obj.read_bytes()).hexdigest()
    }
}
(root / "R140_SOUND_READY_PROOF.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
