#!/usr/bin/env python3
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20")
BATCH = ROOT / "r166-batch"
SETUP_PROOF = ROOT / "r166-junkyard-setup" / "PPC_SETUP_PROOF.json"
TARGET_BINDING = BATCH / "TARGETED_OBJECT_BINDING.json"
COMPOSED_CHECK = BATCH / "COMPOSED_PPC_CHECK.json"
EXIT_RECEIPT = ROOT / "r166-junkyard-transition" / "RECEIPT.md"
OUTPUT = ROOT / "r166-junkyard-linked-check" / "EVIDENCE.json"
EXPECTED_TREE = "77910cc622803ecd685ea93afca520619840d51c"
EXPECTED_INTEGRATED_HEAD = "135948fd62910b9dc8911e865cf476555d05460e"
TELEMETRY = b"[GameStateModule::ProcessGameEvents case 94] action 4 -> ExitJunkyard"

SETUP = "?SendSetupPlayerCarEvent@GameStateModule@BrnGameState@@QAAXPAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z"
EXIT_HANDLER = "?ProcessGameEvents_Group1@GameStateModule@BrnGameState@@AAAXHPBUEvent@CgsModule@@PAV?$VariableEventQueue@$0DEAA@$0BA@@4@PBUPreWorldInputBuffer@GameStateModuleIO@2@PAUOutputBuffer@72@@Z"
EXIT_CALL = "?ExitJunkyard@CarSelectManager@BrnGameState@@QAAXPAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z"

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def require(ok, message):
    if not ok:
        raise SystemExit("FAIL: " + message)

def pin_file(pins, key):
    entry = pins["pins"][key]
    path = Path(entry["path"])
    require(path.is_file(), f"pinned {key} file missing: {path}")
    digest = sha256(path)
    require(digest == entry["sha256"].lower(), f"pinned {key} SHA mismatch")
    return path, digest

def unique_selected(seal, source):
    hits = [item for item in seal["selected"] if item.get("source") == source]
    require(len(hits) == 1, f"selected object count for {source}: {len(hits)}")
    return hits[0]

def map_symbols(path, expected):
    found = {}
    with open(path, "r", encoding="latin-1") as f:
        for line in f:
            fields = line.split()
            if len(fields) < 5 or not re.fullmatch(r"[0-9A-Fa-f]{4}:[0-9A-Fa-f]+", fields[0]):
                continue
            symbol = fields[1]
            if symbol in expected:
                found[symbol] = {"va": int(fields[2], 16), "object": fields[-1], "line": line.rstrip()}
    for symbol in expected:
        require(symbol in found, f"linked map symbol missing: {symbol}")
    return found

def parse_pe(path):
    data = Path(path).read_bytes()
    require(data[:2] == b"MZ", "pinned PE lacks MZ header")
    peoff = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[peoff:peoff + 4] == b"PE\0\0", "pinned PE signature invalid")
    coff = peoff + 4
    machine, section_count = struct.unpack_from("<HH", data, coff)
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x10B:
        image_base = struct.unpack_from("<I", data, opt + 28)[0]
    elif magic == 0x20B:
        image_base = struct.unpack_from("<Q", data, opt + 24)[0]
    else:
        raise SystemExit(f"FAIL: unsupported PE magic 0x{magic:x}")
    size_headers = struct.unpack_from("<I", data, opt + 60)[0]
    sections = []
    start = opt + optional_size
    for i in range(section_count):
        off = start + i * 40
        name = data[off:off + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        vs, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, vs, rva, raw_size, raw_ptr))
    return data, machine, image_base, size_headers, sections

def va_offset(va, image_base, size_headers, sections):
    rva = va - image_base
    require(rva >= 0, f"address below image base: 0x{va:08x}")
    if rva < size_headers:
        return rva
    for name, vs, sec_rva, raw_size, raw_ptr in sections:
        if sec_rva <= rva < sec_rva + max(vs, raw_size):
            delta = rva - sec_rva
            require(delta + 4 <= raw_size, f"address not raw-backed in {name}: 0x{va:08x}")
            return raw_ptr + delta
    raise SystemExit(f"FAIL: address outside PE sections: 0x{va:08x}")

def word_at(data, va, image_base, size_headers, sections):
    off = va_offset(va, image_base, size_headers, sections)
    return struct.unpack_from(">I", data, off)[0]

