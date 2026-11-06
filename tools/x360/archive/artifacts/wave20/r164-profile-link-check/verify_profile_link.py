#!/usr/bin/env python3
import hashlib
import json
import os
import re
import struct
from pathlib import Path

PINS_PATH = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r164-link\ROOT_LINK_PINS.json")
EXPECTED_PINS_SHA256 = "bd72374e4d7e73ddd06603a7d871475a979a5b0fca9ab64fcfc681c80ce27033"
EXPECTED_INTEGRATED_HEAD = "d9d487ef61d4bab2e80d9e4c2fc64c20a600b08a"
EXPECTED_TREE = "4db7a0e79c33f4ebc6ed8058ee2e63edbc4b3cd3"
BATCH = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r164-batch")
OUTPUT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r164-profile-link-check\EVIDENCE.json")
TELEMETRY = b"[GameStateModule::OnProfileLoaded] junkyard="

SYMBOLS = {
    "on_profile_loaded": "?OnProfileLoaded@GameStateModule@BrnGameState@@QAAXPAUOutputBuffer@GameStateModuleIO@2@PAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z",
    "request_unpause": "?RequestUnpause@GameStateModule@BrnGameState@@QAAXHPAV?$VariableEventQueue@$0DEAA@$0BA@@CgsModule@@@Z",
    "process_group1": "?ProcessGameEvents_Group1@GameStateModule@BrnGameState@@AAAXHPBUEvent@CgsModule@@PAV?$VariableEventQueue@$0DEAA@$0BA@@4@PBUPreWorldInputBuffer@GameStateModuleIO@2@PAUOutputBuffer@72@@Z",
    "process_group4": "?ProcessGameEvents_Group4@GameStateModule@BrnGameState@@AAAXHPBUEvent@CgsModule@@PAV?$VariableEventQueue@$0DEAA@$0BA@@4@PBUPreWorldInputBuffer@GameStateModuleIO@2@PAUOutputBuffer@72@@Z",
}

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

def pinned_file(pins, key):
    entry = pins["pins"][key]
    path = Path(entry["path"])
    require(path.is_file(), f"pinned {key} file missing: {path}")
    digest = sha256(path)
    require(digest == entry["sha256"].lower(), f"pinned {key} hash mismatch")
    return path, digest

def selected_entry(seal, source):
    hits = [item for item in seal["selected"] if item.get("source") == source]
    require(len(hits) == 1, f"selected seal entry count for {source}: {len(hits)}")
    return hits[0]

def parse_map(path):
    found = {}
    with open(path, "r", encoding="latin-1") as f:
        for line in f:
            fields = line.split()
            if len(fields) < 5 or not re.fullmatch(r"[0-9A-Fa-f]{4}:[0-9A-Fa-f]+", fields[0]):
                continue
            symbol = fields[1]
            if symbol in SYMBOLS.values():
                found[symbol] = {
                    "va": int(fields[2], 16),
                    "object": fields[-1],
                    "line": line.rstrip(),
                }
    for symbol in SYMBOLS.values():
        require(symbol in found, f"linked map symbol missing: {symbol}")
    return found

def parse_pe(path):
    data = Path(path).read_bytes()
    require(data[:2] == b"MZ", "pinned PE has no MZ header")
    peoff = struct.unpack_from("<I", data, 0x3C)[0]
    require(data[peoff:peoff + 4] == b"PE\0\0", "pinned PE signature invalid")
    coff = peoff + 4
    machine, section_count = struct.unpack_from("<HH", data, coff)
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x10B:
        image_base = struct.unpack_from("<I", data, opt + 28)[0]
        size_headers = struct.unpack_from("<I", data, opt + 60)[0]
    elif magic == 0x20B:
        image_base = struct.unpack_from("<Q", data, opt + 24)[0]
        size_headers = struct.unpack_from("<I", data, opt + 60)[0]
    else:
        raise SystemExit(f"FAIL: unsupported PE optional-header magic 0x{magic:x}")
    sec0 = opt + optional_size
    sections = []
    for i in range(section_count):
        off = sec0 + i * 40
        name = data[off:off + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        virtual_size, rva, raw_size, raw_ptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, virtual_size, rva, raw_size, raw_ptr))
    return data, machine, image_base, size_headers, sections

