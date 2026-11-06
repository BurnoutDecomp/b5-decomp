"""Check the ARTIST per-frame hitch path and its private native PPC rebuild."""

import argparse
import json
from pathlib import Path
import struct
import subprocess


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
HEAD = "fad36d9368195937112bbdf88450c276360b7d2a"
PARENT = "a6cd1614e3e720ad6575db62e98aee7165d381b1"
R155 = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
SOURCE = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.cpp"
RESOLVE = "?ResolveArticulatedJoints@PhysicalTrafficManager@Vehicle@BrnPhysics@@AAAXXZ"
CALLER = "?UpdateTrafficPhysicsPostSimulation@PhysicalTrafficManager@Vehicle@BrnPhysics@@QAAXPBUOutputBuffer@PhysicsSimulationIO@CgsPhysics@@M@Z"
helper = WAVE / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]
original = readers["original"]


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def word(code, offset=0):
    return struct.unpack_from(">I", code, offset)[0]


def require(code, offset, words):
    expected = bytes.fromhex(words)
    assert code[offset:offset + len(expected)] == expected, hex(offset)


def art_words(address, words):
    expected = bytes.fromhex(words)
    assert original(address, len(expected)) == expected, hex(address)


def call_target(pc, instruction):
    assert instruction & 0xfc000003 == 0x48000001, hex(pc)
    return readers["branch_target"](pc, instruction)


def conditional_target(pc, instruction):
    assert instruction >> 26 == 16 and instruction & 3 == 0, hex(pc)
    delta = instruction & 0xfffc
    return pc + delta - (0x10000 if delta & 0x8000 else 0)


def coff(path, symbol):
    raw = path.read_bytes()
    assert raw[:2] == b"\xf2\x01"
    code, relocations = readers["collect"](path, symbol)
    names = {(offset, kind): readers["object_symbol"](raw, index)[0]
             for offset, index, kind in relocations}
    return code, relocations, names


def check_artist():
    art_words(0x825f0ab4, "39299868 7d2a4b78 91210084 e90a0000 2b280000 409a0024")
    art_words(0x825f0c30, "81760024 2f0b0001 409a0818 7ec3b378 4bfc27d9 546b063e 2b0b0000 419a0804")
    art_words(0x825f0fbc, "57cbd1be 7bca06a0 396b330d 556b1838 7d2b902a 39600001 7d6a5036 "
                           "7d4a4838 2b2a0000 409a0008 7f6bdb78 556b063e 2b0b0000 419a0460")
    art_words(0x825f1384, "4bfe710d 7f03b800 419a001c")
    for pc in (0x825f0c38, 0x825f0c4c, 0x825f0ff0):
        assert conditional_target(pc, word(original(pc, 4))) == 0x825f1450
    assert conditional_target(0x825f1640, word(original(0x825f1640, 4))) == 0x825f0c00
    for pc, destination in [(0x825f0c40, 0x825b3418), (0x825f0f28, 0x825d8490),
                            (0x825f1384, 0x825d8490), (0x825f13b0, 0x825c0220),
                            (0x825f13c4, 0x825c0220), (0x826375c0, 0x825f0a90),
                            (0x826375c8, 0x825bd150)]:
        assert call_target(pc, word(original(pc, 4))) == destination
    art_words(0x825f13dc, "8176001c 1000034a 396b0040 112c6184 11a06b4a 114058c3")
    art_words(0x825f142c, "15ac5890 11ad4cc4 11aa680a 11a059c3 817c001c 396b0040 11a058c3 10006aee 100059c3")


