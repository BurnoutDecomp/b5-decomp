"""Read-only R173 linked verification for ten StateManager primary vtables."""
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

import pefile

ARTIFACT_DIR = Path(__file__).resolve().parent
OUT_PATH = ARTIFACT_DIR / "R173_SOUND_VTABLES_LINKED.json"
PINS_PATH = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r173-link\ROOT_LINK_PINS.json")
EXPECTED_PINS_SHA = "181b603ce18ee655ebc666a6fcb33bf1d2f3d3410345c3dd5d7010782f004a2c"
EXPECTED_HEAD = "2ddfc33a3ba62295655a2f897711e5d343df20b2"
EXPECTED_TREE = "088a9731a493058f90c755dbe4b9e3636abb12b7"
EXPECTED_XEX_SHA = "0cd21490477530b657c5bc2bdf85301a7af95039ca7b5becfc70ebc31f55f06a"
REPO = Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
ARTIST_RAW_SHA = "9c15e6b269d6f6e5248038092cd77d48af098d8b3a682e60cc6175bbcb420bad"
ARTIST_PREPARE = bytes.fromhex("7C6B1B783940000438600001914B00244E800020")

def sha(data):
    return hashlib.sha256(data).hexdigest()

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

pins_bytes = PINS_PATH.read_bytes()
require(sha(pins_bytes) == EXPECTED_PINS_SHA, "ROOT_LINK_PINS SHA mismatch")
pins = json.loads(pins_bytes)
require(pins["head"].lower() == EXPECTED_HEAD, "pinned HEAD mismatch")
require(pins["tree"].lower() == EXPECTED_TREE, "pinned tree mismatch")
pin_hashes = {"root_link_pins": EXPECTED_PINS_SHA}
pinned_data = {}
for key in ("completion", "seal", "invocation", "result", "pe", "map", "xex", "image", "diagnostics", "xcontent"):
    item = pins["pins"][key]
    data = Path(item["path"]).read_bytes()
    actual = sha(data)
    require(actual == item["sha256"].lower(), key + " SHA mismatch")
    pin_hashes[key] = actual
    pinned_data[key] = data
require(pin_hashes["xex"] == EXPECTED_XEX_SHA, "supplied XEX SHA mismatch")

seal = json.loads(pinned_data["seal"])
invocation = json.loads(pinned_data["invocation"])
result = json.loads(pinned_data["result"])
require(seal["head"].lower() == EXPECTED_HEAD, "seal HEAD mismatch")
require(invocation["sealed_sha256"].lower() == pins["pins"]["seal"]["sha256"].lower(), "invocation/seal mismatch")
require(result["exit_code"] == 0 and result["all_post_checks_pass"] is True, "link result not passing")
for key in ("pe", "map", "xex"):
    target_name = Path(pins["pins"][key]["path"]).name.lower()
    matched = [v for k, v in result["outputs"].items() if Path(k).name.lower() == target_name]
    require(len(matched) == 1 and matched[0]["sha256"].lower() == pin_hashes[key], "result output binding mismatch: " + key)

tree = subprocess.check_output(
    ["git", "-C", str(REPO), "rev-parse", EXPECTED_HEAD + "^{tree}"],
    text=True,
).strip().lower()
require(tree == EXPECTED_TREE, "git tree mismatch")

source_paths = {
    "base_header": "src/GameShared/GameClasses/Sound/Logic/CgsStateManager.h",
    "base_cpp": "src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp",
    "vehicle_header": "src/GameSource/Sound/Vehicles/BrnVehicleStateManager.h",
}
source_text = {}
for key, relpath in source_paths.items():
    source_text[key] = subprocess.check_output(
        ["git", "-C", str(REPO), "show", EXPECTED_HEAD + ":" + relpath],
    ).decode("utf-8", errors="replace")

def has_nonconst_signature(text, pattern):
    return re.search(pattern, text, re.MULTILINE) is not None

