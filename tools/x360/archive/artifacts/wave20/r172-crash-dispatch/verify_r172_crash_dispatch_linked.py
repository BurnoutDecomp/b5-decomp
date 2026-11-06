"""Bind the R172 crash dispatch, handler, and OnCrash destinations to a sealed PE."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

WAVE = Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path(r"C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANDIDATE = "34cbedc5316a1ee0592385ccb02c76cf06927365"
CANDIDATE_TREE = "27d9d594a2eea753b0057137f4898cbc1d3c3900"
BASE = "b7b549c7f423d38c8ddc63ba089c11152e66d8fd"
EXPECTED_HEAD = "b44620b618ed440be3fcb3770002131be370f198"
PREFLIGHT_SHA = "cd2a143692b0714554ed5428fa9f6570932e92eb2ea7f7bf7c35e9e9b10ade36"
POST_SOURCE = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
HANDLER_SOURCE = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_CrashExit.cpp"
ACTIVE_SOURCE = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.cpp"
LOCK_READ = "?LockForRead@IOBuffer@CgsModule@@QBAXXZ"
LOCK_WRITE = "?LockForWrite@IOBuffer@CgsModule@@QAAXXZ"
ON_CRASH = "?OnCrash@ActiveRaceCar@BrnWorld@@QAAX_NABURaceCarCrashEvent@Vehicle@BrnPhysics@@PAUInSceneUpdateInterface@SceneManagerIO@CgsSceneManager@@0M@Z"

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read(path):
    return json.loads(Path(path).read_bytes())

def git(*args):
    return subprocess.check_output(
        ["git", "--no-optional-locks", *args], cwd=OFFICIAL, text=True
    ).strip()

def main():
    if not __debug__:
        raise RuntimeError("verification requires assertions")
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-pins", required=True, type=Path)
    parser.add_argument("--root-pins-sha", required=True)
    args = parser.parse_args()

    pin_path = args.root_pins.resolve()
    link = pin_path.parent
    assert pin_path.name == "ROOT_LINK_PINS.json"
    assert sha(pin_path) == args.root_pins_sha
    pins = read(pin_path)
    head, tree, parent = pins["head"], pins["tree"], pins["parent"]
    assert (head, tree, parent) == (EXPECTED_HEAD, CANDIDATE_TREE, BASE)
    assert git("rev-parse", head, head + "^{tree}", head + "^").splitlines() == [
        head, tree, parent
    ]
    assert git("rev-parse", CANDIDATE + "^{tree}", CANDIDATE + "^").splitlines() == [
        CANDIDATE_TREE, BASE
    ]
    for item in pins["pins"].values():
        assert sha(item["path"]) == item["sha256"], item["path"]
    result = read(pins["pins"]["result"]["path"])
    diagnostics = read(pins["pins"]["diagnostics"]["path"])
    image_report = read(pins["pins"]["image"]["path"])
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert not diagnostics["unique_symbols"] and not diagnostics["duplicate_messages"]
    assert image_report["head"] == head
    assert image_report["image_sha256"] == pins["pins"]["xex"]["sha256"]
    seal = read(pins["pins"]["seal"]["path"])
    invocation = read(pins["pins"]["invocation"]["path"])
    assert seal["head"] == head
    assert invocation["sealed_sha256"] == pins["pins"]["seal"]["sha256"]

    preflight_path = WAVE / "r172-batch/NATIVE_PPC_PREFLIGHT.json"
    assert sha(preflight_path) == PREFLIGHT_SHA
    proof = read(preflight_path)
    assert (proof["status"], proof["head"], proof["tree"], proof["base"]) == (
        "pass", CANDIDATE, CANDIDATE_TREE, BASE
    )
    helpers = runpy.run_path(str(WAVE / "r156-layout-linked/verify_layout_linked.py"))["m"]
    linked_address, linked_bytes = helpers["linked_reader"](link)
    mapping = Path(pins["pins"]["map"]["path"]).read_text(errors="replace")

    def selected_body(section):
        source, symbol = section["source"], section["symbol"]
        preflight_obj = Path(section["object"]["path"])
        assert sha(preflight_obj) == section["object"]["sha256"]
        preflight_code, _ = helpers["collect"](preflight_obj, symbol)
        assert len(preflight_code) == section["body_bytes"]
        assert hashlib.sha256(preflight_code).hexdigest() == section["body_sha256"]
        selected, = [row for row in seal["selected"] if row["source"] == source]
        invoked, = [row for row in invocation["objects"] if row["source"] == source]
        obj = Path(selected["object"])
        assert obj == Path(invoked["object"])
        assert sha(obj) == selected["sha256"] == invoked["sha256"]
        assert git("rev-parse", head + ":" + source) == git(
            "rev-parse", CANDIDATE + ":" + source
        )
        pattern = (
            rf"(?m)^\s*[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(symbol)}"
            rf"\s+([0-9a-fA-F]{{8}})\s+f(?:\s+i)?\s+(\S+)"
        )
        (address, owner), = re.findall(pattern, mapping)
        va = int(address, 16)
        assert va == linked_address(symbol) and obj.name == owner
        code, rels = helpers["collect"](obj, symbol)
        assert len(code) == section["body_bytes"]
        assert hashlib.sha256(code).hexdigest() == section["body_sha256"]
        image = linked_bytes(va, len(code))
        helpers["fixed_equal"](code, image, rels)
        return obj, va, code, rels, image

    def call(obj, va, rels, image, at, expected_symbol, require_link=True):
        (_, index, kind), = [row for row in rels if row[0] == at and row[2] == 6]
        assert kind == 6
        data = obj.read_bytes()
        symbol = helpers["object_symbol"](data, index)[0]
        assert symbol == expected_symbol
        word = int.from_bytes(image[at:at+4], "big")
        target = helpers["branch_target"](va + at, word)
        assert (word & 1 or not require_link) and target == linked_address(symbol)
        return {"offset": hex(at), "symbol": symbol, "target": hex(target), "link": bool(word & 1)}

    post = proof["post_physics"]
    assert post["source"] == POST_SOURCE
    post_obj, post_va, post_code, post_rels, post_image = selected_body(post)
    expected_post_calls = [
        (0x94, LOCK_READ),
        (0x9c, LOCK_WRITE),
        *((int(row["offset"], 16), row["symbol"]) for row in post["calls"]),
    ]
    post_edges = [
        call(post_obj, post_va, post_rels, post_image, offset, symbol)
        for offset, symbol in expected_post_calls
    ]
    assert [offset for offset, _ in expected_post_calls] == [
        0x94, 0x9c, 0xb0, 0xc0, 0xd0
    ]
    assert [symbol for _, symbol in expected_post_calls[2:]] == [
        "?CopyActiveRaceCarToPlayerScoringMappingToOutput@RaceCarEntityModule@BrnWorld@@QAAXPAURCEntityActiveRaceCarOutputInterface@RaceCarEntityModuleIO@2@@Z",
        proof["crash_handler"]["symbol"],
        "?ProcessCreateVehicleEvents@RaceCarEntityModule@BrnWorld@@QAAXPBUInputBuffer_PostPhysics@RaceCarEntityModuleIO@2@PAUOutputBuffer_PostPhysics@42@@Z",
    ]
    argument_words = {
        0x14: "7c902378",  # r16 <- r4 (lpInput)
        0x1c: "7c741b78",  # r20 <- r3 (this)
        0x20: "7cb12b78",  # r17 <- r5 (lpOutput)
        0xb4: "7e258b78",  # r5 <- lpOutput
        0xb8: "7e048378",  # r4 <- lpInput
        0xbc: "7e83a378",  # r3 <- this
    }
    for offset, word in argument_words.items():
        at = int(offset) if isinstance(offset, str) else offset
        expected = bytes.fromhex(word)
        assert post_code[at:at+4] == post_image[at:at+4] == expected
    artist = read(post["original_export"]["path"])
    assert sha(post["original_export"]["path"]) == post["original_export"]["sha256"]
    for needle in (
        "0x823075D8  bl        BrnWorld__RaceCarEntityModule__CopyActiveRaceCarToPlayerScoringMappingToOutput",
        "0x823075E8  bl        BrnWorld__RaceCarEntityModule__ProcessRaceCarCrashEvents_PostPhysics",
        "0x823075F8  bl        BrnWorld__RaceCarEntityModule__ProcessCreateVehicleEvents",
    ):
        assert needle in artist["assembly"], needle

    handler = proof["crash_handler"]
    assert handler["source"] == HANDLER_SOURCE
    handler_obj, handler_va, handler_code, handler_rels, handler_image = selected_body(handler)
    field_words = {0x1a4: "7d5ca92a", 0x1c8: "99ff0782"}
    for offset, word in field_words.items():
        at = int(offset)
        expected = bytes.fromhex(word)
        assert handler_code[at:at+4] == handler_image[at:at+4] == expected
    crash_edge = call(
        handler_obj, handler_va, handler_rels, handler_image,
        int(handler["call"]["offset"], 16), ON_CRASH
    )

    active = proof["active_crash"]
    assert active["source"] == ACTIVE_SOURCE and active["symbol"] == ON_CRASH
    active_obj, active_va, active_code, active_rels, active_image = selected_body(active)
    active_edges = []
    for offset, index, kind in active_rels:
        if kind == 6:
            symbol = helpers["object_symbol"](active_obj.read_bytes(), index)[0]
            active_edges.append(call(active_obj, active_va, active_rels, active_image, offset, symbol, False))

    handler_edges = []
    for offset, index, kind in handler_rels:
        if kind == 6:
            symbol = helpers["object_symbol"](handler_obj.read_bytes(), index)[0]
            handler_edges.append(call(handler_obj, handler_va, handler_rels, handler_image, offset, symbol, False))

    receipt = {
        "status": "pass",
        "head": head,
        "tree": tree,
        "parent": parent,
        "root_pins_sha256": args.root_pins_sha,
        "pe_sha256": pins["pins"]["pe"]["sha256"],
        "xex_sha256": pins["pins"]["xex"]["sha256"],
        "preflight_sha256": PREFLIGHT_SHA,
        "post_physics": {
            "symbol": post["symbol"],
            "address": hex(post_va),
            "bytes": len(post_code),
            "body_sha256": post["body_sha256"],
            "selected_object_sha256": sha(post_obj),
            "calls": post_edges,
            "argument_words": {hex(k): v for k, v in argument_words.items()},
        },
        "crash_handler": {
            "symbol": handler["symbol"],
            "address": hex(handler_va),
            "bytes": len(handler_code),
            "body_sha256": handler["body_sha256"],
            "selected_object_sha256": sha(handler_obj),
            "field_words": {hex(k): v for k, v in field_words.items()},
            "on_crash_call": crash_edge,
            "resolved_direct_calls": handler_edges,
        },
        "on_crash": {
            "symbol": active["symbol"],
            "address": hex(active_va),
            "bytes": len(active_code),
            "body_sha256": active["body_sha256"],
            "selected_object_sha256": sha(active_obj),
            "resolved_direct_calls": active_edges,
        },
        "scope": "Selected COFF bodies are bound to the PE with relocations resolved for the caller, crash handler, and OnCrash destination. The verifier checks call order, argument registers, field-store words, and destinations; it does not claim a whole-body match to ARTIST or runtime execution.",
    }
    dest = Path(__file__).resolve().parent / "R172_CRASH_DISPATCH_LINKED.json"
    with dest.open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print("R172_CRASH_DISPATCH_LINKED", sha(dest), hex(post_va), hex(handler_va), hex(active_va))

if __name__ == "__main__":
    main()
