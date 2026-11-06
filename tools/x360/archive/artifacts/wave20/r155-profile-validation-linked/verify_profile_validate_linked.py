#!/usr/bin/env python3
"""Verify the R155 linked Profile::ValidateProfile diagnostic removal against R154 and ARTIST."""
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

import capstone

OUT = Path(__file__).resolve().parent
WAVE = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave20")
REPO = Path(r"C:\Users\adri1\.codex\worktrees\bp360\native_resources")
ARTIST = WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex"
R154 = WAVE / "r154-link"
R155 = WAVE / "r155-link"
R155_OBJECT_DIR = WAVE / "r155-intake/native-selected"
R155_HEAD = "1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1"
R155_TREE = "6bdcd7954fa913359b809b7506984f0dc05d90ee"
R155_FROZEN = "68208920c4ac83d0759649576cb2d0516cb57bf8"
R155_DONOR = "4cbc4a2ef9e519d380e95c2b6bb72d9d5c10ed6e"
R154_HEAD = "13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d"
ARTIST_SHA = "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
R154_XEX_SHA = "d477284ed9ff370e673fb67b0f848d9cc1d19125eb21e80cd53e6662c53885d1"
R154_PE_SHA = "8895808adbacbd4c6847d3c4f5322e5ce6421250ba8f02ddbb75942208669c45"
R154_MAP_SHA = "85ea268fb1c72e005910f97c36b9347be5c8fb545539e285c3700d471c393013"
R155_SEAL_SHA = "a969af86b6beeb39371dfbb536a4ba7eae109d541e5fef8868f96fdd5e27a28f"
R155_XEX_SHA = "dc82daf3cb18c67831c2ba76b2e0fcbc589a16d8584c9418e7a6400d61558406"
R155_PE_SHA = "c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510"
R155_MAP_SHA = "62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a"
PROFILE = "?ValidateProfile@Profile@BrnGuiSaveLoad@@QBA_NPBUProgressionData@BrnProgression@@@Z"
PROFILE_DLC1 = "?ValidateProfile@ProfileDLC1@BrnGuiSaveLoad@@QAA_NXZ"
VALIDATE_PROFILES = "?ValidateProfiles@ProfileManager@BrnGui@@AAA_NXZ"
REPORT_TASK = "?ReportTaskCompleted@ProfileManager@BrnGui@@AAAXXZ"
DESERIALISE = "?Deserialise@Profile@BrnProgression@@QAAXPBV1BrnGuiSaveLoad@@PBUProfileUpgradeTable@2@PBVProfileDLC1@3@@Z"
VERSION_TEXT = b"Progression Profile version mismatch, expected "
REMOVED_TEXTS = [b"[profile-save] ValidateProfile:", b"storedEventCount", b"expectedJunctions"]
AGGREGATE_TEXT = b"[profile-save] ValidateProfiles:"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git_text(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def git_blob(commit):
    return git_text("rev-parse", f"{commit}:src/GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfile.cpp")


def link_pin(folder, expected_head, expected_tree, expected_xex, expected_pe, expected_map, expected_seal=None):
    folder = Path(folder)
    verify = json.loads((folder / "image-verification.json").read_text())
    result = json.loads((folder / "result.json").read_text())
    seal_path = folder / "seal.json"
    seal = json.loads(seal_path.read_text())
    assert verify["head"] == expected_head, (folder, verify["head"])
    assert seal["head"] == expected_head, (folder, seal["head"])
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    assert sha(folder / "native-game.exe") == expected_xex
    assert verify["image_sha256"] == expected_xex
    output_rows = result["outputs"]
    pe_path = str((folder / "native-game.pe").resolve())
    map_path = str((folder / "native-game.map").resolve())
    assert output_rows[pe_path]["sha256"] == expected_pe
    assert output_rows[map_path]["sha256"] == expected_map
    assert sha(folder / "native-game.pe") == expected_pe
    assert sha(folder / "native-game.map") == expected_map
    actual_tree = git_text("show", "-s", "--format=%T", expected_head)
    assert actual_tree == expected_tree, (expected_head, actual_tree, expected_tree)
    seal_hash = sha(seal_path)
    if expected_seal:
        assert seal_hash == expected_seal, (seal_hash, expected_seal)
    return {
        "head": expected_head,
        "tree": expected_tree,
        "xex_sha256": expected_xex,
        "pe_sha256": expected_pe,
        "map_sha256": expected_map,
        "seal_sha256": seal_hash,
        "exit_code": result["exit_code"],
        "all_post_checks_pass": result["all_post_checks_pass"],
    }


class LinkedPE:
    def __init__(self, folder):
        self.folder = Path(folder)
        self.mapping = (self.folder / "native-game.map").read_text(errors="replace")
        self.data = (self.folder / "native-game.pe").read_bytes()
        peoff = struct.unpack_from("<I", self.data, 0x3C)[0]
        assert self.data[peoff:peoff + 4] == b"PE\0\0"
        self.machine, count, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", self.data, peoff + 4)
        self.base = struct.unpack_from("<I", self.data, peoff + 24 + 28)[0]
        assert self.machine == 0x01F2 and self.base == 0x82000000
        self.sections = [struct.unpack_from("<8sIIII", self.data, peoff + 24 + opt_size + i * 40)[1:]
                         for i in range(count)]

    def address(self, symbol):
        matches = re.findall(rf"(?m)^\s+[0-9a-fA-F]{{4}}:[0-9a-fA-F]+\s+{re.escape(symbol)}\s+([0-9a-fA-F]{{8}})\s+f\s+([^\r\n]+)", self.mapping)
        assert len(matches) == 1, (symbol, matches)
        return int(matches[0][0], 16), matches[0][1].strip()

    def read(self, va, size):
        rva = va - self.base
        for virtual_size, virtual_address, raw_size, raw_offset in self.sections:
            if virtual_address <= rva and rva + size <= virtual_address + raw_size:
                return self.data[raw_offset + rva - virtual_address:raw_offset + rva - virtual_address + size]
        raise ValueError(f"unmapped VA {va:#x} size {size:#x}")

    def map_at(self, address):
        rows = re.findall(rf"(?m)^\s+([0-9a-fA-F]{{4}}):[0-9a-fA-F]+\s+(\S.*?)\s+{address:08x}\s+f\s+([^\r\n]+)", self.mapping)
        return [{"segment": row[0], "symbol": row[1].strip(), "object": row[2].strip()} for row in rows]


MD = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)


