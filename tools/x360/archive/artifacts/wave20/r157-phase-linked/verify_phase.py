"""Check the guarded slot-7 phase and registration edges in R157 PPC COFF and strict PE."""
import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import sys

WAVE = Path(__file__).resolve().parents[1]
GIT = "C:/Users/adri1/.codex/worktrees/bp360/debug_format_luna"
HEAD = "304029b2c8cdc86a8e744c1bdf1470b9b4468a38"
TREE = "41e8700b3dbef3308250b2da6c0be090e9563b0a"
FROZEN = "ffff6742d77340a053ef9f2420068e6ca0919900"
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import u32 as artist_word

COFF = runpy.run_path(str(WAVE / "r148-boot-presentation-linked/verify_linked.py"))
IMAGE = runpy.run_path(str(WAVE / "r154-racecar-prescene-linked/verify_linked.py"))["Image"]
MASKS = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 0x10: 0xFFFF, 0x11: 0xFFFF}

TRAFFIC = "src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"
BRIDGE = "src/GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp"
REPLAYS = "src/GameSource/Replays/"
TRAFFIC_CODEC = REPLAYS + "Serialisers/BrnReplayTrafficEntitySerialiser.cpp"
REPLAY = REPLAYS + "BrnReplayModule.cpp"
WORLD = "src/GameSource/World/BrnWorldModule.cpp"
GAME = "src/GameSource/Game/BrnGameModule.cpp"

T = "@TrafficEntityModule@BrnTraffic@@"
S = "@TrafficEntitySerialiser@BrnReplays@@"
B = "@BaseSerialiser@BrnReplays@@"
P = "@OutputBuffer_PostPhysics@BrnTrafficIO@BrnTraffic@@"
R = "@ReplayModule@BrnReplays@@"

CHECKS = {
    "pre_scene": (TRAFFIC, "?PreSceneUpdate" + T, {
        0x44: "?EnterReplay" + T, 0x4C: "?LeaveReplay" + T,
        0x60: "?CheckStaticLayoutCleared" + S, 0x68: "?IsPlaying" + B,
        0x90: "?Read" + S, 0x98: "?UpdateSerialiser" + T,
        0xA0: "?LockForWrite@IOBuffer@", 0xA8: "?LockForRead@IOBuffer@",
    }, {
        0x54: 0x9BDD0000, 0x58: 0x3B7B24C0, 0x74: 0x419A0020,
        0x78: 0x3D600007, 0x7C: 0x616B251B, 0x80: 0x7D7F58AE,
        0x84: 0x2B0B0000, 0x88: 0x409A000C,
    }),
    "post_physics": (TRAFFIC, "?PostPhysicsUpdate" + T, {
        0x7B0: "?GetReplayRequestInterface" + P + "QAAPAU",
        0x7B8: "?RegisterSerialiser@RequestInterface@ReplayIO@",
        0x7C0: "?UnlockForRead@IOBuffer@", 0x7C8: "?UnlockForWrite@IOBuffer@",
        0x7D0: "?IsRecording" + B, 0x7F8: "?Write" + S,
    }, {
        0x7AC: 0x3BDE24C0, 0x7DC: 0x419A0020, 0x7E0: 0x3D600007,
        0x7E4: 0x616B251B, 0x7E8: 0x7D7F58AE, 0x7EC: 0x2B0B0000,
        0x7F0: 0x409A000C,
    }),
    "world_bridge": (BRIDGE, "?BridgeEntityModulesToOutput_PostPhysics@WorldModule@", {
        0x25C: "?GetReplayRequestInterface" + P + "QBAPBU",
        0x268: "?AppendReplayRequestInterface@UpdateOutputBuffer@BrnWorldIO@",
    }, {}),
    "construct": (TRAFFIC_CODEC, "?Construct" + S, {
        0x38: "?Construct" + B,
    }, {
        0x14: 0x3CE00003, 0x18: 0x3CC00001, 0x20: 0x39200001,
        0x24: 0x60E7D480, 0x28: 0x60C64000, 0x2C: 0x38A00000,
        0x30: 0x38800007, 0x40: 0x997F005C, 0x44: 0x997F005D,
    }),
    "first_static_clear": (TRAFFIC_CODEC, "?CheckStaticLayoutCleared" + S, {
        0x30: "?GetStaticLayout" + S, 0x44: "memset", 0x4C: "?Clear@TrafficSerialiserFrame@",
    }, {
        0x18: 0x897E005C, 0x20: 0x409A0048, 0x24: 0x817E0020,
        0x2C: 0x419A003C, 0x3C: 0x60A5D480, 0x58: 0x616B4280, 0x64: 0x993E005C,
    }),
    "collector": (TRAFFIC, "?UpdateSerialiser" + T, {
        0x1C: "?Lock" + B, 0x24: "?IsRecording" + B,
        0x40: "?Clear@TrafficSerialiserFrame@", 0x218: "memcpy",
        0x220: "?Unlock" + B, 0x2A8: "?Unlock" + B,
        0x2DC: "?Unlock" + B, 0x304: "?Unlock" + B,
    }, {
        0x204: 0x3C9F0005, 0x208: 0x3C790002, 0x20C: 0x38A012D0,
        0x210: 0x38843790, 0x214: 0x38632FB0,
    }),
    "replay_registration_consumer": (REPLAY, "?Update_PostSim" + R, {
        0xA0: "?GetRequestInterface@InputBuffer_PostSim@ReplayIO@BrnReplays@@QBAPBU",
        0xAC: "?StoreSerialisers" + R,
    }, {}),
}

