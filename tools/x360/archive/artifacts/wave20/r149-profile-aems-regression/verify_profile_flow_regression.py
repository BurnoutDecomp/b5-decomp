"""Verify R149 profile flow against the published R148 link."""
import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess


TREE = "341f7e7ee638d2dc398afb6075cb6e3045cdf466"
BASE = "bdbd812370b68b9d3fd1b3b4f27f6fa3d7632704"
R139 = "e53482ef4d51dde3448310835cbcbdedfa1b4e16"
SOURCE = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
SOURCE_BLOB = "6ed92b65cd88ac0acfa1576ef82da7cf2d121258"
DISK_SOURCE = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowCheckDiskSpace.cpp"
CONTROLLER = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowController.cpp"
GAME = "src/GameSource/Game/BrnGameModule.cpp"
GUI = "src/GameSource/Gui/BrnGuiModule.cpp"
BATCH = Path(__file__).resolve().parent
WAVE = BATCH.parent
BASE_LINK = WAVE / "r140-link"
PREFLIGHT = WAVE / "r141-composed-preflight"
PREFLIGHT_SHA = "7ac609376977dd84dd43c4e6ac3fa14532d2d21e868eb314c2d92f5bc2d2ff00"
R140_SCRIPT_SHA = "3b907cf93cbe2fe8270462cbac62f07837f7f38a1fb126210ffded626a237aa1"
R140_RECEIPT_SHA = "0db3e9d0be41b6375c7a67da94ae2886312b6953687b73b5d7912bc7a1110327"
FIRE_ASSERT = "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z"
CONSTRUCT = "?Construct@GameMainFlowController@BrnGameMainFlowController@@QAAXXZ"
SET_STATE = "?SetState@GameMainFlowController@BrnGameMainFlowController@@QAAXW4EMainGameFlowState@2@@Z"
SEND = "?SendEvent@GameMainFlowController@BrnGameMainFlowController@@QAAXW4EMainGameFlowEvent@2@@Z"
GAME_MAIN = "?GameMain@BrnGameModule@BrnGame@@AAA_NXZ"
UPDATE_SET = "?ConstructUpdateSetFromFsm@BrnGameModule@BrnGame@@QAAGXZ"
BRIDGE_GUI = "?BridgeGuiToGame@BrnGameModule@BrnGame@@QAAXPBUOutputBuffer@CgsGuiModuleIO@CgsGui@@@Z"
PREWORLD = ("?DoUpdate_GameStatePreWorld@BrnGameModule@BrnGame@@QAAXPAUIOBufferStack@CgsModule@@0"
            "PBUOutputBuffer@InputIO@CgsInput@@PBU5BrnNetworkModuleIO@BrnNetwork@@PAU5GameStateModuleIO@BrnGameState@@G@Z")
PERFORM = "?PerformInvite@GuiModule@BrnGui@@AAAXPAUInputBuffer@CgsGuiModuleIO@CgsGui@@PAUOutputBuffer@45@@Z"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def row(rows, value, key="source"):
    item, = [item for item in rows if item[key] == value]
    return item


def state(name, method):
    return f"?{method}@MainGameFlowState{name}@@UAAXXZ"


def specs():
    flows = [state(name, method) for name in ("MemoryCard", "StartScreen", "CheckDiskSpace")
             for method in ("OnEnter", "OnLeave", "Update", "Render")
             if (name, method) != ("CheckDiskSpace", "Update")]
    return {
        SOURCE: flows + [state("InitialLoadingScreen", "FinishLoading")],
        DISK_SOURCE: [state("CheckDiskSpace", "Update")],
        CONTROLLER: [CONSTRUCT, SET_STATE, SEND],
        GAME: [GAME_MAIN, UPDATE_SET, BRIDGE_GUI, PREWORLD],
        GUI: [PERFORM],
    }


def git(root, *args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=root).decode()


def word(body, offset):
    return struct.unpack_from(">I", body, offset)[0]


def bss_anchor(h, blob, section):
    """Find the public anchor of BrnGameModule's shared BSS contribution."""
    _, _, _, table, count, _, _ = struct.unpack_from("<HHIIIHH", blob)
    anchor = "?suNativeValue82FAE280@BrnGame@@3EA"
    index = 0
    while index < count:
        name, value, target_section = h["object_symbol"](blob, index)
        if name == anchor:
            assert target_section == section and value == 4
            return anchor, value
        index += 1 + blob[table + index * 18 + 17]
    raise AssertionError(anchor)


