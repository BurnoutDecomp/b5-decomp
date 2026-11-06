"""Check the ARTIST and native PPC saturated traffic recycle boundary."""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
HEAD = "fad36d9368195937112bbdf88450c276360b7d2a"
PROTOTYPE = "9a2ff3bfcc3fa395581dd515da98cb3656aecc5b"
R155 = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
SOURCE = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Remove.cpp"
RECYCLE = "?RecycleTrafficVehicle@PhysicalTrafficManager@Vehicle@BrnPhysics@@AAAXHPAUVehicleManagerOutputInterface@23@PAUVehicleOutputRequestInterface@23@PAVDeformationInputInterface@Deformation@3@@Z"
REMOVE = "?RemoveTrafficVehicle@PhysicalTrafficManager@Vehicle@BrnPhysics@@AAAXEPAUVehicleOutputRequestInterface@23@PAVDeformationInputInterface@Deformation@3@_N@Z"
CONSTRUCT = "?Construct@VehicleManager@Vehicle@BrnPhysics@@QAAXXZ"
helper = WAVE / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]
art = readers["original"]


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def word(code, offset=0):
    return struct.unpack_from(">I", code, offset)[0]


def require(code, offset, words):
    expected = bytes.fromhex(words)
    assert code[offset:offset + len(expected)] == expected, hex(offset)


def original_words(va, words):
    expected = bytes.fromhex(words)
    assert art(va, len(expected)) == expected, hex(va)


def call(pc, instruction):
    assert instruction & 0xfc000003 == 0x48000001
    return readers["branch_target"](pc, instruction)


def coff(obj, method):
    raw = obj.read_bytes()
    assert raw[:2] == b"\xf2\x01"
    code, relocations = readers["collect"](obj, method)
    targets = {(offset, kind): readers["object_symbol"](raw, index)[0]
               for offset, index, kind in relocations}
    return code, relocations, targets


