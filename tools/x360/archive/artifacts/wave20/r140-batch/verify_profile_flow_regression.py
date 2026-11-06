"""Verify that the R140 link preserves R139 profile flow with ARTIST invite offsets."""
import argparse
import json
from pathlib import Path
import runpy
import struct
import subprocess


TREE = "43d23d4258501ccd488869c7354a57f17b8bebf3"
BASE = "e53482ef4d51dde3448310835cbcbdedfa1b4e16"
BATCH = Path(__file__).resolve().parent
WAVE = BATCH.parent
BASE_LINK = WAVE / "r139-link"
COMPOSED = WAVE / "r140-composed-preflight"
R139_SCRIPT_SHA = "d78170e11c58ebe6ba40d73b17f1e737b28b4cdf35000eaed6c8ac233aee8fe8"
R139_RECEIPT_SHA = "0947a7ffaf1d928802c77fa62cfe52afa1f421f8d0ae5998a4bafcd4b772928b"
COMPOSED_SHA = "5fc57b112dfbc639ab2c33c23c55221f0aa8295f99ab1966a9e30d23b13760d8"
COMPOSED_REPORT_SHA = "8b1c2dc6b3ae724be4f3e4dde27213f203ffe4a3634019606500ec14c5d2b70c"
PROFILE_HOST = "src/GameSource/Gui/BrnGuiProfileHost.h"
PROFILE_HOST_BLOB = "69f5d2c4ba3720f2b03dacfb237fb60250f50d87"
GUI_MODULE = "src/GameSource/Gui/BrnGuiModule.cpp"
GAME_MODULE = "src/GameSource/Game/BrnGameModule.cpp"
START_UPDATE = "?Update@MainGameFlowStateStartScreen@@UAAXXZ"
PERFORM_INVITE = "?PerformInvite@GuiModule@BrnGui@@AAAXPAUInputBuffer@CgsGuiModuleIO@CgsGui@@PAUOutputBuffer@45@@Z"
BRIDGE_GUI = "?BridgeGuiToGame@BrnGameModule@BrnGame@@QAAXPBUOutputBuffer@CgsGuiModuleIO@CgsGui@@@Z"
PREWORLD = ("?DoUpdate_GameStatePreWorld@BrnGameModule@BrnGame@@QAAXPAUIOBufferStack@CgsModule@@0"
            "PBUOutputBuffer@InputIO@CgsInput@@PBU5BrnNetworkModuleIO@BrnNetwork@@PAU5GameStateModuleIO@BrnGameState@@G@Z")


def words(body, offset, expected):
    data = bytes.fromhex(expected)
    assert body[offset:offset + len(data)] == data, hex(offset)


