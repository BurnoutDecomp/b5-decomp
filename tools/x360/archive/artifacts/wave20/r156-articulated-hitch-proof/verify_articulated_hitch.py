"""Check ARTIST and the private native PPC articulated hitch witnesses."""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess


HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
PROTOTYPE = WAVE / "r156-articulated-prototype"
REPO = Path("C:/Users/adri1/.codex/worktrees/bp360/native_lion")
BASE = "9a2ff3bfcc3fa395581dd515da98cb3656aecc5b"
HEAD = "a6cd1614e3e720ad6575db62e98aee7165d381b1"
OWNER = "src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_Create.cpp"
BODY = "src/GameSource/Physics/PhysicsUtilities/ExternallySimulatedBody.cpp"
HEADER = BODY.removesuffix(".cpp") + ".h"
helper = WAVE / "r128-batch/verify_matches.py"
readers = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], readers)
sha = readers["sha"]
original = readers["original"]


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def word(code, offset):
    return struct.unpack_from(">I", code, offset)[0]


def require(code, offset, words):
    expected = bytes.fromhex(words)
    assert code[offset:offset + len(expected)] == expected, hex(offset)


def original_words(va, words):
    expected = bytes.fromhex(words)
    assert original(va, len(expected)) == expected, hex(va)


def art_call(pc, target):
    instruction = word(original(pc, 4), 0)
    assert instruction & 0xfc000003 == 0x48000001
    assert readers["branch_target"](pc, instruction) == target, hex(pc)


def object_methods(folder, basename, method):
    path = folder / (basename + ".obj")
    assembly = path.with_suffix(".asm").read_text(errors="replace")
    symbol, = re.findall(rf"(?m)^; Begin code for function: (\?{method}@\S+)", assembly)
    raw = path.read_bytes()
    assert raw[:2] == b"\xf2\x01"
    code, relocations = readers["collect"](path, symbol)
    names = {(offset, kind): readers["object_symbol"](raw, index)[0]
             for offset, index, kind in relocations}
    return path, code, names