def disasm(data, start):
    return list(MD.disasm(data, start))


def insn_at(instructions, address):
    rows = [i for i in instructions if i.address == address]
    assert len(rows) == 1, (hex(address), rows)
    i = rows[0]
    return {"address": hex(i.address), "mnemonic": i.mnemonic, "operands": i.op_str}


def target(insn):
    m = re.search(r"0x([0-9a-fA-F]+)", insn.op_str)
    return int(m.group(1), 16) if m else None


def call_sites(instructions, destination):
    return [hex(i.address) for i in instructions if i.mnemonic == "bl" and target(i) == destination]


def branch_sites(instructions, mnemonic, destination):
    return [hex(i.address) for i in instructions if i.mnemonic == mnemonic and target(i) == destination]


# Bind final link output pins, source blob lineage, and exact selected object.
r154_pin = link_pin(R154, R154_HEAD, "ae764bcba446118b5bc8bda12ac461f5ede4a9a5", R154_XEX_SHA, R154_PE_SHA, R154_MAP_SHA)
r155_pin = link_pin(R155, R155_HEAD, R155_TREE, R155_XEX_SHA, R155_PE_SHA, R155_MAP_SHA, R155_SEAL_SHA)
assert git_text("show", "-s", "--format=%T", R155_FROZEN) == R155_TREE
assert git_blob(R155_DONOR) == "8c1981a3c1677a16c876b64ba3c6ef2217f3df1c"
assert git_blob(R155_HEAD) == git_blob(R155_FROZEN) == git_blob(R155_DONOR)
assert git_blob(R154_HEAD) == "64e7a9b56db900446c242eeaa8ffa27fc3c5b600"

# Check R155's selected object is the one used by the linked function.
r155_invocation = json.loads((R155 / "invocation.json").read_text())
profile_row = next(row for row in r155_invocation["objects"]
                   if row["source"].replace("\\", "/").endswith("BrnGuiSaveLoadProfile.cpp"))
r155_obj = Path(profile_row["object"])
assert r155_obj.exists() and sha(r155_obj) == profile_row["sha256"] == "faf37e8c5bc5029ee5bd4b7a2f2aff4586e09c3a2223d834dfdbe259ea48afe5"
obj_bytes = r155_obj.read_bytes()
assert struct.unpack_from("<H", obj_bytes)[0] == 0x01F2
obj_asm = r155_obj.with_suffix(".asm").read_text(errors="replace")
for removed in REMOVED_TEXTS:
    assert removed.decode() not in obj_asm
