from __future__ import annotations

import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

ARTIFACTS = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20")
WORKTREE = Path(r"C:\Users\adri1\.codex\worktrees\bp360\native-sound")
OUT = ARTIFACTS / "r156-physics-probes-linked"
R156 = ARTIFACTS / "r156-link"
R155 = ARTIFACTS / "r155-link"
INTAKE = ARTIFACTS / "r156-intake-v2"
SELECTED = INTAKE / "native-selected"

R156_HEAD = "8157850d32d5d15c6187b22969e939f5ca7c201d"
R156_TREE = "f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
R156_PARENT = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
R156_SEAL = "6376515b0c2ab708d8cd8e6d3ec4eeb635826fa8a7f40bbee77799ec8c43f1c4"
R156_XEX = "e1dd2fa9321932520dc561f491eb9e32f19029c6cd599a38f0a8a1302a083ed2"
R156_PE = "d75e7d429422a174df8788ba55bd979460a348ebd7f18339e65a54225a3873c8"
R156_MAP = "1dfdb006eff111042c95e4ce77de3d2c8e605c1fa748540c0c737189c0f8688e"
R155_HEAD = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
C967 = "c9676eaddb019d0b94f60942bca49c811b2d8e07"
MARKER = b"[bridge-queues] frames="

PHYSICS_PATHS = [
    "src/GameSource/Physics/BrnPhysicsModuleBridgeFunctions.cpp",
    "src/GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.cpp",
    "src/GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h",
]
BRIDGE_OBJ = SELECTED / "BrnPhysicsModuleBridgeFunctions-7b16a0a4.obj"
BRIDGE_ASM = SELECTED / "BrnPhysicsModuleBridgeFunctions-7b16a0a4.asm"
INTERFACE_OBJ = SELECTED / "BrnPhysicsModuleIO_PotentialContactInterface-24122188.obj"
INTERFACE_ASM = SELECTED / "BrnPhysicsModuleIO_PotentialContactInterface-24122188.asm"