def branch_target(word, pc):
    require((word >> 26) == 18, f"expected PPC branch at 0x{pc:08x}, got 0x{word:08x}")
    disp = word & 0x03FFFFFC
    if disp & 0x02000000:
        disp -= 0x04000000
    return (disp if (word & 2) else pc + disp) & 0xFFFFFFFF

def check_call(label, data, pc, target, image_base, size_headers, sections):
    word = word_at(data, pc, image_base, size_headers, sections)
    require(word & 1, f"{label} is not a linked call")
    actual = branch_target(word, pc)
    require(actual == target, f"{label} resolved to 0x{actual:08x}, expected 0x{target:08x}")
    return {"pc": f"0x{pc:08x}", "word": f"0x{word:08x}", "target": f"0x{actual:08x}"}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pins-sha256", required=True)
    args = ap.parse_args()

    pins_path = ROOT / "r166-link" / "ROOT_LINK_PINS.json"
    require(pins_path.is_file(), f"ROOT_LINK_PINS missing: {pins_path}")
    pins_sha = sha256(pins_path)
    require(pins_sha == args.pins_sha256.lower(), "ROOT_LINK_PINS SHA differs from supplied final SHA")
    pins = read_json(pins_path)
    require(pins["tree"] == EXPECTED_TREE, "linked tree differs from R166 candidate tree")
    require(pins["head"] == EXPECTED_INTEGRATED_HEAD, "integrated HEAD differs from the supplied final HEAD")

    composed = read_json(COMPOSED_CHECK)
    proof = read_json(SETUP_PROOF)
    binding = read_json(TARGET_BINDING)
    require(composed["tree"] == EXPECTED_TREE and binding["tree"] == EXPECTED_TREE,
            "candidate PPC/binding tree mismatch")
    require(proof["original"] == "0x8239A918", "setup PPC proof points at a different ARTIST function")
    require(proof.get("extra_log_absent") is True, "setup proof does not attest log absence")
    require(composed["junkyard_exit"].get("extra_log_absent") is True,
            "composed exit proof does not attest log absence")
    proof_calls = [(item["offset"], item["name"]) for item in proof["checked_calls"]]
    composed_calls = [(item[0], item[1]) for item in composed["junkyard_setup"]["ordered_native_calls"]]
    require(proof_calls == composed_calls, "setup PPC proof call order differs from composed check")

    bind_sha = sha256(TARGET_BINDING)
    require(bind_sha == composed["targeted_binding_sha256"].lower(), "targeted object binding SHA mismatch")

    seal_path, seal_sha = pin_file(pins, "seal")
    map_path, map_sha = pin_file(pins, "map")
    pe_path, pe_sha = pin_file(pins, "pe")
    seal = read_json(seal_path)
    require(seal["head"] == pins["head"], "seal HEAD differs from final link pins")

    by_source = {item["source"]: item for item in binding["results"]}
    setup_source = "src/GameSource/GameState/BrnGameStateModule.cpp"
    exit_source = "src/GameSource/GameState/GameStateModule_ProcessGameEvents_wBT_01.cpp"
    setup_binding = by_source[setup_source]
    exit_binding = by_source[exit_source]
    setup_selected = unique_selected(seal, setup_source)
    exit_selected = unique_selected(seal, exit_source)
    require(Path(setup_selected["object"]).name == Path(setup_binding["artifacts"]["object"]["path"]).name,
            "linked setup object name differs from candidate binding")
    require(Path(exit_selected["object"]).name == Path(exit_binding["artifacts"]["object"]["path"]).name,
            "linked exit object name differs from candidate binding")

    suffixes = {
        setup_source: setup_binding["source_sha256"].lower(),
        exit_source: exit_binding["source_sha256"].lower(),
    }
    for source, digest in suffixes.items():
        tail = "/" + source.replace("\\", "/")
        matches = [item for item in seal["sources"] if item["path"].replace("\\", "/").endswith(tail)]
        require(len(matches) == 1 and matches[0]["sha256"].lower() == digest,
                f"sealed source SHA mismatch for {source}")

    native_calls = composed["junkyard_setup"]["ordered_native_calls"]
    expected = {symbol for _, symbol in native_calls}
    expected.add(SETUP)
    expected.add(EXIT_HANDLER)
    expected.add(EXIT_CALL)
    symbols = map_symbols(map_path, expected)

    selected_names = {Path(item["object"]).name for item in seal["selected"]}
    for symbol, info in symbols.items():
        owner = Path(info["object"]).name
        require(owner in selected_names, f"map owner not present in selected seal objects for {symbol}")
    require(Path(symbols[SETUP]["object"]).name == Path(setup_selected["object"]).name,
            "setup function link-map owner differs from its selected object")
    require(Path(symbols[EXIT_HANDLER]["object"]).name == Path(exit_selected["object"]).name,
            "case 94 link-map owner differs from its selected object")

    data, machine, image_base, size_headers, sections = parse_pe(pe_path)
    require(machine == 0x1F2, f"unexpected Xbox 360 PE machine 0x{machine:04x}")
    setup_va = symbols[SETUP]["va"]
    exit_va = symbols[EXIT_HANDLER]["va"]
    calls = {}
    for offset_text, symbol in native_calls:
        offset = int(offset_text, 0)
        calls[f"setup_{offset_text}_{symbol}"] = check_call(
            f"setup call at {offset_text}", data, setup_va + offset, symbols[symbol]["va"],
            image_base, size_headers, sections)
    exit_offset = int(composed["junkyard_exit"]["call_offsets"][0], 0)
    calls["case94_exit_junkyard"] = check_call(
        "case 94 ExitJunkyard", data, exit_va + exit_offset, symbols[EXIT_CALL]["va"],
        image_base, size_headers, sections)

    drive_offset = int(next(x[0] for x in native_calls if "OnDriveThru@" in x[1]), 0)
    wait_words = [
        (drive_offset + 4, 0x3D600003),
        (drive_offset + 8, 0x39400001),
        (drive_offset + 12, 0x616B8B72),
        (drive_offset + 16, 0x7D5F59AE),
    ]
    wait_store = []
    for offset, expected_word in wait_words:
        actual = word_at(data, setup_va + offset, image_base, size_headers, sections)
        require(actual == expected_word, f"wait-flag sequence mismatch at +0x{offset:x}: 0x{actual:08x}")
        wait_store.append({"offset": f"0x{offset:x}", "word": f"0x{actual:08x}"})

    telemetry_present = data.find(TELEMETRY) >= 0
    require(not telemetry_present, "case94 telemetry literal is present in the pinned PE")

    evidence = {
        "schema": "r166-junkyard-linked-check-v1",
        "result": "PASS",
        "root_link_pins": {
            "sha256": pins_sha,
            "head": pins["head"],
            "tree": pins["tree"],
        },
        "pinned_files": {"seal_sha256": seal_sha, "map_sha256": map_sha, "pe_sha256": pe_sha},
        "selected_sources": {
            "setup": {
                "object": Path(setup_selected["object"]).name,
                "sha256": setup_selected["sha256"],
                "source_sha256": setup_binding["source_sha256"],
                "preflight_object_sha256": setup_binding["artifacts"]["object"]["sha256"],
                "preflight_object_hash_matches_link": setup_selected["sha256"].lower() == setup_binding["artifacts"]["object"]["sha256"].lower(),
            },
            "exit": {
                "object": Path(exit_selected["object"]).name,
                "sha256": exit_selected["sha256"],
                "source_sha256": exit_binding["source_sha256"],
                "preflight_object_sha256": exit_binding["artifacts"]["object"]["sha256"],
                "preflight_object_hash_matches_link": exit_selected["sha256"].lower() == exit_binding["artifacts"]["object"]["sha256"].lower(),
            },
        },
        "symbols": {
            name: {"va": f"0x{info['va']:08x}", "object": Path(info["object"]).name}
            for name, info in symbols.items()
        },
        "calls": calls,
        "wait_flag_after_on_drive_thru": wait_store,
        "telemetry_literal": {"ascii": TELEMETRY.decode("ascii"), "present_in_pinned_pe": False},
    }
    OUTPUT.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print("PASS: junkyard setup/exit bindings, call destinations, wait flag, and PE telemetry absence")
    print(f"Evidence: {OUTPUT}")
    print(f"Evidence SHA256: {sha256(OUTPUT)}")

if __name__ == "__main__":
    main()