def check():
    assert git("rev-parse", f"{HEAD}^") == BASE
    assert set(git("diff", "--name-only", BASE, HEAD).splitlines()) == {OWNER, HEADER}
    assert git("rev-parse", "codex/experimental-x360-r156-offline-junkyard-exit") == "43ba142ff2345d82c11fcec94dd9ac025271bf1a"
    assert git("rev-parse", "HEAD") == HEAD and not git("status", "--porcelain=v1")
    report = HERE / "compile-results.json"
    compiled = json.loads(report.read_text())
    assert compiled["passed"] == 2 and compiled["failed"] == 0
    assert {row["source"] for row in compiled["results"]} == {OWNER, BODY}
    for row in compiled["results"]:
        assert row["passed"] and row["machine"] == 0x1f2

    original_words(0x82648f80, "15bd0470 151ef6f3 1800fe33 157ffef3 817f001c 396b0040 114058c3")
    original_words(0x82648fb8, "14006890 1400e313 100a000a 100059c3 817e001c 396b0040 100058c3 141b6cf0 100059c3")
    original_words(0x826490b0, "814100a4 7c6b1b78 806100d4 39010130 38e100f0 7fa6eb78 7f85e378 808a0000 102058c3")
    original_words(0x82649108, "93bf002c")
    original_words(0x8264913c, "93be002c")
    for pc, destination in [(0x82648f44, 0x825f3b68), (0x82648f54, 0x825f3b68),
                            (0x82648f60, 0x825c0220), (0x82648f74, 0x825c0220),
                            (0x826490ac, 0x825c0220), (0x826490d4, 0x826009c8)]:
        art_call(pc, destination)

    create = "BrnPhysicalTrafficManager_Create-f218489f"
    method = "ProcessCreateArticulatedTrafficEvents"
    old_obj, old, _ = object_methods(PROTOTYPE, create, method)
    assert sha(old_obj) == "c0b567105649c1b0a485f7cb4bdc8392aa618bfb18e208244416e2bac7bf3d9d"
    require(old, 0x420, "14006890 11abb8c3 114d004a")
    require(old, 0x450, "e9490000 39290008 f94b0000 396b0008 4200fff0")
    require(old, 0x478, "11abb8c3 116d000a")
    require(old, 0x4ac, "e92a0000 394a0008 f92b0000 396b0008 4200fff0")

    obj, code, calls = object_methods(HERE, create, method)
    assert calls[0x110, 0x10] == calls[0x12c, 0x11] == "__real@3f000000"
    require(code, 0x114, "101f038c")
    require(code, 0x124, "1bc000dc")
    require(code, 0x420, "817f001c 100d004a 396b0040 11a058c3 159f04b0 158cf313 11ad600a 11a059c3 "
                         "817d001c 396b0040 11a058c3 15bf04f0 11a059c3")
    require(code, 0x528, "80960000 390100c0 8061008c 38e10100 7f86e378 7fc5f378 102058c3")
    require(code, 0x578, "93df002c")
    require(code, 0x5ac, "93dd002c")
    for offset, method in [
        (0x1c0, "GetCreateCabEvent"), (0x1cc, "GetCreateTrailerEvent"),
        (0x3e4, "SetArticulated"), (0x3f4, "SetArticulated"),
        (0x400, "GetArticulationPointWorldSpace"), (0x40c, "GetArticulationPointWorldSpace"),
        (0x520, "GetArticulationPointWorldSpace"), (0x544, "CreateJoint"),
    ]:
        assert calls[offset, 6].startswith(f"?{method}@")

    # Retained joint frame and pair removal paths in the private prototype.
    frame_obj, _, frame_calls = object_methods(PROTOTYPE, "BrnArticulatedJointPool-7ade32e5", "ConstructArticulatedJoint")
    assert sha(frame_obj) == "03b306c0140325bf7fea6abbf0c00197a5066c33b626b7755144e8f41a828b67"
    for art_pc, target, offset, method in [
        (0x825d81b0, 0x8259b580, 0x3b8, "SetChildAngularFrame"),
        (0x825d81d4, 0x8259b728, 0x3f4, "SetParentAngularFrame"),
        (0x825d8204, 0x8259b8d0, 0x424, "SetParentLinearFrame"),
    ]:
        art_call(art_pc, target)
        assert frame_calls[offset, 6].startswith(f"?{method}@JointFrames@")
    art_call(0x8261ce14, 0x825d8490)
    art_call(0x8261cf48, 0x825d8490)
    art_call(0x8261cfbc, 0x825d8248)
    original_words(0x8261cf74, "93fe0024 3960ffff 93fe0028 3b7b94bc 917e002c")
    original_words(0x8261cfc0, "3960ffff 93f80024 7dc57378 93f80028 5604063e 7f43d378 9178002c")
    remove_obj, remove, remove_calls = object_methods(PROTOTYPE, "BrnPhysicalTrafficManager_Remove-ccf6764a", "RemoveTrafficVehicle")
    assert sha(remove_obj) == "4d8bcdd9f6ea87c4b9764832c433d6db59a7169e31aed6179ec09fa00b0ecb02"
    for offset in (0xec, 0x1b4):
        assert remove_calls[offset, 6].startswith("?GetIndexOfOtherHalf@ArticulatedJointPool@")
    assert remove_calls[0x224, 6].startswith("?RemoveJoint@ArticulatedJointPool@")
    require(remove, 0x1e0, "92bf0024 92bf0028 3bbd94bc 931f002c")
    require(remove, 0x228, "7ec5b378 92bc0024 56e4063e 92bc0028 7fc3f378 931c002c")

    return {
        "state": "verified targeted PPC",
        "base": BASE,
        "head": HEAD,
        "tree": git("rev-parse", f"{HEAD}^{{tree}}"),
        "changed_sources": {path: git("rev-parse", f"{HEAD}:{path}") for path in (OWNER, HEADER)},
        "compile_report_sha256": sha(report),
        "candidate_coff": [{"source": row["source"], "sha256": sha(Path(row["object"]))}
                           for row in compiled["results"]],
        "original": {"separation": "0x82648f80", "cab_store": "0x82648fc4",
                     "trailer_fused_update": "0x82648fd4", "trailer_store": "0x82648fd8",
                     "create_joint": "0x826490d4"},
        "candidate": {"create_object_sha256": sha(obj), "cab_store": "0x43c",
                      "trailer_fused_update": "0x44c", "trailer_store": "0x450",
                      "create_joint": "0x544", "joint_index_stores": ["0x578", "0x5ac"]},
        "prototype": {"create_object_sha256": sha(old_obj), "cab_matrix_copy_loop": "0x450",
                      "trailer_matrix_copy_loop": "0x4ac", "trailer_separate_add": "0x47c"},
        "retained": {"joint_frame_object_sha256": sha(frame_obj), "remove_object_sha256": sha(remove_obj),
                     "partner_lookups": ["0x8261ce14", "0x8261cf48"], "remove_joint": "0x8261cfbc"},
        "scope": "Private prototype. Only body translation rows are written. The cab scales, sign-xors, and adds. The trailer performs one fused multiply-add.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    result = json.dumps(check(), indent=2) + "\n"
    if args.write:
        with (HERE / "R156_ARTICULATED_HITCH_COFF.json").open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(result)
    print(result, end="")