def check():
    tree = git("rev-parse", f"{HEAD}^{{tree}}")
    assert tree == "2c27af3efe1af5c116641026a9265d722f33c9ae"
    blob = git("rev-parse", f"{HEAD}:{SOURCE}")
    assert blob == git("rev-parse", f"{PROTOTYPE}:{SOURCE}") == git("rev-parse", f"{R155}:{SOURCE}")

    original_words(0x82f2a3a4, "ffffffff")
    original_words(0x8262da1c, "7c9b2378 3d608209 7c7f1b78")
    original_words(0x8262da7c, "3d400001 3a80007f 615e98f0 2b0b0000 3f2082fb 419a00b0")
    original_words(0x8262dabc, "397d64f0 7fa4eb78 557c103a 7f5cf82e")
    original_words(0x8262dafc, "7d7cf82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020")
    original_words(0x8262db30, "7d7cf82e 556bb4be 7d6bfa14 7e8bf1ae 397b64f0 7f64db78 557d103a 7fe3fb78 7f9df82e")
    original_words(0x8262db84, "7d7df82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020")
    original_words(0x8262dbb8, "7d7bfa14 7d6bf0ae 7d6bfa14 7e8bf1ae")
    original_words(0x8261d328, "391d64f0 38e0007f")
    original_words(0x8261d358, "550a103a")
    original_words(0x8261d384, "612898f0")
    original_words(0x8261d3ec, "7d6ad02e 556bb4be 7d6bd214 7ceb41ae 3d6082f3 816ba3a4 7d6ad12e")
    for va in (0x8262daf8, 0x8262db80):
        assert call(va, word(art(va, 4))) == 0x8261cc98
    original_words(0x82637790, "3beb003f 2f1f0014 4098000c 2f1fffff 409a0038")
    assert call(0x826377d4, word(art(0x826377d4, 4))) == 0x8262da10
    assert call(0x82647594, word(art(0x82647594, 4))) == 0x82637608

    target = WAVE / "r156-articulated-prototype/BrnPhysicalTrafficManager_Remove-ccf6764a.obj"
    assert sha(target) == "4d8bcdd9f6ea87c4b9764832c433d6db59a7169e31aed6179ec09fa00b0ecb02"
    recycle, _, targets = coff(target, RECYCLE)
    assert targets[0xdc, 6] == targets[0x160, 6] == REMOVE
    require(recycle, 0x70, "3ac0007f 615e98f0")
    require(recycle, 0xe0, "7d7cf82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020")
    require(recycle, 0x114, "7d7cf82e 556bb4be 7d6bfa14 7ecbf1ae")
    require(recycle, 0x164, "7d7df82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020")
    require(recycle, 0x198, "7d7bfa14 7d6bf0ae 7d6bfa14 7ecbf1ae")
    remove, _, _ = coff(target, REMOVE)
    require(remove, 0x428, "556b103a 614998f0 3900007f 38e0ffff 2b1b0014 7d4bf02e 554ab4be 7d4af214 7d0a49ae 7cebf12e")

    link = WAVE / "r155-link"
    assert json.loads((link / "source-inventory.json").read_text())["head"] == R155
    assert sha(link / "native-game.pe") == "c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510"
    assert sha(link / "native-game.map") == "62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a"
    invocation = json.loads((link / "invocation.json").read_text())
    selected, = [item for item in invocation["objects"] if item["source"] == SOURCE]
    selected_obj = Path(selected["object"])
    assert sha(selected_obj) == selected["sha256"]
    address, read = readers["linked_reader"](link)
    recycle_va = address(RECYCLE)
    linked_recycle, linked_relocs, linked_targets = coff(selected_obj, RECYCLE)
    readers["fixed_equal"](linked_recycle, read(recycle_va, len(linked_recycle)), linked_relocs)
    assert recycle == linked_recycle and targets == linked_targets
    for offset in (0xdc, 0x160):
        pc = recycle_va + offset
        assert call(pc, word(read(pc, 4))) == address(REMOVE)
    linked_remove, remove_relocs, _ = coff(selected_obj, REMOVE)
    assert remove == linked_remove
    readers["fixed_equal"](remove, read(address(REMOVE), len(remove)), remove_relocs)

    original_words(0x8263be4c, "3f7f0002")
    original_words(0x8263be7c, "3b7b7dc0")
    original_words(0x8263bf84, "3b7b0400")
    original_words(0x8263bf94, "3c7f0001 3863aee0")
    constructor = address(CONSTRUCT)
    require(read(constructor + 0x514, 16), 0, "3f9f0002 3fbf0001 3b7f1b24 3b9c7dc0")
    require(read(constructor + 0x5b0, 4), 0, "3b9c0400")
    require(read(constructor + 0x5c0, 8), 0, "3c7f0001 3863aee0")
    map_displacement = 0x198f0
    after_remove_index = (0xffffffff >> 10) & 0x3fff
    outer_seat = 0xaee0 + map_displacement + after_remove_index
    debug_index, debug_offset = divmod(outer_seat - 0x27dc0, 0x400)
    assert (outer_seat, debug_index, debug_offset) == (0x287cf, 2, 0x20f)

    return {
        "state": "verified inherited ARTIST saturation behavior",
        "private_head": HEAD, "tree": tree, "source": SOURCE, "source_blob": blob,
        "prototype_coff_sha256": sha(target), "r155_selected_coff_sha256": sha(selected_obj),
        "artist": {"recycle": "0x8262da10", "partner_remove": "0x8262daf8", "partner_map_store": "0x8262db3c",
                   "own_remove": "0x8262db80", "own_sentinel_map_read": "0x8262db90", "own_indirect_map_store": "0x8262dbc4",
                   "id_invalidation": "0x8261d404", "sentinel": "0xffffffff", "saturation_recycle_call": "0x826377d4"},
        "native": {"r155_recycle": hex(recycle_va), "prototype_partner_store": "0x120",
                   "prototype_own_sentinel_map_read": "0x170", "prototype_own_indirect_map_store": "0x1a4",
                   "prototype_id_invalidation": "0x44c"},
        "after_remove": {"map_index": after_remove_index, "manager_relative_byte": hex(outer_seat),
                         "member_region": "maRaceCarDebugComponent[2] +0x20F, rear lateral grip graph +0x0F"},
        "reachability": "A pair with two free physical slots does not recycle. Pool exhaustion permits recycling an attached pair. "
                        "Its partner-side store writes 0x7F at the sentinel-derived byte before the own-side assertion reads that same byte.",
        "disposition": "No recycle correction. ARTIST and native PPC agree on the off-array addressing and ordering. "
                       "An exact-head runtime result remains unverified.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    report = json.dumps(check(), indent=2) + "\n"
    if args.write:
        with (HERE / "R156_RECYCLE_BOUNDARY.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(report)
    print(report, end="")