def bind_code(h, fn, obj, symbol, address, linked):
    """Verify every instruction bit and resolve each address-bearing COFF relocation."""
    body, raw, refs = fn(h, obj, symbol)
    va = address(symbol)
    bound = linked(va, len(body))
    masks = {2: 0xFFFFFFFF, 6: 0x03FFFFFC, 16: 0xFFFF, 17: 0xFFFF, 18: 0}
    by_offset, highs, jump_targets = {}, {}, []
    blob = obj.read_bytes()
    for off, idx, kind in raw:
        by_offset[off] = by_offset.get(off, 0) | masks[kind]
        if kind == 18:
            continue
        name, value, section = h["object_symbol"](blob, idx)
        if name.startswith("$LN"):
            assert symbol in (SEND, BRIDGE_GUI) and section > 0 and 0 <= value < len(body)
            target = va + value
        elif name in ("sbSimUpdateComplete", "sbForceQuitToStartMenu"):
            assert symbol in (GAME_MAIN, BRIDGE_GUI) and kind in (16, 17)
            anchor, anchor_offset = bss_anchor(h, blob, section)
            target = address(anchor) + value - anchor_offset
        else:
            target = address(name)
        if kind == 6:
            assert h["branch_target"](va + off, word(bound, off)) == target, (symbol, name)
        elif kind == 16:
            highs[name] = off
        elif kind == 17:
            assert h["data_target"](bound, highs[name], off) == target, (symbol, name)
        elif kind == 2:
            assert symbol in (SEND, BRIDGE_GUI) and word(bound, off) == target, (symbol, name)
            jump_targets.append(value)
    for off in range(0, len(body), 4):
        assert not ((word(body, off) ^ word(bound, off)) & ~by_offset.get(off, 0)), (symbol, hex(off))
    if symbol == SEND:
        assert jump_targets == [0x8C, 0x8C, 0xA8, 0xC4, 0xE0, 0xFC, 0x118]
    if symbol == BRIDGE_GUI:
        assert len(jump_targets) == 72 and jump_targets[70 - 19] == 0x23C
    return bound, refs


def check_coff(h, fn, root, head):
    sha = h["sha"]
    manifest = read(WAVE / "r141-batch/MANIFEST.json")
    assert manifest["base"] == BASE and manifest["projected_tree"] == TREE
    assert row(manifest["final_paths"], SOURCE, "path")["new"].split()[-1] == SOURCE_BLOB
    receipt_path = PREFLIGHT / "R141_COMPOSED_COFF.json"
    report_path = PREFLIGHT / "compile-results.json"
    assert sha(receipt_path) == PREFLIGHT_SHA
    receipt, report = read(receipt_path), read(report_path)
    assert receipt["tree"] == TREE and receipt["head"] == manifest["candidate"]
    assert sha(report_path) == receipt["compile_report_sha256"]
    assert receipt["targeted_native"] == report["passed"] == len(report["results"]) == 7
    assert report["failed"] == 0
    preflight = {}
    for item in report["results"]:
        obj = Path(item["object"])
        assert item["passed"] and item["machine"] == 0x1F2
        assert sha(obj) == receipt["objects"][item["source"]]
        preflight[item["source"]] = obj

    base_seal = read(BASE_LINK / "seal.json")
    assert base_seal["head"] == BASE
    prior = read(BASE_LINK / "invocation.json")["objects"]
    base_objects = {}
    for path in specs():
        provider = row(prior, path)
        assert provider == row(base_seal["selected"], path)
        base_objects[path] = Path(provider["object"])
        assert sha(base_objects[path]) == provider["sha256"]

    old_src = git(root, "show", f"{BASE}:{SOURCE}").splitlines()
    new_src = git(root, "show", f"{head}:{SOURCE}").splitlines()
    changes = {
        state("CheckDiskSpace", "OnEnter"): (1224, 1209),
        state("StartScreen", "OnEnter"): (1123, 1108),
    }
    line_receipt = {}
    for symbol in specs()[SOURCE]:
        before, _, old_refs = fn(h, base_objects[SOURCE], symbol)
        after, _, new_refs = fn(h, preflight[SOURCE], symbol)
        assert old_refs == new_refs, symbol
        expected = bytearray(before)
        if symbol in changes:
            old_line, new_line = changes[symbol]
            for src, line in ((old_src, old_line), (new_src, new_line)):
                assert "CGS_ASSERT(gBrnScriptedLoadStage == E_LOADINGSTATESTAGE_START," in src[line - 2]
                assert src[line - 1].strip() == '"meLoadingStateStage == E_LOADINGSTATESTAGE_START");'
            assert (0x44, 6, FIRE_ASSERT) in old_refs
            assert word(before, 0x34) == 0x38A00000 | old_line
            assert word(after, 0x34) == 0x38A00000 | new_line
            struct.pack_into(">I", expected, 0x34, 0x38A00000 | new_line)
            line_receipt[symbol] = {"offset": "0x34", "old": old_line, "new": new_line}
        assert after == expected, symbol
    return base_objects, preflight[SOURCE], line_receipt