CHECKS.update({
    "world_output_lock": (WORLD, "?Update@WorldModule@BrnWorld@@", {
        0x14CC: "?LockBuffersForIO@CgsModule@", 0x14E8: "?BridgeEntityModulesToOutput_PostPhysics@",
        0x1500: "?UnlockBuffersForIO@CgsModule@",
    }, {}),
    "record_nonempty_slots": (REPLAY, "?UpdateRecording_PostSim" + R, {
        0x108: "?AddFrunk@WriteStream@BrnReplays@",
    }, {
        0xA0: 0x38A0000B, 0xB0: 0x806B0010, 0xB4: 0x2F030000,
        0xB8: 0x40990028, 0xD4: 0x816B0008, 0xF4: 0x40990038,
    }),
})

BASE = REPLAYS + "BrnReplayBaseSerialiser.cpp"
STRICT_CHECKS = {
    "game_request_forward": (GAME, "?DoUpdate_ReplaysPostSim@BrnGameModule@", {
        0x68: "?LockBuffersForIO@CgsModule@",
        0x70: "?GetReplayRequestInterface@UpdateOutputBuffer@BrnWorldIO@",
        0x7C: "?AppendRequestInterface@InputBuffer_PostSim@ReplayIO@",
        0x114: "?UnlockBuffersForIO@CgsModule@",
    }, {
        0x12C: 0x81630000, 0x130: 0x816B0048, 0x134: 0x7D6903A6, 0x138: 0x4E800421,
    }),
    "initial_serialiser": (BASE, "?Construct" + B, {}, {
        0x10: 0x39600000, 0x1C: 0x909F0028, 0x24: 0x917F0008,
        0x2C: 0x917F0010, 0x30: 0x917F0020, 0x38: 0x917F0000,
        0x3C: 0x997F0004, 0x48: 0x993F005B,
    }),
    "is_playing": (BASE, "?IsPlaying" + B, {}, {
        0x0: 0x81630000, 0x4: 0x2F0B0004, 0xC: 0x2F0B0005, 0x14: 0x2F0B0006,
    }),
    "is_recording": (BASE, "?IsRecording" + B, {}, {
        0x0: 0x81630000, 0x4: 0x2F0B0001, 0xC: 0x2F0B0002, 0x14: 0x2F0B0003,
    }),
    "register_slot": (REPLAYS + "BrnReplayRequestInterface.cpp", "?RegisterSerialiser@RequestInterface@", {}, {
        0x1C: 0x83FD0028, 0x20: 0x2F1F000B, 0x44: 0x57FF103A, 0x70: 0x7FBFF12E,
    }),
    "store_buffers": (REPLAYS + "BrnReplayModule_Prepare.cpp", "?StoreSerialisers" + R, {
        0xA8: "?Malloc@LinearMalloc@CgsMemory@", 0xE8: "?Malloc@LinearMalloc@CgsMemory@",
    }, {
        0x34: 0x3B00000B, 0x40: 0x83F90000, 0x54: 0x419A00BC,
        0x8C: 0x93FE0000, 0x90: 0x809F000C, 0xB0: 0x907F0008,
        0xD0: 0x809F0024, 0xF0: 0x907F0020,
    }),
}

ARTIST_FIXED = {
    0x8264C998: 0x39200001, 0x8264C9A8: 0x38800007, 0x8264C2C8: 0x993F005B,
    0x8274AA30: 0x7D7F58AE, 0x8274AA38: 0x409A0010,
    0x8274EF28: 0x7D7F58AE, 0x8274EF30: 0x409A0010,
    0x8265343C: 0x817E0020, 0x82653444: 0x419A0080,
}