def check():
    assert git("rev-parse", f"{HEAD}^") == PARENT
    assert git("diff", "--name-only", PARENT, HEAD) == SOURCE
    assert git("rev-parse", f"{PARENT}:{SOURCE}") == git("rev-parse", f"{R155}:{SOURCE}")
    assert git("rev-parse", "codex/experimental-x360-r156-offline-junkyard-exit") == "43ba142ff2345d82c11fcec94dd9ac025271bf1a"
    check_artist()

    link = WAVE / "r155-link"
    assert json.loads((link / "source-inventory.json").read_text())["head"] == R155
    assert sha(link / "native-game.pe") == "c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510"
    assert sha(link / "native-game.map") == "62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a"
    assert sha(link / "seal.json") == "a969af86b6beeb39371dfbb536a4ba7eae109d541e5fef8868f96fdd5e27a28f"
    row, = [item for item in json.loads((link / "invocation.json").read_text())["objects"] if item["source"] == SOURCE]
    old_obj = Path(row["object"])
    assert sha(old_obj) == row["sha256"] == "3620695d2e3e703e59d1fd41a3abfdbd7ed7530c3f2f8d44fcfd781a7fb5f144"
    address, read = readers["linked_reader"](link)
    old, old_relocs, old_names = coff(old_obj, RESOLVE)
    old_va = address(RESOLVE)
    assert old_va == 0x8257e5d8
    readers["fixed_equal"](old, read(old_va, len(old)), old_relocs)
    for offset, label in [(0x3f4, "?Translate@"), (0x404, "?Translate@"), (0x414, "?JointResolveWitness@")]:
        name = old_names[offset, 6]
        assert name.startswith(label)
        pc = old_va + offset
        assert call_target(pc, word(read(pc, 4))) == address(name)
    assert b"[T-joint] resolved cab=" in old_obj.read_bytes()

    report = HERE / "compile-results.json"
    compile_result = json.loads(report.read_text())
    assert compile_result["passed"] == 1 and compile_result["failed"] == 0
    compiled, = compile_result["results"]
    assert compiled["source"] == SOURCE and compiled["machine"] == 0x1f2
    obj = Path(compiled["object"])
    raw = obj.read_bytes()
    assert b"[T-joint] resolved cab=" not in raw
    code, relocations, names = coff(obj, RESOLVE)
    assert len(code) == 0x430

    # Ignore the three removed diagnostic/translation calls and the retained assert sites.
    def control_calls(method_names):
        return [name for (_, kind), name in sorted(method_names.items())
                if kind == 6 and not name.startswith(("?Translate@", "?JointResolveWitness@"))
                and "@Assert@CgsDev@@" not in name]

    assert control_calls(old_names) == control_calls(names)
    assert not any(name.startswith(("?Translate@", "?JointResolveWitness@")) for name in names.values())
    require(code, 0x14, "3c790002 38639868 9061005c")
    require(code, 0xfc, "817e0024 2f0b0001 409a0308")
    require(code, 0x110, "546b063e 2b0b0000 419a02f4")
    require(code, 0x1c4, "7c7f1b78 2b1f0014 4198001c")
    require(code, 0x1e8, "57ebd1be 7bea06a0 396b330d 556b1838 7d2bc82a 39600001 7d6a5036 "
                         "7d4a4838 2b2a0000 409a0008 39600000 556b063e 2b0b0000 419a01f0")
    require(code, 0x370, "7f03a000 419a001c")
    for offset in (0x104, 0x118, 0x21c):
        assert conditional_target(offset, word(code, offset)) == 0x40c
    assert conditional_target(0x2c, word(code, 0x2c)) == 0x424
    assert conditional_target(0x420, word(code, 0x420)) == 0xcc
    assert names[0x20, 6].startswith("?GetFirstNonZeroBit@")
    assert names[0x414, 6].startswith("?GetNextNonZeroBit@")
    assert names[0x1c0, 6] == names[0x36c, 6] and names[0x1c0, 6].startswith("?GetIndexOfOtherHalf@")
    assert names[0x398, 6] == names[0x3a4, 6] and names[0x398, 6].startswith("?GetArticulationPointWorldSpace@")

    _, index, _ = next(reloc for reloc in relocations if reloc[0] == 0x34 and reloc[2] == 0x10)
    symbol, value, section = readers["object_symbol"](raw, index)
    assert names[0x48, 0x11] == symbol and section > 0
    _, _, _, size, offset = struct.unpack_from("<8sIIII", raw, 20 + (section - 1) * 40)
    assert value + 4 <= size and raw[offset + value:offset + value + 4] == bytes.fromhex("3f000000")
    require(code, 0x3b0, "101f038c")
    require(code, 0x3cc, "10000184 118058c3 817e001c 11ac684a 118050c3 396b0040 116058c3 154c6890 "
                         "100a04c4 100b000a 100059c3 817f001c 396b0040 100058c3 100c036e 100059c3")

    old_caller, caller_relocs, old_caller_names = coff(old_obj, CALLER)
    new_caller, _, new_caller_names = coff(obj, CALLER)
    readers["fixed_equal"](old_caller, new_caller, caller_relocs)
    assert old_caller_names == new_caller_names and new_caller_names[0x134, 6] == RESOLVE
    assert new_caller_names[0x13c, 6].startswith("?GetJointSpyQueue@")
    assert new_caller_names[0x15c, 6].startswith("?GetEvent@?$BaseEventQueue@UOutJointSpy@")

    return {
        "state": "verified targeted PPC",
        "head": HEAD, "parent": PARENT, "tree": git("rev-parse", f"{HEAD}^{{tree}}"),
        "source": SOURCE, "source_blob": git("rev-parse", f"{HEAD}:{SOURCE}"),
        "compiled_object_sha256": sha(obj), "compile_report_sha256": sha(report),
        "original": {"method": "0x825f0a90", "body_store_sites": ["0x825f1438", "0x825f144c"],
                     "trailer_fused": "0x825f1448", "common_next_slot": "0x825f1450"},
        "r155_baseline": {"head": R155, "method": hex(old_va), "selected_object_sha256": sha(old_obj),
                          "removed_calls": ["0x3f4 Translate", "0x404 Translate", "0x414 JointResolveWitness"]},
        "candidate": {"cab_sign_xor": "0x3ec", "trailer_fused": "0x404",
                      "body_store_sites": ["0x3f4", "0x408"],
                      "common_next_slot": "0x40c", "post_simulation_caller_matches_r155": True},
        "scope": "Existing live-slot walk, cab/joint/partner gates, assertions and joint spy drain retained. "
                 "The correction writes only the two translations. Joint creation/removal and lifetime state have no source changes.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    result = json.dumps(check(), indent=2) + "\n"
    if args.write:
        with (HERE / "R156_ARTICULATED_RESOLVE_COFF.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(result)
    print(result, end="")