signature_checks = {
    "StateManager::IsDataLoaded() nonconst declaration": has_nonconst_signature(
        source_text["base_header"],
        r"^\s*virtual\s+bool\s+IsDataLoaded\s*\(\s*\)\s*;",
    ),
    "StateManager::IsDataLoaded() nonconst definition": has_nonconst_signature(
        source_text["base_cpp"],
        r"^\s*bool\s+StateManager::IsDataLoaded\s*\(\s*\)\s*$",
    ),
    "StateManager::IsStateAlias(s32) nonconst declaration": has_nonconst_signature(
        source_text["base_header"],
        r"^\s*virtual\s+bool\s+IsStateAlias\s*\(\s*s32\s+liState\s*\)\s*\{",
    ),
    "VehicleStateManager::IsStateAlias(s32) nonconst declaration": has_nonconst_signature(
        source_text["vehicle_header"],
        r"^\s*virtual\s+bool\s+IsStateAlias\s*\(\s*s32\s+liState\s*\)(?:\s+override)?\s*\{",
    ),
}
# A source line carrying a trailing const would fail the patterns above.
signature_pass = all(signature_checks.values())

pe_bytes = pinned_data["pe"]
map_bytes = pinned_data["map"]
pe = pefile.PE(data=pe_bytes)
image_base = pe.OPTIONAL_HEADER.ImageBase
map_text = map_bytes.decode("utf-8", errors="replace")
entry_re = re.compile(r"\s*([0-9a-fA-F]{4}:[0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})\s+(.*)")
entries = []
for line in map_text.splitlines():
    m = entry_re.match(line)
    if m:
        entries.append({
            "section_offset": m.group(1),
            "name": m.group(2),
            "va": int(m.group(3), 16),
            "owner": m.group(4).split()[-1] if m.group(4).split() else "",
        })
names_by_va = {}
for entry in entries:
    names_by_va.setdefault(entry["va"], []).append(entry["name"])

def section_read(va, count):
    rva = va - image_base
    candidates = [
        section for section in pe.sections
        if section.VirtualAddress <= rva
        and rva + count <= section.VirtualAddress + section.SizeOfRawData
    ]
    require(len(candidates) == 1, "VA not in exactly one declared raw section: " + hex(va))
    section = candidates[0]
    offset = section.PointerToRawData + rva - section.VirtualAddress
    return {
        "bytes": pe_bytes[offset:offset + count],
        "file_offset": offset,
        "rva": rva,
        "section": section.Name.rstrip(b"\0").decode("ascii", errors="replace"),
    }

def word(va):
    return struct.unpack(">I", section_read(va, 4)["bytes"])[0]

def method_prefix(method, owner):
    return "?" + method + "@" + owner

