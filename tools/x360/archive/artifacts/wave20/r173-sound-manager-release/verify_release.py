"""Check the focused PPC Release methods and their scene helper against ARTIST."""
import hashlib
import json
from pathlib import Path
import runpy
import struct
import sys

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
WORKFLOW = Path("D:/Reverse/BP-Decomp_Workflow")
sys.path.insert(0, str(WORKFLOW / "tools/re"))
import x360rd

HELPER = OUT.parent / "r148-boot-presentation-linked/verify_linked.py"
assert hashlib.sha256(HELPER.read_bytes()).hexdigest() == "434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba"
Coff = runpy.run_path(str(HELPER))["Coff"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def words(data):
    return [word for (word,) in struct.iter_unpack(">I", data)]


def signature(prefix, obj):
    symbol, code, refs = obj.function(prefix)
    assert all(kind == 6 for kind, _ in refs.values())
    return symbol.name, words(code), {at: target.name for at, (_, target) in refs.items()}


def art(address, count):
    return words(x360rd.rd(address, count * 4))


def main():
    report = json.loads((OUT / "compile-results.json").read_text())
    objs = {}
    for row in report["results"]:
        path = Path(row["object"])
        payload = path.read_bytes()
        assert row["returncode"] == 0 and row["machine"] == 0x1F2 and sha(payload) == row["object_sha256"]
        objs[path.stem.split("-")[0]] = Coff(payload)

    data = {}
    for owner, prefix in (
        ("BrnEmitterStateManager", "?Release@EmitterStateManager"),
        ("BrnStreamingStateManager", "?Release@StreamingStateManager"),
        ("BrnPlayerVehicleStateManager", "?Release@PlayerVehicleStateManager"),
        ("BrnSoundWorldScene", "?Release@SoundWorldScene"),
        ("BrnSoundWorldScene", "?Destruct@SoundWorldScene"),
    ):
        data[prefix] = signature(prefix, objs[owner])

    emitter, streaming, player, scene, destruct = data.values()
    assert [x360rd.u32(va + 0x10) for va in (0x820B81C0, 0x820B7F80, 0x820B87A0, 0x820B8484)] == [
        0x826E7080, 0x82C296C8, 0x826EFBB8, 0x826D4CC8,
    ]
    assert streaming[1] == art(0x82C296C8, 2) and not streaming[2]
    assert emitter[1][4] == 0x3BE30098 and emitter[1][9] == 0x38600001
    assert player[1][3] == 0x38630098 and player[1][5] == 0x38600001
    assert emitter[2] == {0x18: scene[0], 0x20: destruct[0]}
    assert player[2] == {0x10: scene[0]}
    assert list(scene[2]) == [0x28] and scene[2][0x28].startswith("?RemoveRequests@ResourceRegistrar@")
    assert not destruct[2]
    # Both ARTIST managers inline the same request dispatch, passing the scene at +0x98.
    assert art(0x826E7090, 7) == art(0x826EFBC8, 7)
    assert art(0x826E7090, 1) == [emitter[1][4]]
    assert scene[1][5:10] == art(0x826E7098, 5)
    writes = art(0x826E70B8, 4)
    assert writes == art(0x826E70C8, 4) == art(0x826EFBF0, 4) == scene[1][13:17]
    # Destruct receives the same scene as its direct r3 argument instead of a saved r31.
    r31_to_r3 = lambda word: (word & ~0x1F0000) | (3 << 16)
    assert destruct[1] == [0x39600000, *map(r31_to_r3, writes), 0x4E800020]
    assert art(0x826E70AC, 1) == [0x4BFFB175] and art(0x826EFBE4, 1) == [0x4BFF263D]
    out = {
        "base": report["base"],
        "functions": {prefix: {"symbol": name, "instruction_words": [f"{w:08x}" for w in code],
                               "relocations": {hex(at): target for at, target in refs.items()}}
                      for prefix, (name, code, refs) in data.items()},
        "scene_stores": ["0x384:word=0", "0x388:word=0", "0x390:halfword=0", "0x392:byte=0"],
        "scope": "Focused derived bodies only. Parent Release slot and full linked ABI are separate.",
    }
    target = OUT / "PPC_RELEASE_PROOF.json"
    target.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print("PASS five Release/lifecycle bodies", sha(target.read_bytes()))


if __name__ == "__main__":
    main()

