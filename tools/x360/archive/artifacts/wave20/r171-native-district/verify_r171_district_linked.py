"""Verify the R171 district producer in the sealed native image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess

WAVE = Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
OFFICIAL = Path(r"C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANDIDATE = "a8176256cbe97bbf91b1abc59fd7314361e5cbca"
CANDIDATE_TREE = "48c7575e7cabf2318abe1bc6e2c6f60256f02794"
BASE = "95836632b166748b4b3ed0a1ed5d57be02122f29"
EXPECTED_HEAD = "b7b549c7f423d38c8ddc63ba089c11152e66d8fd"
EXPECTED_TREE = "48c7575e7cabf2318abe1bc6e2c6f60256f02794"
PREFLIGHT_SHA = "6322cf3fc2b0f6434348c16fa4d123d00262719ad2164bfb5cc0f567da346523"
SOURCE = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
SYMBOL = "?UpdateCurrentWorldRegion@RaceCarEntityModule@BrnWorld@@AAAXPAUGameEventQueue@RaceCarEntityModuleIO@2@@Z"
OLD_LOG = b"[district] region change -> county "

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
    assert (head, tree, parent) == (EXPECTED_HEAD, EXPECTED_TREE, BASE)
    assert git("rev-parse", head, head + "^{tree}", head + "^").splitlines() == [head, tree, parent]
    assert git("rev-parse", CANDIDATE + "^{tree}", CANDIDATE + "^").splitlines() == [CANDIDATE_TREE, BASE]
    assert git("rev-parse", head + ":" + SOURCE) == git("rev-parse", CANDIDATE + ":" + SOURCE)

    for item in pins["pins"].values():
        assert sha(item["path"]) == item["sha256"], item["path"]
    completion = read(pins["pins"]["completion"]["path"])
    result = read(pins["pins"]["result"]["path"])
    diagnostic = read(pins["pins"]["diagnostics"]["path"])
    image_report = read(pins["pins"]["image"]["path"])
    assert completion["status"] == "SEALED_NO_REGRESSIONS"
    assert (completion["head"], completion["passed"], completion["failed"]) == (head, 3710, 0)
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert not diagnostic["unique_symbols"] and not diagnostic["duplicate_messages"]
    assert image_report["head"] == head and image_report["image_sha256"] == pins["pins"]["xex"]["sha256"]
    seal = read(pins["pins"]["seal"]["path"])
    invocation = read(pins["pins"]["invocation"]["path"])
    assert seal["head"] == head
    assert invocation["sealed_sha256"] == pins["pins"]["seal"]["sha256"]

    preflight_path = WAVE / "r171-batch/NATIVE_PPC_PREFLIGHT.json"
    assert sha(preflight_path) == PREFLIGHT_SHA
    proof = read(preflight_path)
    assert (proof["status"], proof["head"], proof["tree"], proof["base"]) == (
        "pass", CANDIDATE, CANDIDATE_TREE, BASE
    )
    district = proof["district"]
    assert (district["source"], district["symbol"]) == (SOURCE, SYMBOL)
    preflight_obj = Path(district["object"]["path"])
    assert sha(preflight_obj) == district["object"]["sha256"]
    helpers = runpy.run_path(str(WAVE / "r156-layout-linked/verify_layout_linked.py"))["m"]
    linked_address, linked_bytes = helpers["linked_reader"](link)
    map_path = Path(pins["pins"]["map"]["path"])
    mapping = map_path.read_text(errors="replace")

    selected, = [row for row in seal["selected"] if row["source"] == SOURCE]
    invoked, = [row for row in invocation["objects"] if row["source"] == SOURCE]
    obj = Path(selected["object"])
    assert obj == Path(invoked["object"])
    assert sha(obj) == selected["sha256"] == invoked["sha256"]
    assert git("rev-parse", head + ":" + SOURCE) == git("rev-parse", CANDIDATE + ":" + SOURCE)
    pattern = rf"(?m)^\s*[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(SYMBOL)}\s+([0-9a-fA-F]{{8}})\s+f(?:\s+i)?\s+(\S+)"
    (address, owner), = re.findall(pattern, mapping)
    va = int(address, 16)
    assert va == linked_address(SYMBOL) and obj.name == owner
    body, rels = helpers["collect"](obj, SYMBOL)
    preflight_body, preflight_rels = helpers["collect"](preflight_obj, SYMBOL)
    assert len(body) == district["body_bytes"] == 268
    assert hashlib.sha256(body).hexdigest() == district["body_sha256"]
    assert body == preflight_body
    data, preflight_data = obj.read_bytes(), preflight_obj.read_bytes()
    def signatures(raw, relocations):
        return [(off, kind, helpers["object_symbol"](raw, index)[0]) for off, index, kind in relocations]
    assert signatures(data, rels) == signatures(preflight_data, preflight_rels)
    linked = linked_bytes(va, len(body))
    helpers["fixed_equal"](body, linked, rels)

    for offset, word in district["words"].items():
        at = int(offset, 16)
        expected = bytes.fromhex(word)
        assert body[at:at+4] == linked[at:at+4] == expected, offset

    def branch(call):
        at = int(call["offset"], 16)
        (_, index, kind), = [row for row in rels if row[0] == at and row[2] == 6]
        assert kind == 6
        target_symbol = helpers["object_symbol"](data, index)[0]
        assert target_symbol == call["symbol"]
        word = struct.unpack_from(">I", linked, at)[0]
        target = helpers["branch_target"](va + at, word)
        assert word & 1 and target == linked_address(target_symbol)
        return {"offset": call["offset"], "symbol": target_symbol, "target": hex(target)}

    calls = [branch(call) for call in district["calls"]]
    assert calls[-1]["offset"] == "0x100"
    assert calls[-1]["symbol"] == "?AddEvent@?$VariableEventQueue@$0GAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z"
    assert district["event"] == 115 and district["size"] == 8
    size_word = struct.unpack_from(">I", linked, 0xec)[0]
    event_word = struct.unpack_from(">I", linked, 0xf0)[0]
    assert (size_word >> 26, (size_word >> 21) & 31, size_word & 0xffff) == (14, 6, 8)
    assert (event_word >> 26, (event_word >> 21) & 31, event_word & 0xffff) == (14, 5, 115)

    expected_assert_calls = [
        (0x58, "?BeginAssert@Assert@CgsDev@@YAXXZ"),
        (0x6c, "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z"),
        (0x70, "?EndAssert@Assert@CgsDev@@YAXXZ"),
        (0x84, "?BeginAssert@Assert@CgsDev@@YAXXZ"),
        (0x98, "?FireAssert@Assert@CgsDev@@YAXPBD0H@Z"),
        (0x9c, "?EndAssert@Assert@CgsDev@@YAXXZ"),
    ]
    assert_calls = []
    for offset, symbol in expected_assert_calls:
        assert_calls.append(branch({"offset": hex(offset), "symbol": symbol}))

    local_strings = {}
    for offset, index, kind in rels:
        if kind != 0x10:
            continue
        name = helpers["object_symbol"](data, index)[0]
        if name == district["mask_symbol"]:
            continue
        literal = helpers["symbol_literal"](data, index)
        (low_offset, low_index, _), = [
            row for row in rels
            if row[2] == 0x11 and helpers["object_symbol"](data, row[1])[0] == name
        ]
        assert helpers["object_symbol"](data, low_index)[0] == name
        target = helpers["data_target"](linked, offset, low_offset)
        raw = literal.encode("ascii") + b"\0"
        assert linked_bytes(target, len(raw)) == raw
        assert literal not in local_strings
        local_strings[literal] = {"address": hex(target), "bytes": len(raw) - 1}

    expected_strings = {
        district["source_path"],
        district["message_by_line"]["3075"],
        district["message_by_line"]["3076"],
    }
    assert set(local_strings) == expected_strings
    assert local_strings[district["message_by_line"]["3075"]]["address"]
    assert local_strings[district["message_by_line"]["3076"]]["address"]

    mask_name = district["mask_symbol"]
    upper, lower = [int(offset, 16) for offset in district["mask_offsets"]]
    (_, upper_index, upper_kind), = [row for row in rels if row[0] == upper and row[2] == 0x10]
    (_, lower_index, lower_kind), = [row for row in rels if row[0] == lower and row[2] == 0x11]
    assert upper_kind == 0x10 and lower_kind == 0x11
    assert helpers["object_symbol"](data, upper_index)[0] == mask_name
    assert helpers["object_symbol"](data, lower_index)[0] == mask_name
    mask_va = helpers["data_target"](linked, upper, lower)
    mask = linked_bytes(mask_va, 16)
    assert mask_va % 16 == 0 and mask.hex() == district["mask_bytes"]

    pe = Path(pins["pins"]["pe"]["path"]).read_bytes()
    assert OLD_LOG not in pe
    receipt = {
        "status": "pass",
        "head": head,
        "tree": tree,
        "parent": parent,
        "root_pins_sha256": args.root_pins_sha,
        "pe_sha256": pins["pins"]["pe"]["sha256"],
        "xex_sha256": pins["pins"]["xex"]["sha256"],
        "preflight_sha256": PREFLIGHT_SHA,
        "district": {
            "symbol": SYMBOL,
            "address": hex(va),
            "body_bytes": len(body),
            "body_sha256": district["body_sha256"],
            "selected_object_sha256": sha(obj),
            "calls": calls,
            "assert_calls": assert_calls,
            "resolved_strings": local_strings,
            "event": district["event"],
            "size": district["size"],
            "size_word": hex(size_word),
            "event_word": hex(event_word),
            "mask_symbol": mask_name,
            "mask_address": hex(mask_va),
            "mask_bytes": mask.hex(),
            "old_district_log_absent": OLD_LOG.decode("ascii"),
        },
        "scope": "Selected linked PPC body, both original assert messages and source path resolved through PE relocations, original vperm mask and event 115/size 8 arguments, old district diagnostic absence. Runtime and other functions in the TU are unverified.",
    }
    dest = Path(__file__).resolve().parent / "R171_DISTRICT_LINKED.json"
    with dest.open("x", encoding="utf-8") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print("R171_DISTRICT_LINKED", sha(dest), hex(va), hex(mask_va))

if __name__ == "__main__":
    main()