assert VERSION_TEXT.decode() in obj_asm

# Bind R154 and R155 exact linked functions through each map and PE image.
r154_pe = LinkedPE(R154)
r155_pe = LinkedPE(R155)
r154_start, r154_object = r154_pe.address(PROFILE)
r154_next, _ = r154_pe.address(PROFILE_DLC1)
r155_start, r155_object = r155_pe.address(PROFILE)
r155_next, _ = r155_pe.address(PROFILE_DLC1)
assert r154_start == 0x824EFE40 and r155_start == 0x824EFE08
assert Path(r154_object).name == "BrnGuiSaveLoadProfile-a15e1d30.obj"
assert Path(r155_object).name == "BrnGuiSaveLoadProfile-a15e1d30.obj"
assert r155_pe.map_at(r155_start)[0]["object"] == "BrnGuiSaveLoadProfile-a15e1d30.obj"

r154_profile = disasm(r154_pe.read(r154_start, r154_next - r154_start), r154_start)
r155_profile = disasm(r155_pe.read(r155_start, r155_next - r155_start), r155_start)
# R154 control: extra debug-print branch after GetEventJunctionCount and before count compare.
assert insn_at(r154_profile, 0x824EFECC)["mnemonic"] == "bl"  # GetEventJunctionCount
assert insn_at(r154_profile, 0x824EFEDC)["mnemonic"] == "cmplwi"  # gpDebugPrint non-null guard
assert target(next(i for i in r154_profile if i.address == 0x824EFEE0)) == 0x824EFF38
assert insn_at(r154_profile, 0x824EFF38)["mnemonic"] == "cmpw"  # count compare follows the log
# R155: original version mismatch logger remains, then the count compare follows the count call directly.
assert insn_at(r155_profile, 0x824EFE20)["mnemonic"] == "cmpwi" and "0x1c" in insn_at(r155_profile, 0x824EFE20)["operands"]
assert insn_at(r155_profile, 0x824EFE2C)["mnemonic"] == "ld"  # message-filter gate
assert insn_at(r155_profile, 0x824EFE58)["mnemonic"] == "lwz"  # gpDebugPrint in version-mismatch arm
assert insn_at(r155_profile, 0x824EFE94)["mnemonic"] == "bl"  # expected-count provider
assert insn_at(r155_profile, 0x824EFE98)["mnemonic"] == "mr"
assert insn_at(r155_profile, 0x824EFE9C)["mnemonic"] == "cmpw"
assert insn_at(r155_profile, 0x824EFEA0)["mnemonic"] == "bne" and target(next(i for i in r155_profile if i.address == 0x824EFEA0)) == 0x824EFE80
assert insn_at(r155_profile, 0x824EFEA4)["mnemonic"] == "cmpwi"
assert insn_at(r155_profile, 0x824EFEA8)["mnemonic"] == "ble" and target(next(i for i in r155_profile if i.address == 0x824EFEA8)) == 0x824EFF28
assert insn_at(r155_profile, 0x824EFEB0)["mnemonic"] == "addi" and "0x7070" in insn_at(r155_profile, 0x824EFEB0)["operands"]
assert insn_at(r155_profile, 0x824EFF0C)["mnemonic"] == "lwz" and "4(r3)" in insn_at(r155_profile, 0x824EFF0C)["operands"]
assert insn_at(r155_profile, 0x824EFF10)["mnemonic"] == "cmpwi"
assert insn_at(r155_profile, 0x824EFF14)["mnemonic"] == "beq" and target(next(i for i in r155_profile if i.address == 0x824EFF14)) == 0x824EFE80
assert insn_at(r155_profile, 0x824EFF1C)["mnemonic"] == "addi" and "8" in insn_at(r155_profile, 0x824EFF1C)["operands"]
assert insn_at(r155_profile, 0x824EFF28)["mnemonic"] == "li" and "r3, 1" in insn_at(r155_profile, 0x824EFF28)["operands"]
assert call_sites(r155_profile, 0x8274B8F8) == ["0x824efe94"]
assert call_sites(r155_profile, 0x827C1820) == ["0x824efecc", "0x824efef0", "0x824eff08"]
assert call_sites(r155_profile, 0x821A69A8) == ["0x824efe5c", "0x824efe6c", "0x824efe7c"]
assert call_sites(r155_profile, 0x821A6D00) == ["0x824efe64", "0x824efe74"]