def fail(message: str) -> None:
    raise SystemExit(f"FAIL: {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def git(*args: str) -> str:
    return subprocess.check_output(["git", "-C", str(WORKTREE), *args], text=True).strip()


def git_pin(commit: str) -> dict:
    fields = git("show", "-s", "--format=%H%x09%T%x09%P", commit).split("\t")
    return {"head": fields[0], "tree": fields[1], "parent": fields[2].split()[0] if fields[2] else ""}


def git_blob(commit: str, path: str) -> str:
    return git("rev-parse", f"{commit}:{path}")


def map_line(map_text: str, symbol: str) -> str | None:
    hits = [line for line in map_text.splitlines() if symbol in line]
    if not hits:
        return None
    if len(hits) != 1:
        fail(f"expected one map row for {symbol}, found {len(hits)}")
    return hits[0]


def map_address(line: str | None, label: str) -> int:
    if line is None:
        fail(f"missing linked map symbol: {label}")
    match = re.search(r"\b([0-9A-Fa-f]{8})\s+f\s+", line)
    if not match:
        fail(f"could not read map address for {label}: {line}")
    return int(match.group(1), 16)


def coff_info(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < 20:
        fail(f"truncated COFF object: {path}")
    machine, sections = struct.unpack_from("<HH", data, 0)
    sym_ptr, sym_count = struct.unpack_from("<II", data, 8)
    names: list[str] = []
    string_base = sym_ptr + 18 * sym_count
    if sym_count:
        if string_base + 4 > len(data):
            fail(f"bad COFF string table: {path}")
        string_size = struct.unpack_from("<I", data, string_base)[0]
        index = 0
        while index < sym_count:
            off = sym_ptr + 18 * index
            raw_name = data[off:off + 8]
            if len(raw_name) != 8:
                fail(f"truncated COFF symbol table: {path}")
            zeroes, = struct.unpack_from("<I", raw_name, 0)
            if zeroes == 0:
                string_off, = struct.unpack_from("<I", raw_name, 4)
                start = string_base + string_off
                end = data.find(b"\0", start, string_base + string_size)
                if end < 0:
                    fail(f"unterminated COFF symbol name: {path}")
                name = data[start:end].decode("ascii", errors="replace")
            else:
                name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="replace")
            names.append(name)
            aux_count = data[off + 17]
            index += 1 + aux_count
    text = data.decode("latin1")
    return {
        "path": str(path),
        "bytes": len(data),
        "sha256": sha256(path),
        "machine": f"0x{machine:04X}",
        "sections": sections,
        "symbol_count": sym_count,
        "helper_symbol_present": any("DebugAccumulateQueueLengths" in name for name in names),
        "probe_log_literal_present": MARKER.decode("ascii") in text,
        "retained_symbols": {
            "CheckContactQueueSize": any("?CheckContactQueueSize@PhysicsModule@BrnPhysics@@" in n for n in names),
            "BridgeContactsToSimulation": any("?BridgeContactsToSimulation@PhysicsModule@BrnPhysics@@" in n for n in names),
            "PotentialContactInterface_GetLength": any("?GetLength@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@" in n for n in names),
            "PotentialContactInterface_GetEvent": any("?GetEvent@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@" in n for n in names),
        },
    }


def pe_branch_target(path: Path, pc: int) -> dict:
    data = path.read_bytes()
    peoff, = struct.unpack_from("<I", data, 0x3C)
    signature = data[peoff:peoff + 4]
    if signature != b"PE\0\0":
        fail(f"invalid PE signature in {path}")
    machine, section_count = struct.unpack_from("<HH", data, peoff + 4)
    optional_size, = struct.unpack_from("<H", data, peoff + 20)
    optional = peoff + 24
    magic, = struct.unpack_from("<H", data, optional)
    image_base, = struct.unpack_from("<I", data, optional + 28)
    rva = pc - image_base
    section_table = optional + optional_size
    file_offset = None
    section_name = None
    for i in range(section_count):
        off = section_table + 40 * i
        name = data[off:off + 8].split(b"\0", 1)[0].decode("ascii", errors="replace")
        virtual_size, virtual_address, raw_size, raw_ptr = struct.unpack_from("<IIII", data, off + 8)
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            file_offset = raw_ptr + (rva - virtual_address)
            section_name = name
            break
    if file_offset is None or file_offset + 4 > len(data):
        fail(f"PE address 0x{pc:08X} is not backed by raw bytes")
    word, = struct.unpack_from(">I", data, file_offset)
    opcode = word >> 26
    aa = (word >> 1) & 1
    lk = word & 1
    displacement = word & 0x03FFFFFC
    if displacement & 0x02000000:
        displacement -= 0x04000000
    target = displacement if aa else pc + displacement
    return {
        "machine": f"0x{machine:04X}",
        "optional_magic": f"0x{magic:04X}",
        "image_base": f"0x{image_base:08X}",
        "section": section_name,
        "file_offset": f"0x{file_offset:08X}",
        "call_va": f"0x{pc:08X}",
        "instruction": f"0x{word:08X}",
        "opcode": opcode,
        "absolute_address_bit": aa,
        "link_bit": lk,
        "decoded_target_va": f"0x{target:08X}",
    }


r156_pin = git_pin(R156_HEAD)
r155_pin = git_pin(R155_HEAD)
require(r156_pin["head"] == R156_HEAD and r156_pin["tree"] == R156_TREE and r156_pin["parent"] == R156_PARENT, "R156 commit/tree/parent pin mismatch")
require(r155_pin["head"] == R155_HEAD and r155_pin["tree"] == "6bdcd7954fa913359b809b7506984f0dc05d90ee", "published R155 pin mismatch")

binding = load_json(INTAKE / "commit-binding.json")
completion = load_json(INTAKE / "completion.json")
require(binding["head"] == R156_HEAD and binding["base"] == R155_HEAD, "r156-intake-v2 commit binding mismatch")
require(completion["head"] == R156_HEAD and completion["status"] == "SEALED_NO_REGRESSIONS", "r156-intake-v2 gate status mismatch")
require(completion["requested"] == 3710 and completion["passed"] == 3710 and completion["failed"] == 0, "r156-intake-v2 compile counts mismatch")

old_compile = load_json(ARTIFACTS / "r153-physics-bridge-r153-compile" / "compile-results.json")
require(old_compile["requested"] == 2 and old_compile["passed"] == 2 and old_compile["failed"] == 0, "c967 targeted XDK compile did not pass 2/2")
require(all(row["passed"] and row["machine"] == 498 for row in old_compile["results"]), "c967 targeted objects are not passing PPC COFF")

r156_result = load_json(R156 / "result.json")
r156_inventory = load_json(R156 / "source-inventory.json")
r156_release = load_json(R156 / "ROOT_RELEASE.json")
r156_image = load_json(R156 / "image-verification.json")
r156_diagnostics = load_json(R156 / "diagnostic-parsed.json")
r156_map_text = (R156 / "native-game.map").read_text(encoding="utf-8", errors="replace")
r155_result = load_json(R155 / "result.json")
r155_inventory = load_json(R155 / "source-inventory.json")
r155_map_text = (R155 / "native-game.map").read_text(encoding="utf-8", errors="replace")
require(r156_inventory["head"] == R156_HEAD and r155_inventory["head"] == R155_HEAD, "link source inventory head mismatch")
require(r156_result["exit_code"] == 0 and r156_result["all_post_checks_pass"], "R156 link/post-link checks failed")
require(r156_release["sealed_sha256"] == R156_SEAL and sha256(R156 / "seal.json") == R156_SEAL, "R156 sealed link pin mismatch")
require(r156_image["head"] == R156_HEAD and r156_image["magic"] == "XEX2" and r156_image["image_sha256"] == R156_XEX, "R156 XEX verification mismatch")
require(r156_diagnostics["unresolved_messages"] == 0 and r156_diagnostics["unique_symbols"] == 0 and not r156_diagnostics["duplicate_messages"], "R156 link diagnostics are not clean")
require(r155_result["exit_code"] == 0 and r155_result["all_post_checks_pass"], "published R155 link receipt failed")

actual_hashes = {
    "xex": sha256(R156 / "native-game.exe"),
    "pe": sha256(R156 / "native-game.pe"),
    "map": sha256(R156 / "native-game.map"),
}
require(actual_hashes == {"xex": R156_XEX, "pe": R156_PE, "map": R156_MAP}, "R156 XEX/PE/map hash pin mismatch")

response_text = (R156 / "r156-strict.rsp").read_text(encoding="utf-8", errors="replace").lower()
for selected_object in (BRIDGE_OBJ, INTERFACE_OBJ):
    require(str(selected_object).lower() in response_text, f"selected COFF not in strict link response: {selected_object.name}")
bridge_coff = coff_info(BRIDGE_OBJ)
interface_coff = coff_info(INTERFACE_OBJ)
require(bridge_coff["machine"] == "0x01F2" and interface_coff["machine"] == "0x01F2", "selected physics COFF is not PPC machine 0x01F2")
require(not bridge_coff["helper_symbol_present"] and not interface_coff["helper_symbol_present"], "probe helper symbol remains in selected COFF")
require(not bridge_coff["probe_log_literal_present"] and not interface_coff["probe_log_literal_present"], "probe log literal remains in selected COFF")
require(all(bridge_coff["retained_symbols"].values()), "native contact bridge/check symbols missing from selected COFF")
require(interface_coff["retained_symbols"]["PotentialContactInterface_GetLength"] and interface_coff["retained_symbols"]["PotentialContactInterface_GetEvent"], "contact queue accessors missing from selected COFF")

check_symbol = "?CheckContactQueueSize@PhysicsModule@BrnPhysics@@AAAX"
bridge_symbol = "?BridgeContactsToSimulation@PhysicsModule@BrnPhysics@@AAAX"
helper_symbol = "?DebugAccumulateQueueLengths@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@"
length_symbol = "?GetLength@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@QBAHXZ"
event_s32_symbol = "?GetEvent@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@QBAABUPotentialContact@SceneManagerIO@CgsSceneManager@@H@Z"
event_id_symbol = "?GetEvent@PotentialContactInterface@PhysicsModuleIO@BrnPhysics@@QBAABUPotentialContact@SceneManagerIO@CgsSceneManager@@UContactId@3@@Z"
r156_symbols = {
    "CheckContactQueueSize": map_address(map_line(r156_map_text, check_symbol), "CheckContactQueueSize"),
    "BridgeContactsToSimulation": map_address(map_line(r156_map_text, bridge_symbol), "BridgeContactsToSimulation"),
    "PotentialContactInterface_GetLength": map_address(map_line(r156_map_text, length_symbol), "PotentialContactInterface::GetLength"),
    "PotentialContactInterface_GetEvent_s32": map_address(map_line(r156_map_text, event_s32_symbol), "PotentialContactInterface::GetEvent(s32)"),
    "PotentialContactInterface_GetEvent_ContactId": map_address(map_line(r156_map_text, event_id_symbol), "PotentialContactInterface::GetEvent(ContactId)"),
}
require(helper_symbol not in r156_map_text, "probe helper remains in final R156 linked map")
r155_symbols = {
    "CheckContactQueueSize": map_address(map_line(r155_map_text, check_symbol), "R155 CheckContactQueueSize"),
    "BridgeContactsToSimulation": map_address(map_line(r155_map_text, bridge_symbol), "R155 BridgeContactsToSimulation"),
    "DebugAccumulateQueueLengths": map_address(map_line(r155_map_text, helper_symbol), "R155 DebugAccumulateQueueLengths"),
}

bridge_asm_text = BRIDGE_ASM.read_text(encoding="utf-8", errors="replace")
call_match = re.search(r"^\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]{8}\s+bl\s+\?CheckContactQueueSize", bridge_asm_text, re.MULTILINE)
if not call_match:
    fail("selected bridge assembly has no direct CheckContactQueueSize call")
call_offset = int(call_match.group(1), 16)
call_va = r156_symbols["BridgeContactsToSimulation"] + call_offset
pe_call = pe_branch_target(R156 / "native-game.pe", call_va)
require(pe_call["machine"] == "0x01F2" and pe_call["section"] == ".text" and pe_call["link_bit"] == 1, "linked contact call is not PPC BL in .text")
require(int(pe_call["decoded_target_va"], 16) == r156_symbols["CheckContactQueueSize"], "linked PE contact call does not target CheckContactQueueSize")

r156_pe_bytes = (R156 / "native-game.pe").read_bytes()
r155_pe_bytes = (R155 / "native-game.pe").read_bytes()
r156_marker_count = r156_pe_bytes.count(MARKER)
r155_marker_count = r155_pe_bytes.count(MARKER)
require(r156_marker_count == 0 and r155_marker_count == 1, "probe PC log literal count does not match removal/baseline")
r155_marker_offset = r155_pe_bytes.find(MARKER)

source_blobs = []
for path in PHYSICS_PATHS:
    final_blob = git_blob(R156_HEAD, path)
    prior_blob = git_blob(R155_HEAD, path)
    c967_blob = git_blob(C967, path)
    source_blobs.append({
        "path": path,
        "r155_blob": prior_blob,
        "c967_blob": c967_blob,
        "r156_blob": final_blob,
        "c967_equals_r156": c967_blob == final_blob,
        "r155_differs_from_r156": prior_blob != final_blob,
    })
require(all(row["c967_equals_r156"] and row["r155_differs_from_r156"] for row in source_blobs), "physics source blobs do not match the c967 removal candidate")

stale_comments = [
    "BrnPhysicsModuleBridgeFunctions.cpp:196 (comment-only `[bridge-queues] witness above measures`)",
    "BrnVehicleManager_PerFrameLeaves.cpp:212 (comment-only `[bridge-queues] witness accumulated`)",
]

proof = {
    "schema": "r156-physics-probes-linked-v1",
    "verdict": "pass",
    "candidate": {**r156_pin, "clean_worktree_at_run": not bool(git("status", "--porcelain"))},
    "intake_gate": {
        "path": str(INTAKE / "completion.json"),
        "status": completion["status"],
        "requested": completion["requested"],
        "passed": completion["passed"],
        "failed": completion["failed"],
        "fresh": completion["fresh"],
        "retained": completion["retained"],
    },
    "prior_targeted_compile": {
        "commit": C967,
        "path": str(ARTIFACTS / "r153-physics-bridge-r153-compile" / "compile-results.json"),
        "target": old_compile["target"],
        "requested": old_compile["requested"],
        "passed": old_compile["passed"],
        "failed": old_compile["failed"],
    },
    "linked_r156": {
        "seal_sha256": sha256(R156 / "seal.json"),
        "xex_sha256": actual_hashes["xex"],
        "pe_sha256": actual_hashes["pe"],
        "map_sha256": actual_hashes["map"],
        "link_exit_code": r156_result["exit_code"],
        "all_post_checks_pass": r156_result["all_post_checks_pass"],
        "unresolved_messages": r156_diagnostics["unresolved_messages"],
        "duplicate_messages": len(r156_diagnostics["duplicate_messages"]),
    },
    "published_r155_baseline": {
        **r155_pin,
        "seal_sha256": sha256(R155 / "seal.json"),
        "pe_sha256": sha256(R155 / "native-game.pe"),
        "map_sha256": sha256(R155 / "native-game.map"),
    },
    "selected_physics_coff": {
        "strict_rsp": str(R156 / "r156-strict.rsp"),
        "bridge": bridge_coff,
        "potential_contact_interface": interface_coff,
        "bridge_assembly_sha256": sha256(BRIDGE_ASM),
        "interface_assembly_sha256": sha256(INTERFACE_ASM),
    },
    "linked_symbols": {
        "r155": {key: f"0x{value:08X}" for key, value in r155_symbols.items()},
        "r156": {key: f"0x{value:08X}" for key, value in r156_symbols.items()},
        "r156_helper_absent": True,
    },
    "native_contact_call": {
        "selected_bridge_asm_call_offset": f"0x{call_offset:X}",
        "linked_pe_instruction": pe_call,
        "targets_linked_CheckContactQueueSize": True,
    },
    "probe_log_literal": {
        "literal": MARKER.decode("ascii"),
        "r155_pe_occurrences": r155_marker_count,
        "r155_pe_file_offset": r155_marker_offset,
        "r156_selected_coff_occurrences": 0,
        "r156_pe_occurrences": r156_marker_count,
    },
    "source_blob_binding": source_blobs,
    "stale_comment_only_mentions": stale_comments,
    "scope": "Static linked-image proof only; no game launch or runtime validation was performed.",
}

out_path = OUT / "R156_PHYSICS_PROBES_LINKED.json"
out_path.write_text(json.dumps(proof, indent=2) + "\n", encoding="utf-8")
print(json.dumps(proof, indent=2))