def va_to_offset(va, image_base, size_headers, sections):
    rva = va - image_base
    require(rva >= 0, f"VA below image base: 0x{va:08x}")
    if rva < size_headers:
        return rva
    for name, virtual_size, sec_rva, raw_size, raw_ptr in sections:
        extent = max(virtual_size, raw_size)
        if sec_rva <= rva < sec_rva + extent:
            delta = rva - sec_rva
            require(delta + 4 <= raw_size, f"VA is not file-backed in {name}: 0x{va:08x}")
            return raw_ptr + delta
    raise SystemExit(f"FAIL: VA is outside PE sections: 0x{va:08x}")

def word_at(data, va, image_base, size_headers, sections):
    off = va_to_offset(va, image_base, size_headers, sections)
    return struct.unpack_from(">I", data, off)[0]

def branch_target(word, pc):
    require((word >> 26) == 18, f"expected PPC branch at 0x{pc:08x}, got 0x{word:08x}")
    disp = word & 0x03FFFFFC
    if disp & 0x02000000:
        disp -= 0x04000000
    if word & 2:
        target = disp & 0xFFFFFFFF
    else:
        target = (pc + disp) & 0xFFFFFFFF
    return target

def verify_call(label, code, pc, target_va, image_base, size_headers, sections):
    word = word_at(code, pc, image_base, size_headers, sections)
    require(word & 1, f"{label} branch is not linked (LK=0)")
    actual = branch_target(word, pc)
    require(actual == target_va, f"{label} target 0x{actual:08x} != 0x{target_va:08x}")
    return {"pc": f"0x{pc:08x}", "instruction": f"0x{word:08x}", "target": f"0x{actual:08x}"}