# Confirm the three per-profile text fragments were in the R154 binary and are absent in R155.
r154_data = r154_pe.data
r155_data = r155_pe.data
r154_string_offsets = {x.decode(): r154_data.find(x) for x in REMOVED_TEXTS}
r155_string_offsets = {x.decode(): r155_data.find(x) for x in REMOVED_TEXTS}
assert all(offset >= 0 for offset in r154_string_offsets.values()), r154_string_offsets
assert all(offset == -1 for offset in r155_string_offsets.values()), r155_string_offsets
assert VERSION_TEXT in r154_data and VERSION_TEXT in r155_data
assert AGGREGATE_TEXT in r155_data  # separate aggregate diagnostic remains in its own owner

# Original ARTIST bytes, checked against the exact raw witness and the read-only image reader.
artist_data = ARTIST.read_bytes()
assert sha(ARTIST) == ARTIST_SHA
payload = struct.unpack_from(">I", artist_data, 8)[0]
assert artist_data[payload:payload + 2] == b"MZ"
def artist_read(va, size):
    raw = artist_data[payload + va - 0x82000000:payload + va - 0x82000000 + size]
    assert len(raw) == size
    return raw
artist_profile_start = 0x824EFE30
artist_profile = disasm(artist_read(artist_profile_start, 0x100), artist_profile_start)
assert insn_at(artist_profile, 0x824EFE40)["mnemonic"] == "cmpwi"
assert insn_at(artist_profile, 0x824EFEB4)["mnemonic"] == "cmplw"
assert insn_at(artist_profile, 0x824EFEB8)["mnemonic"] == "bne"
assert insn_at(artist_profile, 0x824EFEC0)["mnemonic"] == "cmpwi"
assert insn_at(artist_profile, 0x824EFEC4)["mnemonic"] == "ble" and target(next(i for i in artist_profile if i.address == 0x824EFEC4)) == 0x824EFF24
assert insn_at(artist_profile, 0x824EFF08)["mnemonic"] == "lwz" and "4(r11)" in insn_at(artist_profile, 0x824EFF08)["operands"]
assert insn_at(artist_profile, 0x824EFF18)["mnemonic"] == "addi" and "8" in insn_at(artist_profile, 0x824EFF18)["operands"]
for removed in REMOVED_TEXTS:
    assert removed not in artist_data
assert VERSION_TEXT in artist_data

# Caller gates: original ARTIST and R155 both skip deserialization on failed validation.
artist_validate_profiles = disasm(artist_read(0x825073A0, 0xA0), 0x825073A0)
artist_report = disasm(artist_read(0x82513EC0, 0x140), 0x82513EC0)
assert call_sites(artist_validate_profiles, 0x824EFE30) == ["0x825073c8"]
assert call_sites(artist_report, 0x825073A0) == ["0x82513f20"]
assert insn_at(artist_report, 0x82513F28)["mnemonic"] == "cmplwi"
assert insn_at(artist_report, 0x82513F2C)["mnemonic"] == "bne" and target(next(i for i in artist_report if i.address == 0x82513F2C)) == 0x82514110
assert call_sites(artist_report, 0x8237D308) == ["0x82513f90"]

r155_validate_profiles_start, _ = r155_pe.address(VALIDATE_PROFILES)
r155_report_start, _ = r155_pe.address(REPORT_TASK)
r155_deserialise, _ = r155_pe.address(DESERIALISE)
r155_validate_profiles = disasm(r155_pe.read(r155_validate_profiles_start, 0x200), r155_validate_profiles_start)
r155_report = disasm(r155_pe.read(r155_report_start, 0x340), r155_report_start)
assert r155_validate_profiles_start == 0x8242F260
assert r155_report_start == 0x8242FDA8
assert r155_deserialise == 0x823DCEE8
assert call_sites(r155_validate_profiles, r155_start) == ["0x8242f280"]
validator_targets = [r155_start, 0x824EFF58, 0x8242B220, 0x8242C938, 0x8242ED30]
assert all(len(call_sites(r155_validate_profiles, dest)) == 1 for dest in validator_targets)
assert call_sites(r155_report, r155_validate_profiles_start) == ["0x8242fe08"]
assert insn_at(r155_report, 0x8242FE10)["mnemonic"] == "cmplwi"
assert insn_at(r155_report, 0x8242FE14)["mnemonic"] == "beq" and target(next(i for i in r155_report if i.address == 0x8242FE14)) == 0x8242FFAC
assert call_sites(r155_report, r155_deserialise) == ["0x8242fe34"]