LINK_TREE = "bd200df2bf44260dee9d59f4cef5dd7dceb63c46"
LINK_BASE = "bc7ae6234fb6d1993128a6018212cc9cfd86e13a"
TARGET_MANIFEST_SHA = "5f11112152fddbb6291be08c4635e622ea3b3dd08acda4723f1329907e6ce9e3"
TARGET_PATCH_SHA = "cf8a9def3bdeee8cafb5b5a8ae50f57bf79daccf4a44f1dbe65c8a6abe2c40ee"
R148_LINK = WAVE / "r148-link"
R148_PROFILE_SHA = "b6ffa5f1a1dc4f057cc5096069408499ffdf476dc955636ce06656c94f7da8da"


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def target_manifest():
    path = WAVE / "r149-batch/MANIFEST.json"
    assert sha256(path) == TARGET_MANIFEST_SHA
    manifest = read(path)
    assert manifest["schema"] == "r149-native-intake-v1"
    assert manifest["base"] == LINK_BASE and manifest["projected_tree"] == LINK_TREE
    assert manifest["patch"]["sha256"] == TARGET_PATCH_SHA
    for key in ("patch", "base_compile", "base_link", "base_seal", "base_diagnostics",
                "generated_loading_contract", "base_loading_coff", "original_launch_trace"):
        pin = manifest[key]
        assert sha256(Path(pin["path"])) == pin["sha256"], key
    for group in manifest["groups"]:
        for pin in group["receipts"]:
            assert sha256(Path(pin["path"])) == pin["sha256"], pin["path"]
    return manifest


def target_pins(link, expected_head):
    manifest = target_manifest()
    seal_path, result_path = link / "seal.json", link / "result.json"
    seal, invocation, result = read(seal_path), read(link / "invocation.json"), read(result_path)
    assert seal["schema"] == "r149-native-link-seal-v1" and seal["head"] == expected_head
    provider = manifest["inherited_sdk_provider"]
    assert provider["sha256"] == "c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833"
    assert sha256(Path(provider["path"])) == provider["sha256"]
    assert invocation["sealed_sha256"] == sha256(seal_path)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    root = seal["source_root"]
    assert git(root, "rev-parse", "--verify", f"{expected_head}^{{tree}}").strip() == LINK_TREE
    assert git(root, "rev-parse", "--verify", f"{expected_head}^").strip() == LINK_BASE
    outputs = {Path(path).name: item["sha256"] for path, item in result["outputs"].items()}
    for name in ("native-game.exe", "native-game.pe", "native-game.map"):
        assert sha256(link / name) == outputs[name], name
    image = read(link / "image-verification.json")
    assert image["head"] == expected_head and image["magic"] == "XEX2"

    assert image["image_sha256"] == outputs["native-game.exe"]
    return manifest, seal, invocation, result, outputs