def check_coff(r139, helpers):
    sha = helpers["sha"]
    read, row, function = (r139[name] for name in ("read", "artifact_row", "function"))
    reference_path = COMPOSED / "R140_COMPOSED_COFF.json"
    report_path = COMPOSED / "compile-results.json"
    assert sha(reference_path) == COMPOSED_SHA and sha(report_path) == COMPOSED_REPORT_SHA
    reference = read(reference_path)
    assert reference["tree"] == TREE
    assert reference["gui_phase_offset"] == "0x9A0648" and reference["stream_wait_offset"] == "0x9A0625"
    assert reference["invite_getter_offsets"] == {
        "?IsPerformingInvite@GuiModule@BrnGui@@QBA_NXZ": "0x18d838",
        "?IsLoginNeededForInvite@GuiModule@BrnGui@@QBA_NXZ": "0x18d839",
    }
    source = r139["SOURCE"]
    report = read(report_path)
    objects = {}
    for path in (source, GUI_MODULE, GAME_MODULE):
        item = row(report["results"], path, "source")
        assert item["passed"] and item["machine"] == 0x1F2
        obj = Path(item["object"])
        assert sha(obj) == reference["object_sha256"][Path(path).stem]
        objects[path] = obj

    base_receipt = BASE_LINK / "R139_PROFILE_FLOW_LINKED.json"
    assert sha(base_receipt) == R139_RECEIPT_SHA
    baseline = read(base_receipt)
    assert baseline["head"] == BASE and r139["verify"](BASE_LINK) == baseline
    prior = row(read(BASE_LINK / "invocation.json")["objects"], source, "source")
    prior_obj = Path(prior["object"])
    assert sha(prior_obj) == baseline["linked_object_sha256"] == prior["sha256"]

    specs = {
        r139["CARD_ENTER"]: {0x60: ("6108d828", "6108d838"), 0x78: ("6129d829", "6129d839")},
        r139["CARD_LEAVE"]: {},
        r139["DISK_ENTER"]: {},
        r139["CARD_UPDATE"]: {0x2C: ("6169d828", "6169d838"), 0x48: ("6129d829", "6129d839")},
        START_UPDATE: {0x1C: ("614a8248", "614a8258")},
    }
    for symbol, replacements in specs.items():
        before, _, old_refs = function(helpers, prior_obj, symbol)
        after, _, new_refs = function(helpers, objects[source], symbol)
        assert len(before) == len(after) and old_refs == new_refs, symbol
        expected = bytearray(before)
        for off, (old_word, new_word) in replacements.items():
            words(before, off, old_word)
            expected[off:off + 4] = bytes.fromhex(new_word)
        assert after == expected, symbol

    raw = helpers["original"]
    words(raw(0x823F2D90, 0x18), 0, "3d600087 616a8258 817d02d0 7d4b50ae 2b0a0000 409a0048")
    artist_invite = raw(0x825113F8, 0x28)
    words(artist_invite, 0, "3d7f0019 396bd838 894b0000 2b0a0000 409a00a8 "
          "3d400018 3ba00001 614ad839 9bab0000 7d5f50ae")
    producer, _, _ = function(helpers, objects[GUI_MODULE], PERFORM_INVITE)
    assert producer[0x10:0x38] == artist_invite
    update, _, _ = function(helpers, objects[source], r139["CARD_UPDATE"])
    words(update, 0x60, "3d20009a 61290648 7d2a48ae 2b090000")
    words(update, 0x80, "3d60009a 39200001 616b0625 3c6a009a 38800002 38630664 7d2a59ae")
    return objects, specs