# Save a compact, auditable result plus the exact selected PPC instruction windows.
def asmlines(instructions):
    return "\n".join(f"{i.address:08X}: {i.mnemonic:8} {i.op_str}" for i in instructions)

artist_caller_keys = {0x825073C8, 0x82513F20, 0x82513F24, 0x82513F28, 0x82513F2C, 0x82513F90}
r155_caller_keys = {0x8242F280, 0x8242FE08, 0x8242FE0C, 0x8242FE10, 0x8242FE14, 0x8242FE34}
witness_text = [
    "ARTIST Profile::ValidateProfile @ 0x824EFE30 (exact raw XEX witness)", asmlines(artist_profile),
    "", "ARTIST callers (selected instructions)", asmlines([i for i in artist_validate_profiles + artist_report if i.address in artist_caller_keys]),
    "", "R154 linked Profile::ValidateProfile (pre-fix final PE)", asmlines(r154_profile),
    "", "R155 linked Profile::ValidateProfile (actual final PE)", asmlines(r155_profile),
    "", "R155 callers and deserialization gate (selected instructions)", asmlines([i for i in r155_validate_profiles + r155_report if i.address in r155_caller_keys]),
    "",
]
(OUT / "PPC_WITNESSES.txt").write_text("\n".join(witness_text), encoding="utf-8")

report = {
    "result": "PASS",
    "scope": "direct linked proof for BrnGuiSaveLoad::Profile::ValidateProfile count diagnostic removal",
    "r155_link": r155_pin,
    "r155_frozen_source": {"head": R155_FROZEN, "tree": R155_TREE, "same_tree_as_link_head": True},
    "r154_pre_fix_link": r154_pin,
    "artist_raw": {
        "path": str(ARTIST), "sha256": ARTIST_SHA,
        "validate_profile_va": hex(artist_profile_start),
        "validate_profiles_va": "0x825073A0", "report_task_completed_va": "0x82513EC0",
        "count_offsets": {"saved_manifest_count": "+0x268", "progression_count": "+0x1C"},
        "empty_count_result": "true when saved count <= 0 after the counts match",
        "manifest_start": "+0x7070", "manifest_stride": 8,
        "progression_junction_pointer": "+0x18", "junction_stride": 16,
        "matched_junction_required_field": "+0x04 must be nonzero",
        "removed_count_diagnostic_strings_absent": True,
        "version_mismatch_log_present": True,
        "caller_calls": {"ValidateProfiles_to_Profile_ValidateProfile": "0x825073C8", "ReportTaskCompleted_to_ValidateProfiles": "0x82513F20", "deserialize_call_after_true_validation": "0x82513F90", "failed_validation_branch": "0x82513F2C -> 0x82514110"},
    },
    "source": {
        "r154_blob": git_blob(R154_HEAD),
        "r155_blob": git_blob(R155_HEAD),
        "donor_4cbc_blob": git_blob(R155_DONOR),
        "r155_blob_matches_donor": True,
    },
    "r154_selected_profile_object": {"path": "D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r15-full-20261103/full-native/BrnGuiSaveLoadProfile-a15e1d30.obj", "sha256": "84612f01b4ae0152135babbe00539253b9c46252839e8125bc0cdacf84a92fd8"},
    "r154_linked_profile": {
        "va": hex(r154_start), "selected_object": r154_object,
        "diagnostic_text_offsets": r154_string_offsets,
        "count_log_executes_before_compare": True,
        "count_compare_va": "0x824EFF38",
        "version_mismatch_log_present": True,
    },
    "r155_selected_profile_object": {
        "path": profile_row["object"], "sha256": profile_row["sha256"], "machine": "0x01F2",
        "map_object_matches_selected_object": True,
        "diagnostic_text_absent_from_object_and_listing": True,
        "version_mismatch_text_present": True,
    },
    "r155_linked_profile": {
        "va": hex(r155_start), "selected_object": r155_object,
        "diagnostic_text_offsets": r155_string_offsets,
        "version_mismatch_text_present": True,
        "version_mismatch_log_code_present": True,
        "count_getter_va": "0x824EFE94", "count_compare_va": "0x824EFE9C",
        "count_mismatch_false_branch_va": "0x824EFEA0", "empty_count_true_branch_va": "0x824EFEA8",
        "manifest_base_va": "0x824EFEB0", "offline_event_test_va": "0x824EFF0C",
        "count_diagnostic_block_absent": True,
        "separate_aggregate_ValidateProfiles_log_remains": AGGREGATE_TEXT in r155_data,
    },
    "r155_caller_gate": {
        "validate_profiles_va": hex(r155_validate_profiles_start),
        "profile_validate_call": "0x8242F280 -> 0x824EFE08",
        "other_validator_calls_also_run": [hex(x) for x in validator_targets[1:]],
        "report_task_completed_va": hex(r155_report_start),
        "validate_profiles_call": "0x8242FE08 -> 0x8242F260",
        "failed_validation_branch": "0x8242FE14 -> 0x8242FFAC",
        "deserialise_call_only_on_success_path": "0x8242FE34 -> 0x823DCEE8",
        "deserialise_symbol": DESERIALISE,
    },
    "limitations": ["Static source, object, map, PE, and raw-XEX proof only; no runtime was run."],
    "verifier_sha256": sha(__file__),
}
json_path = OUT / "R155_PROFILE_VALIDATE_LINKED.json"
json_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