def verify(link, expected_head):
    prior_script = WAVE / "r140-batch/verify_profile_flow_regression.py"
    assert hashlib.sha256(prior_script.read_bytes()).hexdigest() == R140_SCRIPT_SHA
    r140 = runpy.run_path(str(prior_script))
    earlier_script = WAVE / "r139-batch/verify_profile_flow_link.py"
    assert hashlib.sha256(earlier_script.read_bytes()).hexdigest() == r140["R139_SCRIPT_SHA"]
    r139 = runpy.run_path(str(earlier_script))
    h, fn = r139["load_helpers"](), r139["function"]
    sha = h["sha"]
    base_receipt = BASE_LINK / "R140_PROFILE_FLOW_LINKED.json"
    assert sha(base_receipt) == R140_RECEIPT_SHA
    baseline = read(base_receipt)
    assert baseline["head"] == BASE and baseline["base"] == R139
    assert r140["check_link"](BASE_LINK, r139, h) == baseline

    manifest, seal, invocation, _, outputs = target_pins(link, expected_head)
    seal_path, result_path = link / "seal.json", link / "result.json"
    root = seal["source_root"]
    assert git(root, "rev-parse", "--verify", f"{expected_head}:{SOURCE}").strip() == SOURCE_BLOB
    retained_path = R148_LINK / "R148_PROFILE_FLOW_LINKED.json"
    assert sha(retained_path) == R148_PROFILE_SHA
    retained = read(retained_path)
    assert retained["head"] == LINK_BASE and retained["tree"] == manifest["base_tree"]
    assert retained["seal_sha256"] == manifest["base_seal"]["sha256"]
    assert retained["link_result_sha256"] == manifest["base_link"]["sha256"]
    for key, name in (("xex_sha256", "native-game.exe"), ("pe_sha256", "native-game.pe"),
                      ("map_sha256", "native-game.map")):
        assert retained[key] == sha(R148_LINK / name)
    retained_seal = read(R148_LINK / "seal.json")
    retained_invocation = read(R148_LINK / "invocation.json")
    assert retained_invocation["sealed_sha256"] == retained["seal_sha256"]

    base_objects, preflight, changed_lines = check_coff(h, fn, root, expected_head)
    old_address, old_linked = h["linked_reader"](BASE_LINK)
    address, linked = h["linked_reader"](link)
    retained_address, retained_linked = h["linked_reader"](R148_LINK)
    old_address, address = lru_cache(None)(old_address), lru_cache(None)(address)
    retained_address = lru_cache(None)(retained_address)
    functions, objects = {}, {}
    for path, symbols in specs().items():
        provider = row(invocation["objects"], path)
        assert provider == row(seal["selected"], path)
        official = Path(provider["object"])
        assert sha(official) == provider["sha256"]
        prior_provider = row(retained_invocation["objects"], path)
        assert prior_provider == row(retained_seal["selected"], path)
        prior_obj = Path(prior_provider["object"])
        assert sha(prior_obj) == prior_provider["sha256"] == retained["linked_objects_sha256"][path]
        objects[path] = provider["sha256"]
        for symbol in symbols:
            reference = preflight if path == SOURCE else base_objects[path]
            before, _, ref_relocs = fn(h, reference, symbol)
            after, _, after_relocs = fn(h, official, symbol)
            prior_code, _, prior_relocs = fn(h, prior_obj, symbol)
            assert after == before == prior_code and after_relocs == ref_relocs == prior_relocs, symbol
            bind_code(h, fn, base_objects[path], symbol, old_address, old_linked)
            bind_code(h, fn, prior_obj, symbol, retained_address, retained_linked)
            assert retained["functions"][symbol]["r148_va"] == hex(retained_address(symbol))
            bound, _ = bind_code(h, fn, official, symbol, address, linked)
            functions[symbol] = {"r140_va": hex(old_address(symbol)), "r148_va": hex(retained_address(symbol)),
                                 "r149_va": hex(address(symbol)), "bytes": len(bound)}

    save, video = (r139[key] for key in ("SAVE", "VIDEO"))
    assert linked(address(save), 8) == bytes.fromhex("98830054 4e800020")
    assert linked(address(video), 8) == bytes.fromhex("98830055 4e800020")
    for name in ("MemoryCard", "StartScreen", "CheckDiskSpace"):
        table = address(f"??_7MainGameFlowState{name}@@6B@")
        assert struct.unpack(">IIII", linked(table, 16)) == tuple(
            address(state(name, method)) for method in ("OnEnter", "OnLeave", "Update", "Render"))

    # These semantic locations are attested by the R140 regression and remain fixed in R149.
    card_enter = address(state("MemoryCard", "OnEnter"))
    card_update = address(state("MemoryCard", "Update"))
    assert word(linked(card_enter + 0x60, 4), 0) == 0x6108D838
    assert word(linked(card_enter + 0x78, 4), 0) == 0x6129D839
    assert word(linked(card_update + 0x2C, 4), 0) == 0x6169D838
    assert word(linked(card_update + 0x48, 4), 0) == 0x6129D839
    assert word(linked(card_update + 0x64, 4), 0) == 0x61290648
    assert word(linked(card_update + 0x88, 4), 0) == 0x616B0625

    return {
        "scope": "R149 linked profile-flow regression against frozen R140 and R141 witnesses",
        "head": expected_head, "tree": LINK_TREE, "base": LINK_BASE,
        "pe_sha256": outputs["native-game.pe"], "map_sha256": outputs["native-game.map"],
        "xex_sha256": outputs["native-game.exe"], "seal_sha256": sha(seal_path),
        "link_result_sha256": sha(result_path), "linked_objects_sha256": objects,
        "r140_receipt_sha256": R140_RECEIPT_SHA, "composed_preflight_sha256": PREFLIGHT_SHA,
        "r148_profile_receipt_sha256": R148_PROFILE_SHA, "target_manifest_sha256": TARGET_MANIFEST_SHA,
        "functions": functions, "assertion_line_immediates": changed_lines,
        "invite_offsets_from_gui_module": ["0x18D838", "0x18D839"],
        "gui_phase_complete_offset": "0x9A0648", "streaming_wait_offset": "0x9A0625",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link", type=Path)
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--output", type=Path, help="write a new receipt file instead of stdout")
    args = parser.parse_args()
    if not __debug__:
        parser.error("verification requires Python assertions")
    receipt = json.dumps(verify(args.link.resolve(), args.expected_head), indent=2) + "\n"
    if args.output is None:
        print(receipt, end="")
    else:
        with args.output.open("x", encoding="utf-8") as output:
            output.write(receipt)


if __name__ == "__main__":
    main()