ARTIST_EDGES = {
    0x82740860: 0x8264C978, 0x8264C9B0: 0x8264C280,
    0x8274A9EC: 0x82653418, 0x8274AA44: 0x8265F2D8, 0x8274AA4C: 0x8272DA80,
    0x8274AA54: 0x82204A18, 0x8274AA5C: 0x82204C88,
    0x8274EED4: 0x82711D90, 0x8274EEDC: 0x821F34A0,
    0x8274EEE4: 0x82204D50, 0x8274EEEC: 0x82204BB8, 0x8274EF3C: 0x8265F4E8,
    0x827AF1AC: 0x827A0B78, 0x827AF1B8: 0x827A4CA8,
    0x8272DAA4: 0x823A6718, 0x8272E248: 0x823A67C0,
    0x8265F460: 0x8265DD38, 0x8265F5A4: 0x8265D288,
    0x823DD4AC: 0x823B6698, 0x823DD4B8: 0x823BB320, 0x8264B71C: 0x82866DC0,
    0x8264B7E8: 0x82866DC0, 0x8265F1E8: 0x8265EB70, 0x827CFF74: 0x8274E6D0,
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(path.read_bytes())


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", GIT, *args])


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def word(code, offset):
    return struct.unpack_from(">I", code, offset)[0]


def check_artist():
    result = {}
    for address, expected in ARTIST_FIXED.items():
        actual = artist_word(address)
        assert actual == expected, (hex(address), hex(actual), hex(expected))
        result[hex(address)] = hex(actual)
    for address, expected in ARTIST_EDGES.items():
        raw = artist_word(address)
        assert raw >> 26 == 18 and raw & 1, (hex(address), hex(raw))
        actual = IMAGE.target(None, address, raw)
        assert actual == expected, (hex(address), hex(actual), hex(expected))
        result[hex(address)] = hex(actual)
    return result


def bind(name, specification, objects, linked=None):
    source, prefix, edges, fixed = specification
    row = objects[source]
    data = Path(row["object"]).read_bytes()
    assert digest(data) == row["sha256"], source
    blob = git("rev-parse", f"{HEAD}:{source}").decode().strip()
    assert row.get("blob", blob) == blob
    symbol, code, refs = COFF["Coff"](data).function(prefix)
    owner = Path(row["object"]).name
    result = {
        "source": source, "git_blob": blob, "object": str(row["object"]),
        "object_sha256": row["sha256"], "symbol": symbol.name, "coff_sha256": digest(code),
        "fixed_words": {}, "call_edges": {},
    }
    if linked:
        image, entries, source_rows, link_head = linked
        sealed_source = source_rows[source.lower()]
        source_data = Path(sealed_source["path"]).read_bytes()
        assert digest(source_data) == sealed_source["sha256"]
        assert source_data.replace(b"\r\n", b"\n") == git("show", f"{link_head}:{source}").replace(b"\r\n", b"\n")
        link_blob = git("rev-parse", f"{link_head}:{source}").decode().strip()
        if name in CHECKS:
            assert link_blob == blob, (source, blob, link_blob)
        result.update(link_git_blob=link_blob, link_source_sha256=sealed_source["sha256"])
        [va] = {va for va, module in entries[symbol.name] if module.lower() == owner.lower()}
        actual = b"".join(image.word(va + offset).to_bytes(4, "big") for offset in range(0, len(code), 4))
        for offset in range(0, len(code), 4):
            mask = MASKS[refs[offset][0]] if offset in refs else 0
            assert not ((word(actual, offset) ^ word(code, offset)) & ~mask), (name, hex(offset))
        result.update(linked_va=hex(va), linked_sha256=digest(actual))

    for offset, expected in fixed.items():
        assert offset not in refs and word(code, offset) == expected, (name, hex(offset), hex(word(code, offset)))
        result["fixed_words"][hex(offset)] = hex(expected)
    for offset, prefix in edges.items():
        kind, target = refs[offset]
        assert kind == 6 and target.name.startswith(prefix), (name, hex(offset), target.name)
        edge = {"target": target.name}
        if linked:
            pc = va + offset
            raw = image.word(pc)
            assert raw >> 26 == 18 and raw & 1
            destination = image.target(pc, raw)
            assert destination in {address for address, _ in entries[target.name]}, (name, hex(pc), hex(destination), target.name)
            edge.update(site_va=hex(pc), target_va=hex(destination), word=hex(raw))
        result["call_edges"][hex(offset)] = edge
    return result


def focused_objects():
    path = WAVE / "r157-native-preliminary/WHEEL_PHASE_CANDIDATE.json"
    candidate = load(path)
    wheel = {row["source"]: {**row, "object": row["object"]["path"], "sha256": row["object"]["sha256"]}
             for row in candidate["selected"]}
    frozen_path = WAVE / "r157-batch/TARGETED_OBJECT_BINDING.json"
    frozen = load(frozen_path)
    assert sha(frozen_path) == "3d5f7a12568d300b620ec0c13eadc8e874bf87ca3af85317edd4e1c67822c6ac"
    assert frozen["head"] == FROZEN and frozen["tree"] == TREE
    compile_report = frozen["compile_report"]
    assert sha(Path(compile_report["path"])) == compile_report["sha256"]
    exact = {}
    for row in frozen["results"]:
        obj = row["artifacts"]["object"]
        exact[row["source"]] = {"object": obj["path"], "sha256": obj["sha256"], "blob": row["git_blob"]}
    pins = {"wheel_path": str(path), "wheel_sha256": sha(path), "wheel_head": candidate["head"],
            "frozen_path": str(frozen_path), "frozen_sha256": sha(frozen_path), "frozen_head": FROZEN,
            "compile_report": compile_report}
    return wheel, exact, pins


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--link", type=Path)
    parser.add_argument("--seal-sha256")
    for kind in ("invocation", "result", "pe", "map", "xex"):
        parser.add_argument(f"--{kind}-sha256")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assert git("rev-parse", f"{HEAD}^{{tree}}").decode().strip() == TREE
    assert git("rev-parse", f"{FROZEN}^{{tree}}").decode().strip() == TREE
    wheel, exact, pins = focused_objects()
    checked = {name: bind(name, spec, exact) for name, spec in CHECKS.items()}
    for name, spec in CHECKS.items():
        assert checked[name]["coff_sha256"] == bind(name, spec, wheel)["coff_sha256"], name
    receipt = {"head": HEAD, "tree": TREE, "artist": check_artist(), "focused_candidate": pins}
    linked = None
    if args.link:
        seal_path = args.link / "seal.json"
        assert args.seal_sha256 and sha(seal_path) == args.seal_sha256
        seal = load(seal_path)
        invocation = load(args.link / "invocation.json")
        result = load(args.link / "result.json")
        for pin, filename in (("invocation", "invocation.json"), ("result", "result.json"),
                              ("pe", "native-game.pe"), ("map", "native-game.map"), ("xex", "native-game.exe")):
            expected = getattr(args, f"{pin}_sha256")
            if expected:
                assert sha(args.link / filename) == expected, filename
        assert seal["schema"] == "r157-native-link-seal-v1"
        link_head = seal["head"]
        link_tree = git("rev-parse", f"{link_head}^{{tree}}").decode().strip()
        assert link_tree == TREE, (link_head, link_tree, TREE)
        assert invocation["sealed_sha256"] == args.seal_sha256
        assert invocation["objects"] == seal["selected"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        objects = {row["source"]: row for row in invocation["objects"]}
        outputs = {Path(path).name: row for path, row in result["outputs"].items()}
        for name in ("native-game.pe", "native-game.map", "native-game.exe"):
            assert sha(args.link / name) == outputs[name]["sha256"]
        image = IMAGE(args.link / "native-game.pe", args.link / "native-game.map")
        entries = COFF["map_symbols"]((args.link / "native-game.map").read_bytes())
        root = Path(seal["source_root"])
        source_rows = {Path(row["path"]).relative_to(root).as_posix().lower(): row for row in seal["sources"]
                       if Path(row["path"]).is_relative_to(root)}
        linked = image, entries, source_rows, link_head
        receipt["strict_link"] = {
            "head": link_head, "tree": link_tree, "seal_sha256": args.seal_sha256,
            "invocation_sha256": sha(args.link / "invocation.json"),
            "result_sha256": sha(args.link / "result.json"), "outputs": outputs,
        }
    receipt["functions"] = checked
    if linked:
        receipt["functions"] = {name: bind(name, spec, objects, linked)
                                for name, spec in (CHECKS | STRICT_CHECKS).items()}
        for name, prior in checked.items():
            assert prior["coff_sha256"] == receipt["functions"][name]["coff_sha256"], name
        image, entries, _, _ = linked
        replay = receipt["functions"]["replay_registration_consumer"]
        vtable = "??_7ReplayModule@BrnReplays@@6B@"
        coff = COFF["Coff"](Path(replay["object"]).read_bytes())
        assert coff.vtable_slot(vtable, 0x48) == replay["symbol"]
        [address] = {address for address, module in entries[vtable]
                     if module.lower() == Path(replay["object"]).name.lower()}
        assert image.word(address + 0x48) == int(replay["linked_va"], 16)
        receipt["vtable_dispatch"] = {"symbol": vtable, "slot_va": hex(address + 0x48), "target": replay["linked_va"]}
    receipt["scope"] = "Static ARTIST and native PPC evidence. Both leaf codec edges retain the original skip guard."
    document = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == document, f"Receipt differs: {args.output}"
    else:
        with args.output.open("x", encoding="utf-8") as stream:
            stream.write(document)
    print("PASS", len(receipt["functions"]), "phase functions", sha(args.output))


if __name__ == "__main__":
    main()