def main():
    require(PINS_PATH.is_file(), f"ROOT_LINK_PINS missing: {PINS_PATH}")
    pins_sha = sha256(PINS_PATH)
    require(pins_sha == EXPECTED_PINS_SHA256, "ROOT_LINK_PINS SHA does not match the supplied final SHA")
    pins = read_json(PINS_PATH)
    require(pins["head"] == EXPECTED_INTEGRATED_HEAD, "integrated HEAD differs from the supplied final HEAD")
    require(pins["tree"] == EXPECTED_TREE, "integrated tree differs from R164 composition tree")

    composed = read_json(BATCH / "COMPOSED_PPC_CHECK.json")
    binding_path = BATCH / "TARGETED_OBJECT_BINDING.json"
    binding_sha = sha256(binding_path)
    require(binding_sha == composed["targeted_binding_sha256"].lower(), "targeted object binding SHA mismatch")
    binding = read_json(binding_path)
    require(composed["tree"] == EXPECTED_TREE, "composed PPC check tree mismatch")
    require(binding["tree"] == EXPECTED_TREE, "targeted object binding tree mismatch")

    seal_path, seal_sha = pinned_file(pins, "seal")
    map_path, map_sha = pinned_file(pins, "map")
    pe_path, pe_sha = pinned_file(pins, "pe")
    seal = read_json(seal_path)
    require(seal["head"] == pins["head"], "seal HEAD differs from ROOT_LINK_PINS")

    profile_binding = next(item for item in binding["results"]
                           if item["source"] == "src/GameSource/GameState/BrnGameStateModule.cpp")
    profile_artifact = profile_binding["artifacts"]["object"]
    expected_objects = {
        "on_profile_loaded": {
            "source": "src/GameSource/GameState/BrnGameStateModule.cpp",
            "path": profile_artifact["path"],
            "sha256": profile_artifact["sha256"],
            "source_sha256": profile_binding["source_sha256"],
        }
    }
    dispatch = composed["profile"]["profile_dispatch"]
    for item in dispatch:
        if item["source"].endswith("_wBT_01.cpp"):
            key = "process_group1"
        elif item["source"].endswith("_wBT_04.cpp"):
            key = "process_group4"
        else:
            continue
        expected_objects[key] = {
            "source": item["source"],
            "path": item["object"]["path"],
            "sha256": item["object"]["sha256"],
        }
    require("process_group1" in expected_objects and "process_group4" in expected_objects,
            "composed check lacks both retained ProcessGameEvents callers")

    selected = {}
    for key, expected in expected_objects.items():
        item = selected_entry(seal, expected["source"])
        require(Path(item["object"]).name == Path(expected["path"]).name,
                f"selected object name mismatch for {expected['source']}")
        if key != "on_profile_loaded":
            require(item["sha256"].lower() == expected["sha256"].lower(),
                    f"selected object SHA mismatch for {expected['source']}")
        selected[key] = {
            "source": expected["source"],
            "object": Path(item["object"]).name,
            "sha256": item["sha256"].lower(),
        }
        if key == "on_profile_loaded":
            selected[key]["preflight_object_sha256"] = expected["sha256"].lower()
            selected[key]["preflight_object_hash_matches_link"] = (
                item["sha256"].lower() == expected["sha256"].lower())

    profile_source_suffix = "/src/GameSource/GameState/BrnGameStateModule.cpp"
    profile_sources = [item for item in seal["sources"]
                       if item["path"].replace("\\", "/").endswith(profile_source_suffix)]
    require(len(profile_sources) == 1, "sealed profile source entry is missing or duplicated")
    require(profile_sources[0]["sha256"].lower() == profile_binding["source_sha256"].lower(),
            "sealed profile source SHA differs from the composed candidate")

    linked = parse_map(map_path)
    symbol_owner_key = {
        SYMBOLS["on_profile_loaded"]: "on_profile_loaded",
        SYMBOLS["request_unpause"]: "on_profile_loaded",
        SYMBOLS["process_group1"]: "process_group1",
        SYMBOLS["process_group4"]: "process_group4",
    }
    for symbol, info in linked.items():
        owner_key = symbol_owner_key[symbol]
        require(Path(info["object"]).name == selected[owner_key]["object"],
                f"map owner differs from selected object for {owner_key}")

    data, machine, image_base, size_headers, sections = parse_pe(pe_path)
    require(machine == 0x1F2, f"unexpected Xbox 360 PE machine 0x{machine:04x}")
    onprofile = linked[SYMBOLS["on_profile_loaded"]]["va"]
    request = linked[SYMBOLS["request_unpause"]]["va"]
    group1 = linked[SYMBOLS["process_group1"]]["va"]
    group4 = linked[SYMBOLS["process_group4"]]["va"]

    unpause_offset = int(composed["profile"]["unpause_offset"], 0)
    group1_offset = int(next(x for x in dispatch if x["source"].endswith("_wBT_01.cpp"))["call_offset"], 0)
    group4_offset = int(next(x for x in dispatch if x["source"].endswith("_wBT_04.cpp"))["call_offset"], 0)
    calls = {
        "OnProfileLoaded_to_RequestUnpause": verify_call(
            "OnProfileLoaded to RequestUnpause", data, onprofile + unpause_offset,
            request, image_base, size_headers, sections),
        "Group1_to_OnProfileLoaded": verify_call(
            "ProcessGameEvents_Group1 to OnProfileLoaded", data, group1 + group1_offset,
            onprofile, image_base, size_headers, sections),
        "Group4_to_OnProfileLoaded": verify_call(
            "ProcessGameEvents_Group4 to OnProfileLoaded", data, group4 + group4_offset,
            onprofile, image_base, size_headers, sections),
    }

    restore = word_at(data, onprofile + unpause_offset + 4, image_base, size_headers, sections)
    epilogue = word_at(data, onprofile + unpause_offset + 8, image_base, size_headers, sections)
    require(restore == 0x382100B0, f"OnProfileLoaded stack restore mismatch: 0x{restore:08x}")
    require((epilogue >> 26) == 18 and not (epilogue & 1),
            f"OnProfileLoaded post-unpause transfer is not a no-link epilogue branch: 0x{epilogue:08x}")

    present = data.find(TELEMETRY)
    require(present < 0, "telemetry literal is present in the pinned PE")

    evidence = {
        "schema": "r164-profile-linked-check-v1",
        "result": "PASS",
        "root_link_pins": {
            "path": str(PINS_PATH),
            "sha256": pins_sha,
            "head": pins["head"],
            "tree": pins["tree"],
        },
        "pinned_files": {
            "seal_sha256": seal_sha,
            "map_sha256": map_sha,
            "pe_sha256": pe_sha,
        },
        "selected_objects": selected,
        "profile_source_sha256": profile_sources[0]["sha256"].lower(),
        "linked_symbols": {
            key: {
                "va": f"0x{info['va']:08x}",
                "object": Path(info["object"]).name,
            }
            for key, info in linked.items()
        },
        "calls": calls,
        "profile_tail": {
            "request_unpause_offset": f"0x{unpause_offset:x}",
            "stack_restore": f"0x{restore:08x}",
            "epilogue_branch": f"0x{epilogue:08x}",
            "post_call_is_linked_call": False,
        },
        "telemetry_literal": {
            "ascii": TELEMETRY.decode("ascii"),
            "present_in_pinned_pe": False,
        },
    }
    OUTPUT.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print("PASS: linked profile bindings, call destinations, final RequestUnpause action, and PE literal absence")
    print(f"Evidence: {OUTPUT}")
    print(f"Evidence SHA256: {sha256(OUTPUT)}")

if __name__ == "__main__":
    main()