state_owner = "StateManager@Logic@CgsSound@@"
vehicle_owner = "VehicleStateManager@Vehicles@BrnSound@@"
class_owners = {
    "StateManager": state_owner,
    "BrnStateManager": "BrnStateManager@Logic@BrnSound@@",
    "GlobalStateManager": "GlobalStateManager@Logic@BrnSound@@",
    "StreamingStateManager": "StreamingStateManager@Streaming@Logic@BrnSound@@",
    "TrafficStateManager": "TrafficStateManager@Traffic@Logic@BrnSound@@",
    "EmitterStateManager": "EmitterStateManager@World@Logic@BrnSound@@",
    "CollisionStateManager": "CollisionStateManager@Collision@Logic@BrnSound@@",
    "PassbyStateManager": "PassbyStateManager@Passby@Logic@BrnSound@@",
    "PlayerVehicleStateManager": "PlayerVehicleStateManager@Vehicles@BrnSound@@",
    "AIVehicleStateManager": "AIVehicleStateManager@Vehicles@BrnSound@@",
}
table_prefixes = {
    "StateManager": "??_7StateManager@Logic@CgsSound@@6B@",
    "BrnStateManager": "??_7BrnStateManager@Logic@BrnSound@@6BStateManager@",
    "GlobalStateManager": "??_7GlobalStateManager@Logic@BrnSound@@6BStateManager@",
    "StreamingStateManager": "??_7StreamingStateManager@Streaming@Logic@BrnSound@@6BStateManager@",
    "TrafficStateManager": "??_7TrafficStateManager@Traffic@Logic@BrnSound@@6BStateManager@",
    "EmitterStateManager": "??_7EmitterStateManager@World@Logic@BrnSound@@6BStateManager@",
    "CollisionStateManager": "??_7CollisionStateManager@Collision@Logic@BrnSound@@6BStateManager@",
    "PassbyStateManager": "??_7PassbyStateManager@Passby@Logic@BrnSound@@6BStateManager@",
    "PlayerVehicleStateManager": "??_7PlayerVehicleStateManager@Vehicles@BrnSound@@6BStateManager@",
    "AIVehicleStateManager": "??_7AIVehicleStateManager@Vehicles@BrnSound@@6BStateManager@",
}
artist_tables = {
    "StateManager": (0x820B66E4, {0x04:0x8268D798,0x08:0x8268D7A8,0x0C:0x8268D7B8,0x14:0x8268D7D0,0x18:0x8268D800,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "BrnStateManager": (0x820B7C6C, {0x04:0x82682AA8,0x08:0x82682AB8,0x0C:0x8268D7B8,0x14:0x8268D7D0,0x18:0x8268D800,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "GlobalStateManager": (0x820B7DA8, {0x04:0x826867D8,0x08:0x826867E8,0x0C:0x826F5BF8,0x14:0x8268D7D0,0x18:0x8269C368,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "StreamingStateManager": (0x820B7F80, {0x04:0x82683A78,0x08:0x82683A88,0x0C:0x826EE680,0x14:0x826B0EB8,0x18:0x826B0D38,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "TrafficStateManager": (0x820B7FB4, {0x04:0x826841C8,0x08:0x826841D8,0x0C:0x826F0E78,0x14:0x8268D7D0,0x18:0x82702ED8,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48,0x2C:0x826CB358}),
    "EmitterStateManager": (0x820B81C0, {0x04:0x826867A8,0x08:0x826867B8,0x0C:0x826F5AC0,0x14:0x8269C200,0x18:0x826E6F58,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "CollisionStateManager": (0x820B844C, {0x04:0x82688758,0x08:0x82688768,0x0C:0x826F8B78,0x14:0x826BD928,0x18:0x82704420,0x1C:0x8268D888,0x20:0x826F8E68,0x24:0x826C4C60,0x28:0x82680D48}),
    "PassbyStateManager": (0x820B8484, {0x04:0x82688FE8,0x08:0x82688FF8,0x0C:0x826F9748,0x14:0x8268D7D0,0x18:0x826D4D00,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82680D48}),
    "PlayerVehicleStateManager": (0x820B87A0, {0x04:0x82683FF8,0x08:0x82684008,0x0C:0x826EF428,0x14:0x8268D7D0,0x18:0x826EF7E8,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82683D50}),
    "AIVehicleStateManager": (0x820B87D4, {0x04:0x82684018,0x08:0x82684028,0x0C:0x826EFC18,0x14:0x826B1B28,0x18:0x826CA578,0x1C:0x8268D888,0x20:0x8284CB38,0x24:0x826C4C60,0x28:0x82683D50}),
}
slots = (0x04,0x08,0x0C,0x14,0x18,0x1C,0x20,0x24,0x28)
prepare_overrides = {"GlobalStateManager","StreamingStateManager","TrafficStateManager","EmitterStateManager","CollisionStateManager","PassbyStateManager","PlayerVehicleStateManager","AIVehicleStateManager"}
free_overrides = {"StreamingStateManager","EmitterStateManager","CollisionStateManager","AIVehicleStateManager"}
update_overrides = {"GlobalStateManager","StreamingStateManager","TrafficStateManager","EmitterStateManager","CollisionStateManager","PassbyStateManager","PlayerVehicleStateManager","AIVehicleStateManager"}

def expected_symbol(class_name, offset):
    owner = class_owners[class_name]
    if offset == 0x04: return method_prefix("GetTypeInfo", owner)
    if offset == 0x08: return method_prefix("GetTypeName", owner)
    if offset == 0x0C: return method_prefix("Prepare", owner if class_name in prepare_overrides else state_owner)
    if offset == 0x14: return method_prefix("GetFreeState", owner if class_name in free_overrides else state_owner)
    if offset == 0x18: return method_prefix("UpdateParams", owner if class_name in update_overrides else state_owner)
    if offset == 0x1C: return method_prefix("ProcessUpdate", state_owner)
    if offset == 0x20:
        return method_prefix("Notify", class_owners["CollisionStateManager"] if class_name == "CollisionStateManager" else state_owner)
    if offset == 0x24: return "?IsDataLoaded@StateManager@Logic@CgsSound@@UAA_NXZ"
    if offset == 0x28:
        return "?IsStateAlias@VehicleStateManager@Vehicles@BrnSound@@UAA_NH@Z" if class_name in ("PlayerVehicleStateManager","AIVehicleStateManager") else "?IsStateAlias@StateManager@Logic@CgsSound@@UAA_NH@Z"
    raise ValueError(hex(offset))

def resolve_symbol(prefix):
    found = [e for e in entries if e["name"].startswith(prefix)]
    require(len(found) == 1, "missing/ambiguous map symbol prefix: " + prefix)
    return found[0]

table_results = []
cell_checks = []
for class_name, table_prefix in table_prefixes.items():
    found = [e for e in entries if e["name"].startswith(table_prefix) and "IResourceRequester@" not in e["name"]]
    require(len(found) == 1, "missing/ambiguous primary vtable: " + class_name)
    table = found[0]
    cells = []
    artist_table_va, artist_slots = artist_tables[class_name]
    for offset in slots:
        cell_va = table["va"] + offset
        cell = section_read(cell_va, 4)
        pointer = struct.unpack(">I", cell["bytes"])[0]
        expected = expected_symbol(class_name, offset)
        aliases = names_by_va.get(pointer, [])
        matched = [name for name in aliases if expected in name]
        target_loc = section_read(pointer, 4)
        ok = cell["section"] == ".rdata" and target_loc["section"] == ".text" and bool(matched)
        cell_checks.append(ok)
        cells.append({
            "slot_offset": hex(offset),
            "cell_va": hex(cell_va),
            "cell_rva": hex(cell["rva"]),
            "cell_section": cell["section"],
            "cell_raw_file_offset": hex(cell["file_offset"]),
            "pointer_big_endian": hex(pointer),
            "target_section": target_loc["section"],
            "target_rva": hex(target_loc["rva"]),
            "target_raw_file_offset": hex(target_loc["file_offset"]),
            "expected_symbol_prefix": expected,
            "target_symbol_alias_count": len(aliases),
            "matched_expected_symbols": matched,
            "artist_table_va": hex(artist_table_va),
            "artist_target_va": hex(artist_slots[offset]),
            "pass": ok,
        })
    table_results.append({
        "class": class_name,
        "map_symbol_va": hex(table["va"]),
        "map_symbol": table["name"],
        "map_object": table["owner"],
        "artist_table_va": hex(artist_table_va),
        "cells": cells,
    })

traffic_table = next(t for t in table_results if t["class"] == "TrafficStateManager")
traffic_va = int(traffic_table["map_symbol_va"], 16)
traffic_cell_va = traffic_va + 0x2C
traffic_cell = section_read(traffic_cell_va,4)
traffic_ptr = struct.unpack(">I",traffic_cell["bytes"])[0]
traffic_aliases = names_by_va.get(traffic_ptr,[])
traffic_expected = "?ExitGamePlay@TrafficStateManager@Traffic@Logic@BrnSound@@"
traffic_match = [n for n in traffic_aliases if traffic_expected in n]
traffic_target_loc = section_read(traffic_ptr,4)
traffic_extra = {
    "slot_offset": "0x2c",
    "cell_va": hex(traffic_cell_va),
    "cell_section": traffic_cell["section"],
    "cell_raw_file_offset": hex(traffic_cell["file_offset"]),
    "pointer_big_endian": hex(traffic_ptr),
    "target_section": traffic_target_loc["section"],
    "target_rva": hex(traffic_target_loc["rva"]),
    "target_raw_file_offset": hex(traffic_target_loc["file_offset"]),
    "expected_symbol_prefix": traffic_expected,
    "target_symbol_alias_count": len(traffic_aliases),
    "matched_expected_symbols": traffic_match,
    "artist_target_va": hex(artist_tables["TrafficStateManager"][1][0x2C]),
    "pass": traffic_cell["section"] == ".rdata" and traffic_target_loc["section"] == ".text" and bool(traffic_match),
}

base_table = next(t for t in table_results if t["class"] == "StateManager")
base_prepare_cell = next(c for c in base_table["cells"] if c["slot_offset"] == "0xc")
base_prepare_va = int(base_prepare_cell["pointer_big_endian"], 16)
base_prepare_symbol = "?Prepare@StateManager@Logic@CgsSound@@"
prepare_symbol = resolve_symbol(base_prepare_symbol)
get_free_symbol = resolve_symbol("?GetFreeState@StateManager@Logic@CgsSound@@")
prepare_bytes = section_read(base_prepare_va,20)["bytes"]
prepare_raw = section_read(base_prepare_va,24)
base_prepare = {
    "symbol": prepare_symbol["name"],
    "target_va": hex(base_prepare_va),
    "target_section": section_read(base_prepare_va,20)["section"],
    "target_rva": hex(section_read(base_prepare_va,20)["rva"]),
    "target_raw_file_offset": hex(section_read(base_prepare_va,20)["file_offset"]),
    "first_20_bytes": prepare_bytes.hex().upper(),
    "artist_expected_first_20_bytes": ARTIST_PREPARE.hex().upper(),
    "exact_first_20_byte_match": prepare_bytes == ARTIST_PREPARE,
    "next_map_symbol": get_free_symbol["name"],
    "next_map_symbol_va": hex(get_free_symbol["va"]),
    "distance_to_next_map_symbol": hex(get_free_symbol["va"] - base_prepare_va),
    "raw_24_byte_window": prepare_raw["bytes"].hex().upper(),
    "padding_after_first_20": prepare_raw["bytes"][20:].hex().upper(),
    "semantics_if_exact": "mr r11,r3; li r10,4; li r3,1; stw r10,0x24(r11); blr",
}

all_cells_pass = all(cell_checks)
status = "PASS" if all_cells_pass and traffic_extra["pass"] and base_prepare["exact_first_20_byte_match"] and signature_pass else "FAIL"
sections = []
for section in pe.sections:
    sections.append({
        "name": section.Name.rstrip(b"\0").decode("ascii",errors="replace"),
        "virtual_address_rva": hex(section.VirtualAddress),
        "virtual_size": hex(section.Misc_VirtualSize),
        "raw_pointer": hex(section.PointerToRawData),
        "raw_size": hex(section.SizeOfRawData),
    })
receipt = {
    "task": "R173_StateManager_primary_vtables_and_base_Prepare",
    "status": status,
    "head": EXPECTED_HEAD,
    "tree": EXPECTED_TREE,
    "parent": pins.get("parent"),
    "root_link_pins_path": str(PINS_PATH),
    "root_link_pins_sha256": EXPECTED_PINS_SHA,
    "artifact_sha256": pin_hashes,
    "image_base": hex(image_base),
    "pe_sections": sections,
    "selected_source_signatures": signature_checks,
    "manager_primary_vtables": table_results,
    "traffic_exit_gameplay_extra": traffic_extra,
    "base_prepare": base_prepare,
    "scope_note": "Checks only +0x04, +0x08, +0x0C, and +0x14 through +0x28 for all ten primary manager tables; additionally checks Traffic +0x2C ExitGamePlay. Release +0x10 and CRT ordering are excluded.",
    "artist_raw_sha256": ARTIST_RAW_SHA,
    "limitations": "Static linked PE/map evidence only; no runtime or launch is established.",
    "verifier_sha256": sha(Path(__file__).read_bytes()),
}
OUT_PATH.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(json.dumps(receipt, indent=2))