def check_link(link, r139, helpers):
    read, row, function = (r139[name] for name in ("read", "artifact_row", "function"))
    sha = helpers["sha"]
    source, source_blob = r139["SOURCE"], r139["SOURCE_BLOB"]
    manifest = read(BATCH / "MANIFEST.json")
    assert manifest["base"] == BASE and manifest["projected_tree"] == TREE
    assert row(manifest["final_paths"], PROFILE_HOST, "path")["new"].split()[-1] == PROFILE_HOST_BLOB
    seal_path = link / "seal.json"
    result_path = link / "result.json"
    seal, invocation, result = read(seal_path), read(link / "invocation.json"), read(result_path)
    assert seal["schema"] == "r140-native-link-seal-v1"
    assert invocation["sealed_sha256"] == sha(seal_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    head = seal["head"]

    def git(*args):
        return subprocess.check_output(["git", "--no-optional-locks", *args],
                                       cwd=seal["source_root"]).strip().decode()

    assert git("rev-parse", "--verify", f"{head}^{{tree}}") == TREE
    assert git("rev-parse", "--verify", f"{head}^") == BASE
    assert git("rev-parse", "--verify", f"{head}:{source}") == source_blob
    assert git("rev-parse", "--verify", f"{head}:{PROFILE_HOST}") == PROFILE_HOST_BLOB
    outputs = {Path(path).name: entry["sha256"] for path, entry in result["outputs"].items()}
    for name in ("native-game.exe", "native-game.pe", "native-game.map"):
        assert sha(link / name) == outputs[name]
    image = read(link / "image-verification.json")
    assert image["head"] == head and image["magic"] == "XEX2" and image["image_sha256"] == outputs["native-game.exe"]

    composed, specs = check_coff(r139, helpers)
    providers = {}
    for path in composed:
        provider = row(invocation["objects"], path, "source")
        assert provider == row(seal["selected"], path, "source")
        official = Path(provider["object"])
        assert sha(official) == provider["sha256"]
        providers[path] = official

    address, linked = helpers["linked_reader"](link)
    functions = {}
    for path, symbols in ((source, list(specs)), (GUI_MODULE, [PERFORM_INVITE])):
        for symbol in symbols:
            code, _, expected_refs = function(helpers, composed[path], symbol)
            official, relocs, refs = function(helpers, providers[path], symbol)
            assert code == official and refs == expected_refs, symbol
            va = address(symbol)
            bound = linked(va, len(official))
            helpers["fixed_equal"](official, bound, relocs)
            highs = {}
            for off, kind, name in refs:
                if kind == 6:
                    (word,) = struct.unpack_from(">I", bound, off)
                    assert helpers["branch_target"](va + off, word) == address(name), (symbol, name)
                elif kind == 16:
                    highs[name] = off
                elif kind == 17:
                    assert helpers["data_target"](bound, highs[name], off) == address(name), (symbol, name)
                else:
                    raise AssertionError((symbol, hex(off), kind))
            functions[symbol] = {"va": hex(va), "bytes": len(bound),
                                 "offset_changes_from_r139": [hex(off) for off in specs.get(symbol, {})]}

    assert linked(address(r139["SAVE"]), 8) == bytes.fromhex("98830054 4e800020")
    assert linked(address(r139["VIDEO"]), 8) == bytes.fromhex("98830055 4e800020")
    table = address("??_7MainGameFlowStateMemoryCard@@6B@")
    assert struct.unpack(">III", linked(table, 12)) == tuple(
        address(r139[name]) for name in ("CARD_ENTER", "CARD_LEAVE", "CARD_UPDATE"))
    disk_table = address("??_7MainGameFlowStateCheckDiskSpace@@6B@")
    assert struct.unpack(">I", linked(disk_table, 4))[0] == address(r139["DISK_ENTER"])

    # The independently linked producer and consumer still use the same outer game bytes.
    bridge, _, _ = function(helpers, composed[GAME_MODULE], BRIDGE_GUI)
    phase = "3d60009a 616b0648 7f7f59ae"
    words(bridge, 0x23C, phase)
    phase_va = address(BRIDGE_GUI) + 0x23C
    words(linked(phase_va, 12), 0, phase)
    preworld, _, refs = function(helpers, composed[GAME_MODULE], PREWORLD)
    consume = "3d7f009a 396b0625 894b0000 2b0a0000 419a0020 7f83e378 9bab0000 3bc10050"
    words(preworld, 0x2F0, consume)
    wait_va = address(PREWORLD) + 0x2F0
    words(linked(wait_va, 32), 0, consume)
    words(linked(wait_va + 0x24, 8), 0, "7fc4f378 38a00008")
    for off, kind, name in refs:
        if off in (0x310, 0x31C):
            assert kind == 6
            call = address(PREWORLD) + off
            (word,) = struct.unpack(">I", linked(call, 4))
            assert helpers["branch_target"](call, word) == address(name), name

    return {
        "scope": "composed R140 linked profile-flow regression",
        "head": head, "tree": TREE, "base": BASE,
        "xex_sha256": outputs["native-game.exe"], "pe_sha256": outputs["native-game.pe"],
        "map_sha256": outputs["native-game.map"], "link_result_sha256": sha(result_path),
        "seal_sha256": sha(seal_path), "r139_profile_receipt_sha256": R139_RECEIPT_SHA,
        "composed_coff_receipt_sha256": COMPOSED_SHA,
        "linked_objects_sha256": {path: sha(obj) for path, obj in providers.items()},
        "functions": functions,
        "invite_offsets_from_gui_module": ["0x18D838", "0x18D839"],
        "gui_phase_complete_offset": "0x9A0648", "gui_event_70_store_va": hex(phase_va + 8),
        "streaming_wait_offset": "0x9A0625", "streaming_wait_consumer_load_va": hex(wait_va + 8),
        "memory_card_vtable": hex(table), "check_disk_vtable": hex(disk_table),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path, help="directory with the completed strict R140 link")
    parser.add_argument("--output", type=Path, help="write a new receipt file instead of stdout")
    args = parser.parse_args()
    if not __debug__:
        parser.error("verification requires Python assertions")
    r139_script = WAVE / "r139-batch/verify_profile_flow_link.py"
    import hashlib
    assert hashlib.sha256(r139_script.read_bytes()).hexdigest() == R139_SCRIPT_SHA
    r139 = runpy.run_path(str(r139_script))
    receipt = json.dumps(check_link(args.link.resolve(), r139, r139["load_helpers"]()), indent=2) + "\n"
    if args.output is None:
        print(receipt, end="")
    else:
        with args.output.open("x", encoding="utf-8") as output:
            output.write(receipt)


if __name__ == "__main__":
    main()

