"""Bind the R156 articulated-physics paths to selected PPC and strict-link output."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys

if not __debug__:
    raise RuntimeError("Run verification with Python assertions enabled")

HERE = Path(__file__).resolve().parent
WAVE = HERE.parent
CANDIDATE = "b521454a05f57df74a009c39c89c47af55877d4d"
TREE = "74cc11db3f78b4ded89afcfab097c96ed2b3fc49"
V2_TREE = "f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
R155 = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
CHECKOUT = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
FROZEN = CHECKOUT.with_name("native-sound")
PHYSICS = "src/GameSource/Physics/"
VEHICLE = PHYSICS + "VehicleManager/"
CREATE = VEHICLE + "BrnPhysicalTrafficManager_Create.cpp"
MANAGER = VEHICLE + "BrnPhysicalTrafficManager.cpp"
REMOVE = VEHICLE + "BrnPhysicalTrafficManager_Remove.cpp"
POOL = VEHICLE + "VehiclePhysics/BrnArticulatedJointPool.cpp"
EVENT = VEHICLE + "SharedIO/CreateArticulatedTrafficEvent.cpp"
MAINTENANCE = VEHICLE + "BrnVehicleManager_MaintenanceEvents.cpp"
POST = VEHICLE + "BrnVehicleManager_UpdateVehiclePhysics.cpp"

EVIDENCE = {
    "r156-composed-preflight/compile-results.json": "069b865d33a925592261bf89f55786dd83975d4b9922443d404defacf627f496",
    "r156-articulated-hitch-proof/R156_ARTICULATED_HITCH_COFF.json": "702b36d6e05f1c494e2e653745ce2d2ef96b4bd6c4e6f386f10b309108574c71",
    "r156-articulated-resolve-proof/R156_ARTICULATED_RESOLVE_COFF.json": "aae29454e4106d79c43f47f9ebc61afe99d375e92631dcd4dce90985cf856eeb",
    "r156-articulated-runtime-boundary/R156_RECYCLE_BOUNDARY.json": "751194eb09f77f8d54c20d7a64ef602623c91eb5e74e538affb571d2ebf8eb1d",
}
PREFLIGHT_OBJECTS = {
    CREATE: "9062c14b497b915e445cdc8634fd0441fab80dedb6249f582d121a7f1c7f65c3",
    REMOVE: "c5a615d21e18ae0f95f4660a8008c56acc1c0a71bcc601d1c2f65b150c2fface",
    MANAGER: "116d124a139ca6bd1962d78d696bcc709a93586cea3c6471eb3c4df5feea709a",
    EVENT: "e5d886ac9f1cbf8c69308dd987a17278b5c29c79731df491e4e729051a9054ed",
    POOL: "a3c4e23e1d634af46fd3609d3a868d0e33f6855bf465339b22bb939aefd700cf",
}
BASELINE_PINS = {
    "seal.json": "a969af86b6beeb39371dfbb536a4ba7eae109d541e5fef8868f96fdd5e27a28f",
    "native-game.exe": "dc82daf3cb18c67831c2ba76b2e0fcbc589a16d8584c9418e7a6400d61558406",
    "native-game.pe": "c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510",
    "native-game.map": "62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a",
}

HELPER = WAVE / "r128-batch/verify_matches.py"
PPC = {"__file__": str(HELPER)}
exec(HELPER.read_text(encoding="utf-8").split("\nif len(sys.argv)==2:", 1)[0], PPC)
sha = PPC["sha"]
original = PPC["original"]


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def git(root, *args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(root), *args])


def ref(root, name):
    return git(root, "rev-parse", name).decode().strip()


def word(code, offset=0):
    return struct.unpack_from(">I", code, offset)[0]


def words_at(read, address, words):
    expected = bytes.fromhex(words)
    assert read(address, len(expected)) == expected, hex(address)


def branch(pc, instruction):
    assert instruction & 0xfc000003 == 0x48000001, hex(pc)
    return PPC["branch_target"](pc, instruction)


def conditional_target(pc, instruction):
    assert instruction >> 26 == 16 and instruction & 3 == 0, hex(pc)
    delta = instruction & 0xfffc
    return pc + delta - (0x10000 if delta & 0x8000 else 0)


def index_unique(rows, key):
    index = {row[key]: row for row in rows}
    assert len(index) == len(rows), key
    return index


class LinkedImage:
    def __init__(self, folder, head, pins):
        self.folder = folder
        for name, digest in pins.items():
            assert sha(folder / name) == digest, name
        seal = load(folder / "seal.json")
        invocation = load(folder / "invocation.json")
        result = load(folder / "result.json")
        image = load(folder / "image-verification.json")
        self.root = Path(seal["source_root"])
        assert self.root == CHECKOUT
        assert seal["head"] == image["head"] == head
        assert invocation["sealed_sha256"] == pins["seal.json"]
        assert result["exit_code"] == 0 and result["all_post_checks_pass"]
        assert image["image_sha256"] == pins["native-game.exe"] and image["magic"] == "XEX2"
        with (folder / "native-game.exe").open("rb") as stream:
            assert stream.read(4) == b"XEX2"
        outputs = {Path(path): row for path, row in result["outputs"].items()}
        for name, digest in pins.items():
            if name != "seal.json":
                assert outputs[folder / name]["sha256"] == digest, name
        inventory_path = folder / "source-inventory.json"
        assert sha(inventory_path) == seal["source_inventory"]["sha256"]
        inventory = load(inventory_path)
        assert inventory["head"] == head
        self.sources = index_unique(inventory["files"], "path")
        self.rows = index_unique(invocation["objects"], "source")
        self.signed = index_unique(seal["selected"], "source")
        assert self.rows.keys() == self.signed.keys()
        self.read = PPC["linked_reader"](folder)[1]
        maptext = (folder / "native-game.map").read_text(encoding="utf-8", errors="replace")
        self.symbols = {}
        for match in re.finditer(
            r"(?m)^ +[0-9a-fA-F]{4}:[0-9a-fA-F]+ +(\S+) +([0-9a-fA-F]{8}) +(.*?)\s*$",
            maptext,
        ):
            symbol, address, detail = match.groups()
            if not detail.strip():
                continue
            self.symbols.setdefault(symbol, []).append((int(address, 16), detail.split()[-1]))
        self.object_sources = {}
        for source, row in self.rows.items():
            self.object_sources.setdefault(Path(row["object"]).name.lower(), []).append(source)
        self.selected = {}
        self.methods = {}

    def selected_object(self, source):
        if source not in self.selected:
            row = self.rows[source]
            signed = self.signed[source]
            obj = Path(row["object"])
            assert obj == Path(signed["object"]) and sha(obj) == row["sha256"] == signed["sha256"], source
            self.selected[source] = obj
        return self.selected[source]

    def mapped(self, symbol):
        entry, = self.symbols[symbol]
        return entry

    def method(self, prefix, expected_source=None):
        symbol, = [name for name in self.symbols if name.startswith(prefix)]
        if symbol not in self.methods:
            address, basename = self.mapped(symbol)
            source, = self.object_sources[basename.lower()]
            obj = self.selected_object(source)
            code, relocs, names = coff(obj, symbol)
            linked = self.read(address, len(code))
            PPC["fixed_equal"](code, linked, relocs)
            self.methods[symbol] = {
                "source": source, "object": obj, "symbol": symbol, "address": address,
                "coff": code, "linked": linked, "relocs": relocs, "names": names,
            }
        result = self.methods[symbol]
        if expected_source is not None:
            assert result["source"] == expected_source, (symbol, result["source"])
        return result

    def edge(self, method, target_prefix, offset=None, provider=True):
        if offset is None:
            offset, = [off for (off, kind), name in method["names"].items()
                       if kind == 6 and name.startswith(target_prefix)]
        target = method["names"][offset, 6]
        assert target.startswith(target_prefix), (method["symbol"], hex(offset), target)
        site = method["address"] + offset
        actual = branch(site, word(method["linked"], offset))
        mapped, object_name = self.mapped(target)
        assert actual == mapped, (target, hex(actual), hex(mapped))
        source = self.method(target)["source"] if provider else None
        return {"site": hex(site), "offset": hex(offset), "target": target,
                "target_va": hex(actual), "map_object": object_name, "target_source": source}

    def source_pin(self, source, head):
        path = self.root / source
        blob = git(CHECKOUT, "show", f"{head}:{source}")
        sealed = self.sources[str(path)]["sha256"]
        lf = blob.replace(b"\r\n", b"\n")
        accepted = {hashlib.sha256(value).hexdigest(): projection for projection, value in
                    (("checkout CRLF", lf.replace(b"\n", b"\r\n")), ("checkout LF", lf), ("raw blob", blob))}
        assert sealed in accepted, source
        return {"git_blob": ref(CHECKOUT, f"{head}:{source}"), "sha256": sealed,
                "inventory_projection": accepted[sealed]}


def coff(path, symbol):
    code, relocs = PPC["collect"](path, symbol)
    raw = path.read_bytes()
    return code, relocs, {(off, kind): PPC["object_symbol"](raw, idx)[0]
                          for off, idx, kind in relocs}


def compare_coff(method, obj):
    expected, relocs, names = coff(obj, method["symbol"])
    assert names.keys() == method["names"].keys(), (method["symbol"], "COFF relocation locations")
    other_indices = {(off, kind): idx for off, idx, kind in method["relocs"]}
    for offset, index, kind in relocs:
        name = names[offset, kind]
        other = method["names"][offset, kind]
        if name == other:
            continue
        if kind == 6 and name.startswith("?JointFrameInBodySpace@"):
            erase_checkout_hash = lambda value: re.sub(r"@\?A0x[0-9a-f]{8}@", "@?A0xcheckout@", value)
            assert erase_checkout_hash(name) == erase_checkout_hash(other), (name, other)
            continue
        assert kind in (0x10, 0x11) and name.startswith("??_C") and other.startswith("??_C"), (name, other)
        expected_literal = PPC["symbol_literal"](obj.read_bytes(), index)
        selected_literal = PPC["symbol_literal"](method["object"].read_bytes(), other_indices[offset, kind])
        assert expected_literal.startswith(str(FROZEN) + "\\src\\")
        assert selected_literal == str(CHECKOUT) + expected_literal[len(str(FROZEN)):], (name, other)
    PPC["fixed_equal"](expected, method["coff"], relocs)


def half_literal(image, method, upper, lower, symbol):
    name = method["names"][upper, 0x10]
    assert name == method["names"][lower, 0x11] == symbol
    address = PPC["data_target"](method["linked"], upper, lower)
    assert image.read(address, 4) == bytes.fromhex("3f000000")
    return hex(address)


def code_witnesses(method, sequences):
    for offset, words in sequences.items():
        words_at(lambda off, size: method["linked"][off:off + size], offset, words)
    return [hex(method["address"] + offset) for offset in sequences]


def artist_witnesses():
    resolve_check = runpy.run_path(str(WAVE / "r156-articulated-resolve-proof/verify_articulated_resolve.py"))
    resolve_check["check_artist"]()
    art_words = {
        0x82648f80: "15bd0470 151ef6f3 1800fe33 157ffef3 817f001c 396b0040 114058c3",
        0x82648fb8: "14006890 1400e313 100a000a 100059c3 817e001c 396b0040 100058c3 141b6cf0 100059c3",
        0x826490b0: "814100a4 7c6b1b78 806100d4 39010130 38e100f0 7fa6eb78 7f85e378 808a0000 102058c3",
        0x82649108: "93bf002c",
        0x8264913c: "93be002c",
        0x82f2a3a4: "ffffffff",
        0x8262dafc: "7d7cf82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020",
        0x8262db30: "7d7cf82e 556bb4be 7d6bfa14 7e8bf1ae",
        0x8262db84: "7d7df82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020",
        0x8262dbb8: "7d7bfa14 7d6bf0ae 7d6bfa14 7e8bf1ae",
        0x8261d3ec: "7d6ad02e 556bb4be 7d6bd214 7ceb41ae 3d6082f3 816ba3a4 7d6ad12e",
        0x82637790: "3beb003f 2f1f0014 4098000c 2f1fffff 409a0038",
        0x8263be4c: "3f7f0002", 0x8263be7c: "3b7b7dc0", 0x8263bf84: "3b7b0400",
        0x8263bf94: "3c7f0001 3863aee0",
    }
    for address, words in art_words.items():
        words_at(original, address, words)
    edges = {
        0x82649ad8: 0x826495e8, 0x82649b24: 0x82615b10, 0x82649758: 0x826487c8,
        0x82648a48: 0x825b3030, 0x82648a54: 0x825b3108, 0x82648a88: 0x82646e70, 0x82648be0: 0x82646e70,
        0x82601000: 0x825d7dd8, 0x82601010: 0x825c23b0,
        0x826014dc: 0x825c26f0, 0x826014e8: 0x825e7170, 0x82642c50: 0x826371d0,
        0x82648f44: 0x825f3b68, 0x82648f54: 0x825f3b68, 0x82648f60: 0x825c0220,
        0x82648f74: 0x825c0220, 0x826490ac: 0x825c0220, 0x826490d4: 0x826009c8,
        0x825d81b0: 0x8259b580, 0x825d81d4: 0x8259b728, 0x825d8204: 0x8259b8d0,
        0x82615b90: 0x826013c0, 0x8261ce14: 0x825d8490, 0x8261cf48: 0x825d8490,
        0x8261cfbc: 0x825d8248, 0x8262daf8: 0x8261cc98, 0x8262db80: 0x8261cc98,
        0x826377d4: 0x8262da10, 0x82647594: 0x82637608,
        0x825f0c40: 0x825b3418, 0x825f0f28: 0x825d8490, 0x825f1384: 0x825d8490,
        0x825f13b0: 0x825c0220, 0x825f13c4: 0x825c0220, 0x826375c0: 0x825f0a90, 0x826375c8: 0x825bd150,
    }
    for address, target in edges.items():
        assert branch(address, word(original(address, 4))) == target, hex(address)
    reader = PPC["append_check"]["coff"]["x360rd"]
    artist_image = Path(reader.RAW or reader.FLAT)
    return {"image": {"path": str(artist_image), "sha256": sha(artist_image)},
            "reader": {"path": reader.__file__, "sha256": sha(Path(reader.__file__))},
            "instruction_ranges": {hex(address): len(bytes.fromhex(words)) for address, words in art_words.items()},
            "direct_calls": {hex(address): hex(target) for address, target in edges.items()},
            "create": "0x826487c8", "resolve": "0x825f0a90",
            "create_translation_stores": ["0x82648fc4", "0x82648fd8"],
            "resolve_translation_stores": ["0x825f1438", "0x825f144c"]}


def check_physics(image, baseline, preflight):
    create = image.method("?ProcessCreateArticulatedTrafficEvents@", CREATE)
    resolve = image.method("?ResolveArticulatedJoints@", MANAGER)
    recycle = image.method("?RecycleTrafficVehicle@", REMOVE)
    remove = image.method("?RemoveTrafficVehicle@", REMOVE)
    free = image.method("?GetFreeTrafficVehicleWithPhysics@", CREATE)
    alloc = image.method("?CreateTrafficVehicle@", CREATE)
    construct = image.method("?ConstructArticulatedJoint@", POOL)
    create_joint = image.method("?CreateJoint@ArticulatedJointPool@", POOL)
    post = image.method("?UpdateTrafficPhysicsPostSimulation@", MANAGER)
    create_events = image.method("?ProcessCreateEvents@PhysicalTrafficManager@", CREATE)
    maintenance = image.method("?ProcessTrafficMaintenanceEvents@PhysicalTrafficManager@", MAINTENANCE)
    post_manager = image.method("?UpdateVehiclePhysicsPostSimulation@VehicleManager@", POST)
    bridge = image.method("?BridgeArticulatedJointRequestsToSim@", MANAGER)
    send = image.method("?SendCreateRemoveJointEvents@ArticulatedJointPool@", POOL)
    vehicle_construct = image.method("?Construct@VehicleManager@Vehicle@BrnPhysics@@QAAXXZ",
                                     VEHICLE + "BrnVehicleManager_Construct.cpp")
    for method in (create, resolve, recycle, remove, free, alloc, construct, create_joint, post, create_events, bridge, send):
        compare_coff(method, Path(preflight[method["source"]]["object"]))
    for method in (recycle, remove, post, maintenance, post_manager, bridge, send, vehicle_construct):
        old = baseline.method(method["symbol"], method["source"])
        compare_coff(method, old["object"])
    assert len(resolve["linked"]) == 0x430
    assert not any(name.startswith(("?Translate@", "?JointResolveWitness@")) for name in resolve["names"].values())
    assert b"[T-joint] resolved cab=" not in resolve["object"].read_bytes()
    assert any(name.startswith("?JointResolveWitness@")
               for name in baseline.method(resolve["symbol"], MANAGER)["names"].values())

    edges = []
    def check_edges(method, *specs):
        for offset, target in specs:
            edge = image.edge(method, target, offset)
            edges.append({"caller": method["symbol"], **edge})

    check_edges(maintenance, (None, "?ProcessCreateEvents@PhysicalTrafficManager@"),
                (None, "?BridgeArticulatedJointRequestsToSim@"))
    check_edges(create_events, (0x120, "?RemoveBrokenJointsFromSimulation@"),
                (0x170, "?ProcessCreateArticulatedTrafficEvents@"))
    check_edges(create, (0x1b4, "?GetEvent@?$BaseEventQueue@UCreateArticulatedTrafficEvent@"), (0x1c0, "?GetCreateCabEvent@"), (0x1cc, "?GetCreateTrailerEvent@"),
                (0x200, "?CreateTrafficVehicle@"), (0x270, "?CreateTrafficVehicle@"),
                (0x3e4, "?SetArticulated@"), (0x3f4, "?SetArticulated@"),
                (0x400, "?GetArticulationPointWorldSpace@"), (0x40c, "?GetArticulationPointWorldSpace@"),
                (0x484, "?GetPhysicsEntityId@PhysicalTrafficManager@"), (0x494, "?GetPhysicsEntityId@PhysicalTrafficManager@"),
                (0x520, "?GetArticulationPointWorldSpace@"), (0x544, "?CreateJoint@ArticulatedJointPool@"))
    for target in ("?GetCreateCabEvent@", "?GetCreateTrailerEvent@"):
        compare_coff(image.method(target, EVENT), Path(preflight[EVENT]["object"]))
    check_edges(create_joint, (0xf4, "?GetFirstZeroBit@"), (0x138, "?Set@ArticulatedJointId@"),
                (0x1cc, "?ConstructArticulatedJoint@"), (0x1dc, "?FlagJointToBeCreated@"))
    check_edges(construct, (0x368, "?JointFrameInBodySpace@"), (0x378, "?JointFrameInBodySpace@"),
                (0x3b8, "?SetChildAngularFrame@JointFrames@"),
                (0x3f4, "?SetParentAngularFrame@JointFrames@"), (0x424, "?SetParentLinearFrame@JointFrames@"))
    check_edges(bridge, (0x80, "?SendCreateRemoveJointEvents@ArticulatedJointPool@"))
    check_edges(send, (0x84, "?GetCreateJointEvent@"), (0x90, "?AddJoint@"))
    check_edges(post_manager, (None, "?UpdateTrafficPhysicsPostSimulation@"))
    check_edges(post, (0x134, "?ResolveArticulatedJoints@"), (0x13c, "?GetJointSpyQueue@"),
                (0x15c, "?GetEvent@?$BaseEventQueue@UOutJointSpy@"))
    check_edges(resolve, (0x1c0, "?GetIndexOfOtherHalf@"), (0x36c, "?GetIndexOfOtherHalf@"),
                (0x398, "?GetArticulationPointWorldSpace@"), (0x3a4, "?GetArticulationPointWorldSpace@"))
    check_edges(alloc, (None, "?GetFreeTrafficVehicleWithPhysics@"))
    check_edges(free, (0x13c, "?GetFirstClearBit@"), (0x160, "?GetLeastInterestingFullyPhysicalVehicle@"),
                (0x17c, "?RecycleTrafficVehicle@"))
    check_edges(recycle, (0xdc, "?RemoveTrafficVehicle@"), (0x160, "?RemoveTrafficVehicle@"),
                (0x190, "?FireAssert@Assert@CgsDev@@"))
    check_edges(remove, (0xec, "?GetIndexOfOtherHalf@"), (0x1b4, "?GetIndexOfOtherHalf@"),
                (0x224, "?RemoveJoint@ArticulatedJointPool@"))

    create_witness = code_witnesses(create, {
        0x114: "101f038c", 0x124: "1bc000dc",
        0x420: "817f001c 100d004a 396b0040 11a058c3 159f04b0 158cf313 11ad600a 11a059c3 "
               "817d001c 396b0040 11a058c3 15bf04f0 11a059c3",
        0x528: "80960000 390100c0 8061008c 38e10100 7f86e378 7fc5f378 102058c3",
        0x578: "93df002c", 0x5ac: "93dd002c",
    })
    resolve_witness = code_witnesses(resolve, {
        0x14: "3c790002 38639868 9061005c",
        0xfc: "817e0024 2f0b0001 409a0308",
        0x110: "546b063e 2b0b0000 419a02f4",
        0x1c4: "7c7f1b78 2b1f0014 4198001c",
        0x1e8: "57ebd1be 7bea06a0 396b330d 556b1838 7d2bc82a 39600001 7d6a5036 "
               "7d4a4838 2b2a0000 409a0008 39600000 556b063e 2b0b0000 419a01f0",
        0x370: "7f03a000 419a001c", 0x3b0: "101f038c",
        0x3cc: "10000184 118058c3 817e001c 11ac684a 118050c3 396b0040 116058c3 154c6890 "
               "100a04c4 100b000a 100059c3 817f001c 396b0040 100058c3 100c036e 100059c3",
    })
    for offset, expected in [(0x104, 0x40c), (0x118, 0x40c), (0x21c, 0x40c), (0x2c, 0x424), (0x420, 0xcc)]:
        assert conditional_target(offset, word(resolve["linked"], offset)) == expected
    for method, offset, expected in [(free, 0x148, 0x180), (recycle, 0x178, 0x198)]:
        assert conditional_target(offset, word(method["linked"], offset)) == expected

    vehicle_construct_witness = code_witnesses(vehicle_construct, {
        0x514: "3f9f0002 3fbf0001 3b7f1b24 3b9c7dc0",
        0x5b0: "3b9c0400", 0x5c0: "3c7f0001 3863aee0",
    })
    free_witness = code_witnesses(free, {0x130: "3fbe0002 3bbd9868 7fa3eb78",
                                           0x144: "2f1fffff 409a0038"})
    recycle_witness = code_witnesses(recycle, {
        0x70: "3ac0007f 615e98f0",
        0xe0: "7d7cf82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020",
        0x114: "7d7cf82e 556bb4be 7d6bfa14 7ecbf1ae",
        0x164: "7d7df82e 556bb4be 7d6bfa14 7d6bf0ae 2b0b007f 409a0020",
        0x198: "7d7bfa14 7d6bf0ae 7d6bfa14 7ecbf1ae",
    })
    assertion_name = recycle["names"][0x180, 0x10]
    assert recycle["names"][0x188, 0x11] == assertion_name
    assert assertion_name.startswith("??_C@_0GM@KHFHIMDD@mu8GlobalToPhysicalEntityIndexMa@")
    assertion_va = PPC["data_target"](recycle["linked"], 0x180, 0x188)
    assertion_text = (b"mu8GlobalToPhysicalEntityIndexMap[maTrafficEntityIDs[liIndexToRecycle]."
                      b"GetEntityIndex()] != KU8_INVALID_MAP\x00")
    assert image.read(assertion_va, len(assertion_text)) == assertion_text
    remove_witness = code_witnesses(remove, {
        0x1e0: "92bf0024 92bf0028 3bbd94bc 931f002c",
        0x228: "7ec5b378 92bc0024 56e4063e 92bc0028 7fc3f378 931c002c",
        0x428: "556b103a 614998f0 3900007f 38e0ffff 2b1b0014 7d4bf02e 554ab4be 7d4af214 7d0a49ae 7cebf12e",
    })
    return {
        "edges": edges,
        "translation_paths": {
            "create": {"instruction_ranges": create_witness, "half": half_literal(image, create, 0x110, 0x12c, "__real@3f000000"),
                       "cab_store": hex(create["address"] + 0x43c), "trailer_fused": hex(create["address"] + 0x44c),
                       "trailer_store": hex(create["address"] + 0x450),
                       "shared_joint_index_stores": [hex(create["address"] + offset) for offset in (0x578, 0x5ac)]},
            "post_simulation": {"instruction_ranges": resolve_witness, "half": half_literal(image, resolve, 0x34, 0x48,
                                                     "KF_ARTICULATED_SOLVE_PENETRATION_WEIGHT_FACTOR"),
                                "cab_store": hex(resolve["address"] + 0x3f4), "trailer_fused": hex(resolve["address"] + 0x404),
                                "trailer_store": hex(resolve["address"] + 0x408),
                                "next_slot": hex(resolve["address"] + 0x40c)},
        },
        "inherited_full_pool_boundary": {
            "free_slot_and_recycle_guards": free_witness,
            "vehicle_manager_member_placement": vehicle_construct_witness,
            "recycle_witnesses": recycle_witness, "remove_witnesses": remove_witness,
            "partner_map_store": hex(recycle["address"] + 0x120),
            "own_map_read": hex(recycle["address"] + 0x170),
            "own_map_store": hex(recycle["address"] + 0x1a4),
            "own_assert_call": hex(recycle["address"] + 0x190),
            "own_assertion_literal_va": hex(assertion_va),
            "own_assertion_literal": assertion_text[:-1].decode(),
            "id_invalidation_store": hex(remove["address"] + 0x44c),
        },
    }


def verify(args):
    assert args.tree in (TREE, V2_TREE) and ref(FROZEN, f"{CANDIDATE}^{{tree}}") == TREE
    assert ref(CHECKOUT, "HEAD") == args.head and ref(CHECKOUT, f"{args.head}^{{tree}}") == args.tree
    assert ref(CHECKOUT, f"{args.head}^") == R155
    frozen_delta = git(CHECKOUT, "diff", "--name-only", TREE, args.tree).decode().splitlines()
    if args.tree == V2_TREE:
        assert frozen_delta == ["src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp"]
    else:
        assert not frozen_delta
    assert not git(CHECKOUT, "status", "--porcelain=v1")
    evidence = {}
    for name, digest in EVIDENCE.items():
        path = WAVE / name
        assert sha(path) == digest, name
        evidence[name] = digest
    preflight_report = load(WAVE / next(iter(EVIDENCE)))
    assert (preflight_report["passed"], preflight_report["failed"]) == (23, 0)
    preflight = index_unique(preflight_report["results"], "source")
    assert all(row["passed"] and row["machine"] == 0x1f2 for row in preflight.values())
    for source, digest in PREFLIGHT_OBJECTS.items():
        assert sha(Path(preflight[source]["object"])) == digest, source
    boundary = load(WAVE / "r156-articulated-runtime-boundary/R156_RECYCLE_BOUNDARY.json")
    pins = dict(zip(BASELINE_PINS, [args.seal, args.xex, args.pe, args.map]))
    image = LinkedImage(args.link.resolve(), args.head, pins)
    baseline = LinkedImage(WAVE / "r155-link", R155, BASELINE_PINS)
    assert sha(baseline.selected_object(REMOVE)) == boundary["r155_selected_coff_sha256"]
    assert ref(CHECKOUT, f"{args.head}:{REMOVE}") == boundary["source_blob"] == ref(CHECKOUT, f"{R155}:{REMOVE}")
    artist = artist_witnesses()
    physics = check_physics(image, baseline, preflight)
    assert boundary["after_remove"]["map_index"] == ((0xffffffff >> 10) & 0x3fff) == 16383
    assert boundary["after_remove"]["manager_relative_byte"] == hex(0xaee0 + 0x198f0 + 16383)
    physics["inherited_full_pool_boundary"]["sentinel_mapping"] = boundary["after_remove"]
    physics["inherited_full_pool_boundary"]["reachability"] = boundary["reachability"]
    physics["inherited_full_pool_boundary"]["runtime_result"] = "unverified"
    tracked_sources = set(image.selected) | {PHYSICS + "PhysicsUtilities/ExternallySimulatedBody.h"}
    sources = {source: image.source_pin(source, args.head) for source in sorted(tracked_sources)}
    for source, pin in sources.items():
        assert pin["git_blob"] == ref(FROZEN, f"{CANDIDATE}:{source}"), source
    methods = []
    for entry in image.methods.values():
        obj = entry["object"]
        methods.append({"symbol": entry["symbol"], "source": entry["source"], "object": str(obj),
                        "object_sha256": sha(obj), "va": hex(entry["address"]),
                        "bytes_matched_outside_relocation_fields": len(entry["coff"])})
    return {
        "state": "pass", "head": args.head, "frozen_candidate": CANDIDATE, "tree": args.tree,
        "frozen_candidate_tree": TREE, "frozen_candidate_path_delta": frozen_delta,
        "pins": pins, "baseline_pins": BASELINE_PINS, "evidence": evidence,
        "preflight_objects": PREFLIGHT_OBJECTS,
        "source_inventory_sha256": sha(image.folder / "source-inventory.json"),
        "invocation_sha256": sha(image.folder / "invocation.json"),
        "sources": sources, "artist": artist, "methods": methods, **physics,
        "scope": "Static selected COFF and linked PPC. No gameplay or pool-exhaustion execution evidence.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path)
    for pin in ("head", "tree", "seal", "xex", "pe", "map"):
        parser.add_argument("--" + pin, required=True)
    args = parser.parse_args()
    receipt = verify(args)
    output = HERE / "R156_ARTICULATED_PHYSICS_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output))