md = f"""# R155 Profile::ValidateProfile linked proof

## Pins

- R155 link head `{R155_HEAD}`, tree `{R155_TREE}`, seal SHA-256 `{r155_pin['seal_sha256']}`.
- R155 XEX SHA-256 `{R155_XEX_SHA}`; PE SHA-256 `{R155_PE_SHA}`; map SHA-256 `{R155_MAP_SHA}`.
- R154 pre-fix XEX SHA-256 `{R154_XEX_SHA}`; PE SHA-256 `{R154_PE_SHA}`; map SHA-256 `{R154_MAP_SHA}`.
- ARTIST raw XEX SHA-256 `{ARTIST_SHA}`.
- R155 source blob `{git_blob(R155_HEAD)}` is byte-identical to donor `4cbc4a2e` blob `{git_blob(R155_DONOR)}`.

## Direct linked result

R154 linked `Profile::ValidateProfile` is at `{hex(r154_start)}` and retains the singular `[profile-save] ValidateProfile:` / `storedEventCount` / `expectedJunctions` strings. Its code checks `gpDebugPrint` and executes the count log before its count comparison at `0x824EFF38`.

R155 links the newly selected object `{profile_row['object']}` (SHA-256 `{profile_row['sha256']}`, PPC COFF `0x01F2`) as `Profile::ValidateProfile` at `{hex(r155_start)}`. All three singular count-diagnostic strings are absent from both the linked PE and selected object/listing. The original version mismatch message remains in the R155 PE, object and version-gated code. The separate `[profile-save] ValidateProfiles:` aggregate log remains in its other owner.

R155 validation instructions compare saved count `this+0x268` to `GetEventJunctionCount()`, branch false on mismatch (`0x824EFEA0`), accept nonpositive count (`0x824EFEA8`), scan stored manifest IDs from `this+0x7070` in 8-byte steps against progression junctions, and reject a matched junction whose `+4` offline-event field is zero (`0x824EFF0C–0x824EFF14`). The ARTIST raw body at `0x824EFE30` shows the same outcomes and field strides.

## Caller gate

ARTIST calls `ValidateProfile` from `ValidateProfiles` at `0x825073C8`; `ReportTaskCompleted` calls `ValidateProfiles` at `0x82513F20`, branches away on false at `0x82513F2C`, and calls `Deserialise` only on the success path at `0x82513F90`.

R155 calls the linked validator at `0x8242F280`; `ReportTaskCompleted` calls `ValidateProfiles` at `0x8242FE08` and branches to the skip path at `0x8242FE14` on failure. The `Deserialise` call is at `0x8242FE34` only on the success path. The five validator terms are still all invoked and ANDed.

`PPC_WITNESSES.txt` contains the decoded original, R154 and R155 instructions. `R155_PROFILE_VALIDATE_LINKED.json` contains the full hashes, strings, call targets and assertions. No source, save, runtime or link operation was performed by this verifier.
"""
(OUT / "REPORT.md").write_text(md, encoding="utf-8")
print(json.dumps({"result": report["result"], "json": str(json_path), "json_sha256": sha(json_path), "report_sha256": sha(OUT / "REPORT.md"), "witness_sha256": sha(OUT / "PPC_WITNESSES.txt"), "verifier_sha256": report["verifier_sha256"]}, indent=2))