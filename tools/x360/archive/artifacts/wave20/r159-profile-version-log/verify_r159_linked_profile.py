import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, CS_MODE_32

ROOT_PINS_SHA = "0079e9f5142a38ce932b2e0197bf3ce1c2fb1eb01745b98648342ddb48154b8e"
INTEGRATED_HEAD = "2fcdc0e829479059db82e0973d28db450c204869"
INTEGRATED_TREE = "213435d590050d1fadd612a2e923ce1b58720e0a"
DONOR_HEAD = "ce69aa8c092e532638e8c2bc40bc4b4e2e35feac"
DONOR_TREE = "2003a9fe279b233061ec3b61eba3d8e45490e898"
HEADER = "src/GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"
SYMBOLS = {
    "validator": "?ValidateProfile@LiveRevengeProfile@BrnNetwork@@QAA_NXZ",
    "validate_profiles": "?ValidateProfiles@ProfileManager@BrnGui@@AAA_NXZ",
    "report_task": "?ReportTaskCompleted@ProfileManager@BrnGui@@AAAXXZ",
    "loaded_event": "?HandleLiveRevengeProfileLoadedEvent@LiveRevengeManager@BrnNetwork@@QAAXPBULiveRevengeProfile@2@@Z",
    "operator_s32": "??6StrStreamBase@CgsDev@@QAAAAU01@H@Z",
    "operator_text": "??6StrStreamBase@CgsDev@@QAAAAU01@PBD@Z",
    "gp_debug_print": "?gpDebugPrint@Log@CgsDev@@3PAUStrStreamBase@2@A",
    "message_filter": "?gxMessageFilterFlags@Message@CgsDev@@3_KA",
    "deserialise": "?Deserialise@Profile@BrnProgression@@QAAXPBV1BrnGuiSaveLoad@@PBUProfileUpgradeTable@2@PBVProfileDLC1@3@@Z",
    "relationship_validate": "?Validate@LiveRevengeRelationship@BrnNetwork@@QBA_NXZ",
}

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()

def parse_map(path):
    out = []
    row = re.compile(r"^\s*([0-9A-Fa-f]{4}):([0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{8})\s+(.*)$")
    for line in Path(path).read_text(errors="replace").splitlines():
        m = row.match(line)
        if not m:
            continue
        seg, off, name, va, tail = m.groups()
        tags = tail.split()
        out.append({
            "section": int(seg, 16),
            "offset": int(off, 16),
            "symbol": name,
            "va": int(va, 16),
            "function": "f" in tags[:3],
            "flags": tags[:3],
            "object": next((t for t in tags if t.lower().endswith(".obj")), None),
        })
    return out

def find_symbol(rows, name):
    matches = [r for r in rows if r["symbol"] == name]
    if not matches:
        raise RuntimeError("map symbol missing: " + name)
    return matches[0]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pins", required=True)
    ap.add_argument("--evidence-dir", required=True)
    ap.add_argument("--composed-repo", default=r"C:\Users\adri1\.codex\worktrees\bp360\debug_format_luna")
    ap.add_argument("--donor-repo", default=r"C:\Users\adri1\.codex\worktrees\bp360\ui_cpp_luna")
    args = ap.parse_args()
    ev = Path(args.evidence_dir)
    pins_path = Path(args.pins)
    if sha(pins_path) != ROOT_PINS_SHA:
        raise RuntimeError("ROOT_LINK_PINS.json hash mismatch")
    pins = json.loads(pins_path.read_text())
    if pins["head"] != INTEGRATED_HEAD or pins["tree"] != INTEGRATED_TREE:
        raise RuntimeError("integrated source pin mismatch")
    if pins["parent"] != "49484358f743078512f861ab783ee194c998744b":
        raise RuntimeError("integrated parent mismatch")
    for name, entry in pins["pins"].items():
        actual = sha(entry["path"])
        if actual != entry["sha256"]:
            raise RuntimeError(f"{name} hash mismatch: {actual}")
    seal = json.loads(Path(pins["pins"]["seal"]["path"]).read_text())
    result = json.loads(Path(pins["pins"]["result"]["path"]).read_text())
    if seal.get("head") != INTEGRATED_HEAD:
        raise RuntimeError("seal head mismatch")
    if result.get("exit_code") != 0 or result.get("all_post_checks_pass") is not True:
        raise RuntimeError("link result did not pass")
    composed_blob = git(args.composed_repo, "rev-parse", f"{INTEGRATED_HEAD}:{HEADER}")
    donor_blob = git(args.donor_repo, "rev-parse", f"{DONOR_HEAD}:{HEADER}")
    if composed_blob != donor_blob:
        raise RuntimeError("composed validator header differs from frozen R159 donor")

    pe_path = Path(pins["pins"]["pe"]["path"])
    map_path = Path(pins["pins"]["map"]["path"])
    xex_path = Path(pins["pins"]["xex"]["path"])
    pe = pefile.PE(str(pe_path), fast_load=True)
    if pe.FILE_HEADER.Machine != 0x01F2:
        raise RuntimeError(f"unexpected PE machine: {pe.FILE_HEADER.Machine:#x}")
    image_base = pe.OPTIONAL_HEADER.ImageBase
    if image_base != 0x82000000:
        raise RuntimeError(f"unexpected PE image base: {image_base:#x}")
    xex_magic = xex_path.read_bytes()[:4]
    if xex_magic != b"XEX2":
        raise RuntimeError(f"unexpected XEX magic: {xex_magic!r}")

    rows = parse_map(map_path)
    located = {key: find_symbol(rows, name) for key, name in SYMBOLS.items()}
    def fn_extent(sym):
        nexts = [r["va"] for r in rows if r["function"] and r["section"] == sym["section"] and r["va"] > sym["va"]]
        return min(max(0x40, (min(nexts) - sym["va"]) if nexts else 0x600), 0x1000)
    md = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN | CS_MODE_32)
    md.detail = True
    def disasm(sym, max_bytes=None):
        size = min(fn_extent(sym), max_bytes) if max_bytes else fn_extent(sym)
        rva = sym["va"] - image_base
        offset = pe.get_offset_from_rva(rva)
        data = pe.__data__[offset:offset + size]
        insns = list(md.disasm(data, sym["va"]))
        return insns
    funcs = {k: disasm(located[k]) for k in ("validator", "validate_profiles", "report_task", "loaded_event")}
    image_bytes = pe.__data__
    strings = {}
    for label, value in {
        "mismatch": b"Live Revenge Profile version mismatch, expected ",
        "comma_got": b", got ",
        "newline": b"\n",
    }.items():
        needle = value + b"\0"
        found = None
        for section in pe.sections:
            section_name = section.Name.rstrip(b"\0")
            if section_name not in (b".rdata", b".data"):
                continue
            start = section.PointerToRawData
            chunk = image_bytes[start:start + section.SizeOfRawData]
            rel = chunk.find(needle)
            if rel >= 0:
                found = (start + rel, image_base + section.VirtualAddress + rel)
                break
        strings[label] = {"present": found is not None, "file_offset": found[0] if found else -1, "va": f"0x{found[1]:08X}" if found else None}

    def at(insns, address):
        for insn in insns:
            if insn.address == address:
                return insn
        raise RuntimeError(f"instruction missing at 0x{address:08X}")

    def target(insn):
        operand = insn.op_str.split(",")[-1].strip()
        return int(operand, 0)

    def split_ops(insn):
        return [x.strip() for x in insn.op_str.split(",")]

    def pair_address(lis_address, addi_address):
        lis_insn = at(funcs["validator"], lis_address)
        addi_insn = at(funcs["validator"], addi_address)
        hi = split_ops(lis_insn)
        lo = split_ops(addi_insn)
        if lis_insn.mnemonic != "lis" or addi_insn.mnemonic != "addi":
            raise RuntimeError("expected lis/addi relocation pair missing")
        if hi[0] != lo[1]:
            raise RuntimeError("lis/addi relocation register mismatch")
        return (((int(hi[1], 0) & 0xFFFF) << 16) + int(lo[2], 0)) & 0xFFFFFFFF

    def va_to_file_offset(va):
        rva = va - image_base
        for section in pe.sections:
            start = section.VirtualAddress
            end = start + section.SizeOfRawData
            if start <= rva < end:
                return section.PointerToRawData + (rva - start)
        raise RuntimeError(f"VA outside raw-backed PE sections: 0x{va:08X}")

    def require_literal(va, expected):
        off = va_to_file_offset(va)
        actual = pe.__data__[off:off + len(expected)]
        if actual != expected:
            raise RuntimeError(f"literal mismatch at 0x{va:08X}: {actual!r}")
        return {"va": f"0x{va:08X}", "file_offset": off, "bytes": actual.hex(), "text": expected[:-1].decode("ascii", errors="replace")}

    validator = funcs["validator"]
    validate_profiles = funcs["validate_profiles"]
    report_task = funcs["report_task"]
    loaded_event = funcs["loaded_event"]
    validator_sym = located["validator"]
    op_text_va = located["operator_text"]["va"]
    op_s32_va = located["operator_s32"]["va"]
    gp_va = located["gp_debug_print"]["va"]
    filter_va = located["message_filter"]["va"]

    compare = at(validator, 0x8242F968)
    if compare.mnemonic != "cmpwi" or compare.op_str != "cr6, r29, 6":
        raise RuntimeError("validator does not compare signed version 6")
    load_version = at(validator, 0x8242F964)
    if load_version.mnemonic != "lwz" or load_version.op_str != "r29, 0(r3)":
        raise RuntimeError("validator version word is not loaded from this+0")
    equal_branch = at(validator, 0x8242F96C)
    if equal_branch.mnemonic != "beq" or target(equal_branch) != 0x8242F9C0:
        raise RuntimeError("version 6 equality does not enter relationship path")

    mismatch_va = pair_address(0x8242F970, 0x8242F974)
    newline_va = pair_address(0x8242F978, 0x8242F97C)
    comma_got_va = pair_address(0x8242F980, 0x8242F984)
    gp_lis = at(validator, 0x8242F988)
    gp_load = at(validator, 0x8242F98C)
    gp_hi = int(split_ops(gp_lis)[1], 0)
    mem = re.search(r"([^()]+)[(](r[0-9]+)[)]", split_ops(gp_load)[1])
    if gp_lis.mnemonic != "lis" or gp_load.mnemonic != "lwz" or not mem:
        raise RuntimeError("gpDebugPrint relocation sequence missing")
    gp_effective = (((gp_hi & 0xFFFF) << 16) + int(mem.group(1), 0)) & 0xFFFFFFFF
    if gp_effective != gp_va:
        raise RuntimeError(f"gpDebugPrint relocation target mismatch: 0x{gp_effective:08X}")
    if split_ops(gp_load)[0] != "r3" or mem.group(2) != split_ops(gp_lis)[0]:
        raise RuntimeError("gpDebugPrint pointer is not loaded into r3")

    log_calls = [i for i in validator if i.mnemonic == "bl" and 0x8242F970 <= i.address <= 0x8242F9B0]
    log_targets = [target(i) for i in log_calls]
    expected_calls = [op_text_va, op_s32_va, op_text_va, op_s32_va, op_text_va]
    if log_targets != expected_calls:
        raise RuntimeError(f"log overload target/order mismatch: {log_targets}")
    if at(validator, 0x8242F994).mnemonic != "li" or at(validator, 0x8242F994).op_str != "r4, 6":
        raise RuntimeError("expected version 6 is not passed as a signed scalar")
    if at(validator, 0x8242F9A4).mnemonic != "mr" or at(validator, 0x8242F9A4).op_str != "r4, r29":
        raise RuntimeError("stored version word is not passed as signed scalar")
    if at(validator, 0x8242F9B4).mnemonic != "li" or at(validator, 0x8242F9B4).op_str != "r3, 0":
        raise RuntimeError("mismatch path does not return false")
    mismatch_region = [i for i in validator if 0x8242F970 <= i.address <= 0x8242F9B4]
    if any(i.mnemonic.startswith("b") and i.mnemonic != "bl" for i in mismatch_region):
        raise RuntimeError("unexpected branch/filter guard in mismatch log path")
    memory_loads = [i for i in mismatch_region if i.mnemonic in ("lwz", "lbz", "lhz", "ld")]
    if len(memory_loads) != 1 or memory_loads[0].address != 0x8242F98C:
        raise RuntimeError("unexpected memory gate in mismatch logging path")
    if gp_effective == filter_va:
        raise RuntimeError("message-filter global was used as logger")

    relocated_literals = {
        "mismatch": require_literal(mismatch_va, b"Live Revenge Profile version mismatch, expected " + bytes((0,))),
        "comma_got": require_literal(comma_got_va, b", got " + bytes((0,))),
        "newline": require_literal(newline_va, bytes((10, 0))),
    }

    relationship_calls = [target(i) for i in validator if i.mnemonic == "bl"]
    if located["relationship_validate"]["va"] not in relationship_calls:
        raise RuntimeError("valid profile path lacks relationship Validate call")

    all_validation_calls = [target(i) for i in validate_profiles if i.mnemonic == "bl"]
    expected_validation_calls = [0x824F0978, 0x824F0AC8, 0x8242BEE8, 0x8242D600, validator_sym["va"]]
    validation_calls = [call for call in all_validation_calls if call in expected_validation_calls]
    if validation_calls != expected_validation_calls or len(all_validation_calls) != 6:
        raise RuntimeError(f"stored-profile validator call order differs: {all_validation_calls}")

    report_validate = at(report_task, 0x82430948)
    report_false_branch = at(report_task, 0x82430954)
    report_deserialise = at(report_task, 0x82430974)
    if report_validate.mnemonic != "bl" or target(report_validate) != located["validate_profiles"]["va"]:
        raise RuntimeError("ReportTaskCompleted does not call ValidateProfiles")
    if report_false_branch.mnemonic != "beq" or target(report_false_branch) != 0x82430B08:
        raise RuntimeError("false validation does not branch around Deserialise")
    if report_deserialise.mnemonic != "bl" or target(report_deserialise) != located["deserialise"]["va"]:
        raise RuntimeError("success-path Deserialise target mismatch")
    if not (report_validate.address < report_false_branch.address < report_deserialise.address < target(report_false_branch)):
        raise RuntimeError("Deserialise is not behind the false-result gate")

    handler_load = at(loaded_event, 0x8251BA34)
    handler_validate = at(loaded_event, 0x8251BA38)
    handler_zero_branch = at(loaded_event, 0x8251BA44)
    if handler_load.mnemonic != "lwz" or handler_load.op_str != "r3, 0x934(r30)":
        raise RuntimeError("loaded-event route does not validate manager profile +0x934")
    if handler_validate.mnemonic != "bl" or target(handler_validate) != validator_sym["va"]:
        raise RuntimeError("loaded-event route does not call shared validator")
    if handler_zero_branch.mnemonic != "bne" or target(handler_zero_branch) != 0x8251BA64:
        raise RuntimeError("loaded-event false result no longer falls through assert path")

    semantic = {
        "status": "PASS_LINKED_PROFILE_SEMANTICS",
        "validator": {
            "va": f"0x{validator_sym['va']:08X}",
            "object": validator_sym["object"],
            "comdat_inline": "i" in validator_sym["flags"],
            "version_load": "lwz r29,0(r3)",
            "signed_version_compare": "cmpwi r29,6",
            "equality_target": "0x8242F9C0",
            "mismatch_log_call_targets": [f"0x{x:08X}" for x in log_targets],
            "operator_text_va": f"0x{op_text_va:08X}",
            "operator_s32_va": f"0x{op_s32_va:08X}",
            "gp_debug_print_effective_va": f"0x{gp_effective:08X}",
            "message_filter_va_not_referenced": f"0x{filter_va:08X}",
            "relocated_literals": relocated_literals,
            "mismatch_returns_false_at": "0x8242F9B4",
            "valid_relationship_validate_target": f"0x{located['relationship_validate']['va']:08X}",
            "no_extra_mismatch_guard": True,
        },
        "stored_save_route": {
            "validate_profiles_va": f"0x{located['validate_profiles']['va']:08X}",
            "ordered_validator_targets": [f"0x{x:08X}" for x in validation_calls],
            "report_task_va": f"0x{located['report_task']['va']:08X}",
            "false_branch": "0x82430954 -> 0x82430B08",
            "deserialise_success_call": f"0x{target(report_deserialise):08X}",
            "deserialise_gated_on_valid": True,
        },
        "loaded_event_route": {
            "handler_va": f"0x{located['loaded_event']['va']:08X}",
            "manager_profile_load": "lwz r3,0x934(r30) at 0x8251BA34",
            "shared_validator_call": "0x8251BA38",
            "false_assert_path": "bne on true at 0x8251BA44; false falls through assertion calls",
        },
    }

    def format_insns(insns):
        return [f"{i.address:08X}: {i.mnemonic:<8} {i.op_str}".rstrip() for i in insns]
    dump = []
    for key in ("validator", "validate_profiles", "report_task", "loaded_event"):
        sym = located[key]
        dump.append(f"## {key} {sym['symbol']} @ 0x{sym['va']:08X} [{sym['object']}]")
        dump.extend(format_insns(funcs[key]))
        dump.append("")
    dump.append("## string locations")
    dump.extend(f"{k}: {v}" for k, v in strings.items())
    dump.append("")
    dump_path = ev / "R159_LINKED_PROFILE_DISASSEMBLY.txt"
    dump_path.write_text("\n".join(dump) + "\n", encoding="utf-8")

    summary = {
        "status": semantic["status"],
        "root_pins_sha256": ROOT_PINS_SHA,
        "integrated": {"head": INTEGRATED_HEAD, "tree": INTEGRATED_TREE, "parent": pins["parent"], "validator_header_blob": composed_blob},
        "link": {
            "seal_sha256": pins["pins"]["seal"]["sha256"],
            "invocation_sha256": pins["pins"]["invocation"]["sha256"],
            "result_sha256": pins["pins"]["result"]["sha256"],
            "exit_code": result.get("exit_code"),
            "all_post_checks_pass": result.get("all_post_checks_pass"),
            "pe_sha256": pins["pins"]["pe"]["sha256"],
            "map_sha256": pins["pins"]["map"]["sha256"],
            "xex_sha256": pins["pins"]["xex"]["sha256"],
            "xex_magic": xex_magic.decode("ascii"),
            "machine": f"0x{pe.FILE_HEADER.Machine:04X}",
            "image_base": f"0x{image_base:08X}",
        },
        "map_symbols": {
            k: {"symbol": v["symbol"], "va": f"0x{v['va']:08X}", "object": v["object"], "flags": v["flags"]}
            for k, v in located.items()
        },
        "first_string_occurrences": strings,
        "function_instruction_counts": {k: len(v) for k, v in funcs.items()},
        "semantic": semantic,
        "disassembly_path": str(dump_path),
        "disassembly_sha256": sha(dump_path),
        "limits": ["Static linked-image evidence only; no runtime or save access."]
    }
    out_path = ev / "R159_LINKED_PROFILE_DISASSEMBLY.json"
    out_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))

if __name__ == "__main__":
    main()





