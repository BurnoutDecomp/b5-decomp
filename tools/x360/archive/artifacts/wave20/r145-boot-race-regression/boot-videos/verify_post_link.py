#!/usr/bin/env python3
"""Verify the BootVideos R141 object in a future exact X360 link folder.

Read-only: consumes invocation.json, native-game.map, native-game.pe, the
invocation-selected object, and the frozen sibling targeted-receipt.json.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

REGRESSION_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REGRESSION_ROOT))
from link_context import DEFAULT_HEAD, TARGETS, verify_link_context

MACHINE_PPC_BE = 0x01F2
SOURCE = "src/GameSource/Gui/Flow/HUD/States/BrnBootVideos.cpp"
UPDATE = "?Update@BootVideos@BrnGui@@UAAXXZ"
POST70 = "??$PostCommand16@$0EG@@?A0xdf9a84f2@BrnGui@@YAXPAUStateInterface@CgsGui@@HE@Z"
EA_LOGO = "??_C@_0BC@HJLDLDHA@EA_Criterion_Logo?$AA@"
ANON_HASH = re.compile(r"(\?A0x)[0-9A-Fa-f]+(@)")
RELOC_MASKS = {0x02: 0xFFFFFFFF, 0x06: 0x03FFFFFC, 0x10: 0x0000FFFF,
               0x11: 0x0000FFFF, 0x12: 0}


class VerifyError(Exception):
    pass


def need(condition: bool, message: str) -> None:
    if not condition:
        raise VerifyError(message)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def read_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as e:
        raise VerifyError(f"cannot read JSON {path}: {e}") from e


def canonical_symbol(name: str) -> str:
    return ANON_HASH.sub(r"\1<anonymous>@", name)


class Coff:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        need(len(self.data) >= 20, f"short COFF object: {path}")
        machine, nsec, _, symptr, nsyms, optsize, _ = struct.unpack_from("<HHIIIHH", self.data)
        need(machine == MACHINE_PPC_BE, f"wrong COFF machine in {path}: 0x{machine:04X}")
        self.machine = machine
        strings_at = symptr + nsyms * 18
        need(strings_at + 4 <= len(self.data), f"bad COFF symbol table in {path}")
        strings = self.data[strings_at:]
        symbols = {}
        i = 0
        while i < nsyms:
            raw, value, section, _, storage, aux = struct.unpack_from("<8sIhHBB", self.data, symptr + 18 * i)
            if raw[:4] == bytes(4):
                off = struct.unpack_from("<I", raw, 4)[0]
                end = strings.find(b"\0", off) if 4 <= off < len(strings) else -1
                need(end >= 0, f"bad long COFF symbol name in {path}")
                name = strings[off:end].decode("latin1")
            else:
                name = raw.rstrip(b"\0").decode("latin1")
            symbols[i] = {"name": name, "value": value, "section": section, "storage_class": storage}
            i += 1 + aux
        self.symbols = symbols
        sections = {}
        relocs = []
        for n in range(nsec):
            h = struct.unpack_from("<8sIIIIIIHHI", self.data, 20 + optsize + 40 * n)
            name = h[0].rstrip(b"\0").decode("latin1")
            raw_size, raw_at, reloc_at, nreloc = h[3], h[4], h[5], h[7]
            need(raw_at + raw_size <= len(self.data), f"bad section data in {path}, section {n + 1}")
            sections[n + 1] = {"name": name, "size": raw_size, "raw_at": raw_at,
                               "bytes": self.data[raw_at:raw_at + raw_size]}
            for k in range(nreloc):
                address, symbol_index, kind = struct.unpack_from("<IIH", self.data, reloc_at + 10 * k)
                need(symbol_index in symbols, f"bad COFF relocation symbol index in {path}")
                s = symbols[symbol_index]
                relocs.append({"section": n + 1, "offset": address, "type": kind,
                               "symbol": s["name"], "target_section": s["section"],
                               "target_value": s["value"], "storage_class": s["storage_class"]})
        self.sections = sections
        self.relocations = relocs

    def function(self, name: str):
        found = [s for s in self.symbols.values() if s["name"] == name and s["section"] > 0]
        if not found:
            key = canonical_symbol(name)
            found = [s for s in self.symbols.values() if canonical_symbol(s["name"]) == key and s["section"] > 0]
        need(len(found) == 1, f"expected one COFF function symbol {name}, found {len(found)}")
        sym = found[0]
        sec = self.sections[sym["section"]]
        need(sym["value"] == 0, f"{name} is not at its COMDAT section start")
        return sym, sec, [r for r in self.relocations if r["section"] == sym["section"]]


def relocation_manifest(records):
    return sorted(({"offset": r["offset"], "type": r["type"], "symbol": r["symbol"],
                    "target_section": r["target_section"], "target_value": r["target_value"],
                    "storage_class": r["storage_class"]} for r in records),
                  key=lambda r: (r["offset"], r["type"], r["symbol"]))


def canonical_manifest(records):
    result = relocation_manifest(records)
    for r in result:
        r["symbol"] = canonical_symbol(r["symbol"])
    return result


def normalize_code(raw: bytes, relocs) -> bytes:
    need(len(raw) % 4 == 0, "function body is not PPC-word aligned")
    out = bytearray(raw)
    for r in relocs:
        kind = r["type"]
        need(kind in RELOC_MASKS, f"unsupported PPC COFF relocation type 0x{kind:02X}")
        mask = RELOC_MASKS[kind]
        if not mask:
            continue
        at = r["offset"]
        need(at % 4 == 0 and at + 4 <= len(out), f"relocation outside function at +0x{at:X}")
        word = struct.unpack_from(">I", out, at)[0]
        struct.pack_into(">I", out, at, word & ~mask)
    return bytes(out)


def word_at(raw: bytes, offset: int) -> int:
    need(offset % 4 == 0 and offset + 4 <= len(raw), f"bad word offset +0x{offset:X}")
    return struct.unpack_from(">I", raw, offset)[0]


class PeImage:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        need(self.data[:2] == b"MZ", f"not an MZ/PE image: {path}")
        pe_at = struct.unpack_from("<I", self.data, 0x3C)[0]
        need(self.data[pe_at:pe_at + 4] == b"PE\0\0", f"missing PE signature: {path}")
        machine, nsec, _, _, _, optsize, _ = struct.unpack_from("<HHIIIHH", self.data, pe_at + 4)
        need(machine == MACHINE_PPC_BE, f"wrong linked PE machine: 0x{machine:04X}")
        opt = pe_at + 24
        magic = struct.unpack_from("<H", self.data, opt)[0]
        need(magic == 0x10B, f"expected PPC PE32 optional header, got 0x{magic:04X}")
        self.image_base = struct.unpack_from("<I", self.data, opt + 28)[0]
        self.sections = []
        for i in range(nsec):
            q = opt + optsize + 40 * i
            name = self.data[q:q + 8].rstrip(b"\0").decode("latin1")
            virtual_size, virtual_address, raw_size, raw_at = struct.unpack_from("<IIII", self.data, q + 8)
            self.sections.append({"name": name, "va": self.image_base + virtual_address,
                                  "virtual_size": virtual_size, "raw_size": raw_size, "raw_at": raw_at})

    def bytes_at_va(self, va: int, size: int) -> bytes:
        for s in self.sections:
            delta = va - s["va"]
            if delta >= 0 and delta + size <= s["raw_size"]:
                return self.data[s["raw_at"] + delta:s["raw_at"] + delta + size]
        raise VerifyError(f"VA 0x{va:08X}+0x{size:X} is not backed by PE section bytes")


def read_map(path: Path):
    result = {}
    pattern = re.compile(r"^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8})\s+(.*)$")
    for line in path.read_text(encoding="latin1", errors="replace").splitlines():
        m = pattern.match(line)
        if not m:
            continue
        name, address, rest = m.groups()
        object_match = re.search(r"(?:^|\s)([^\s]+\.obj)(?:\s|$)", rest, re.IGNORECASE)
        item = {"va": int(address, 16), "object": object_match.group(1) if object_match else None}
        result.setdefault(name, []).append(item)
    return result


def map_symbol(symbols, name: str, required=True):
    rows = symbols.get(name, [])
    unique = {(r["va"], (r["object"] or "").lower()): r for r in rows}
    if not unique and not required:
        return None
    need(len(unique) == 1, f"map symbol {name!r} missing or ambiguous ({len(unique)} rows)")
    return next(iter(unique.values()))


def sign_extend(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def branch_target(word: int, pc: int) -> int:
    need((word >> 26) == 18, f"expected PPC REL24 branch at 0x{pc:08X}, got 0x{word:08X}")
    disp = sign_extend(word & 0x03FFFFFC, 26)
    return disp & 0xFFFFFFFF if word & 2 else (pc + disp) & 0xFFFFFFFF


def low_base_register(word: int) -> int:
    return (word >> 16) & 31


def high_target_register(word: int) -> int:
    return (word >> 21) & 31


def verify_linked_relocations(function, linked: bytes, function_va: int, map_symbols, image: PeImage):
    relocs = function["relocations"]
    verified_branches = 0
    resolved_call_targets = {}
    for r in relocs:
        at, kind, name = r["offset"], r["type"], r["symbol"]
        if kind == 0x06:
            target = map_symbol(map_symbols, name)
            got = branch_target(word_at(linked, at), function_va + at)
            need(got == target["va"], f"REL24 +0x{at:X} to {name} resolves 0x{got:08X}, expected 0x{target['va']:08X}")
            resolved_call_targets[f"0x{at:X}"] = {"symbol": name, "va": f"0x{got:08X}"}
            verified_branches += 1
        elif kind == 0x02 and r["target_section"] == function["section"]:
            got = word_at(linked, at)
            expected = (function_va + r["target_value"]) & 0xFFFFFFFF
            need(got == expected, f"ADDR32 +0x{at:X} to {name} is 0x{got:08X}, expected 0x{expected:08X}")
        elif kind == 0x02:
            target = map_symbol(map_symbols, name, required=False)
            if target:
                got = word_at(linked, at)
                need(got == target["va"], f"ADDR32 +0x{at:X} to {name} is 0x{got:08X}, expected 0x{target['va']:08X}")

    resolved_ref_pairs = []
    highs = [r for r in relocs if r["type"] == 0x10]
    lows = [r for r in relocs if r["type"] == 0x11]
    for high in highs:
        target = map_symbol(map_symbols, high["symbol"], required=False)
        if not target:
            continue
        hi_word = word_at(linked, high["offset"])
        need((hi_word >> 26) == 15, f"REFHI +0x{high['offset']:X} is not addis/lis")
        reg = high_target_register(hi_word)
        matches = []
        for low in lows:
            if low["symbol"] != high["symbol"] or low["offset"] <= high["offset"]:
                continue
            lo_word = word_at(linked, low["offset"])
            if low_base_register(lo_word) != reg:
                continue
            lo = sign_extend(lo_word & 0xFFFF, 16)
            address = (((hi_word & 0xFFFF) << 16) + lo) & 0xFFFFFFFF
            if address == target["va"]:
                matches.append(low)
        need(matches, f"REFHI +0x{high['offset']:X} for {high['symbol']} has no matching linked REFLO")
        resolved_ref_pairs.append({"symbol": high["symbol"], "va": f"0x{target['va']:08X}",
                                   "high_offset": f"0x{high['offset']:X}",
                                   "low_offsets": [f"0x{x['offset']:X}" for x in matches]})

    logo_reloc = next((r for r in relocs if r["offset"] == 0x108 and r["type"] == 0x10 and r["symbol"] == EA_LOGO), None)
    if function["symbol"] == UPDATE:
        need(logo_reloc is not None, "EA_Criterion_Logo REFHI relocation missing at Update+0x108")
        logo = map_symbol(map_symbols, EA_LOGO)
        need(any(x["symbol"] == EA_LOGO and x["high_offset"] == "0x108" and "0x114" in x["low_offsets"]
                 for x in resolved_ref_pairs), "EA_Criterion_Logo pointer relocation did not resolve through +0x114")
        payload = image.bytes_at_va(logo["va"], len(b"EA_Criterion_Logo\0"))
        need(payload == b"EA_Criterion_Logo\0", "linked component-name literal is not EA_Criterion_Logo")

    return {"rel24_calls": verified_branches, "resolved_calls": resolved_call_targets,
            "resolved_ref_pairs": resolved_ref_pairs}


def verify_function(coff: Coff, function, image: PeImage, map_symbols, selected_object: Path):
    symbol = function["symbol"]
    actual_symbol, section, actual_relocs = coff.function(symbol)
    need(section["size"] == function["size"], f"selected object function size differs for {symbol}")
    mapped = map_symbol(map_symbols, actual_symbol["name"])
    need(Path(mapped["object"] or "").name.lower() == selected_object.name.lower(),
         f"map {symbol} owner {mapped['object']!r} is not invocation-selected {selected_object.name}")
    va = mapped["va"]
    linked = image.bytes_at_va(va, function["size"])
    linked_norm = normalize_code(linked, actual_relocs)
    expected_norm = bytes.fromhex(function["normalized_code_be_hex"])
    need(linked_norm == expected_norm, f"linked PPC differs for {symbol}: {first_word_difference(expected_norm, linked_norm)}")
    landmarks = []
    for item in function.get("landmarks", []):
        got = word_at(linked, item["offset"])
        need(got == item["word"], f"{symbol} {item['name']} at +0x{item['offset']:X}: got 0x{got:08X}, expected 0x{item['word']:08X}")
        landmarks.append({"name": item["name"], "offset": f"0x{item['offset']:X}", "word": f"0x{got:08X}"})
    linked_function = dict(function)
    linked_function["symbol"] = actual_symbol["name"]
    linked_function["section"] = actual_symbol["section"]
    linked_function["relocations"] = relocation_manifest(actual_relocs)
    reloc_result = verify_linked_relocations(linked_function, linked, va, map_symbols, image)
    canonical_relocs = canonical_manifest(actual_relocs)
    reloc_sha = hashlib.sha256(json.dumps(canonical_relocs, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest().upper()
    return {"symbol": symbol, "linked_symbol": actual_symbol["name"], "va": f"0x{va:08X}", "size": function["size"],
            "normalized_ppc_sha256": hashlib.sha256(linked_norm).hexdigest().upper(),
            "targeted_normalized_ppc_sha256": function["normalized_code_sha256"],
            "matches_targeted_ppc": True,
            "semantic_relocation_manifest_sha256": reloc_sha,
            "landmarks": landmarks, "relocations": reloc_result}


def first_word_difference(expected: bytes, actual: bytes) -> str:
    for at in range(0, min(len(expected), len(actual)), 4):
        a, b = struct.unpack_from(">I", expected, at)[0], struct.unpack_from(">I", actual, at)[0]
        if a != b:
            return f"word +0x{at:X}: expected 0x{a:08X}, got 0x{b:08X}"
    return f"length expected {len(expected)}, got {len(actual)}"


def verify(link_dir: Path, receipt_path: Path, expected_head: str = DEFAULT_HEAD):
    link_context = verify_link_context(link_dir, expected_head)
    receipt = read_json(receipt_path)
    need(receipt.get("schema") == 1, "unsupported targeted receipt schema")
    need(receipt.get("source") == SOURCE, "targeted receipt is for a different source")
    compile_record = receipt["compile_report"]
    compile_path = Path(compile_record["path"])
    need(compile_path.is_file() and sha256(compile_path) == compile_record["sha256"].upper(),
         "frozen targeted compile report is missing or changed")
    compile_report = read_json(compile_path)
    need(compile_report.get("state") == "complete" and compile_report.get("target") == "xbox360-ppc-be",
         "frozen targeted compile report is not a completed native X360 compile")
    assembly = receipt["assembly"]
    assembly_path = Path(assembly["path"])
    need(assembly_path.is_file() and sha256(assembly_path) == assembly["sha256"].upper(),
         "frozen targeted assembly is missing or changed")
    link_dir = link_dir.resolve()
    invocation_path = link_dir / "invocation.json"
    map_path = link_dir / "native-game.map"
    pe_path = link_dir / "native-game.pe"
    result_path = link_dir / "result.json"
    for p in (invocation_path, map_path, pe_path, result_path):
        need(p.is_file(), f"exact link folder is missing {p.name}: {link_dir}")
    invocation = read_json(invocation_path)
    matches = [o for o in invocation.get("objects", []) if o.get("source", "").replace("\\", "/").lower() == SOURCE.lower()]
    need(len(matches) == 1, f"invocation must select exactly one {SOURCE}, found {len(matches)}")
    selected = matches[0]
    selected_path = Path(selected["object"])
    if not selected_path.is_absolute():
        selected_path = (link_dir / selected_path).resolve()
    need(selected_path.is_file(), f"invocation-selected BootVideos object is missing: {selected_path}")
    expected_obj = receipt["object"]
    selected_hash = sha256(selected_path)
    invocation_object_hash = selected.get("sha256", "").upper()
    need(invocation_object_hash == selected_hash, "invocation BootVideos SHA does not match the selected object file")

    link_result = read_json(result_path)
    need(link_result.get("exit_code") == 0 and link_result.get("all_post_checks_pass") is True,
         "exact link result does not report a completed link with passing post checks")
    bound_outputs = {}
    outputs = link_result.get("outputs", {})
    exe_path = link_dir / "native-game.exe"
    for required_name, required_path in (("native-game.map", map_path), ("native-game.pe", pe_path), ("native-game.exe", exe_path)):
        need(required_path.is_file(), f"exact link folder is missing {required_name}")
        rows = [v for k, v in outputs.items() if Path(k).name.lower() == required_name]
        need(len(rows) == 1, f"result.json must bind exactly one {required_name}")
        row = rows[0]
        need(required_path.stat().st_size == row.get("size") and sha256(required_path) == row.get("sha256", "").upper(),
             f"{required_name} does not match result.json size/SHA")
        bound_outputs[required_name] = {"size": row["size"], "sha256": row["sha256"].upper()}

    target_object = Path(expected_obj["path"])
    need(target_object.is_file(), f"frozen target object is missing: {target_object}")
    need(sha256(target_object) == expected_obj["sha256"].upper(), "frozen target object SHA changed")
    compile_entries = [o for o in compile_report.get("results", []) if o.get("source", "").replace("\\", "/").lower() == SOURCE.lower()]
    need(len(compile_entries) == 1 and compile_entries[0].get("passed") is True,
         "frozen compile report does not contain one passing BootVideos entry")
    need(Path(compile_entries[0]["object"]).resolve() == target_object.resolve(),
         "target receipt object path differs from frozen compile report")
    coff = Coff(selected_path)
    target_coff = Coff(target_object)
    map_symbols = read_map(map_path)
    image = PeImage(pe_path)
    results = []
    for function in receipt["functions"]:
        _, _, actual_relocs = coff.function(function["symbol"])
        actual_manifest = canonical_manifest(actual_relocs)
        expected_manifest = [{**r, "symbol": canonical_symbol(r["symbol"])} for r in function["relocations"]]
        need(actual_manifest == expected_manifest, f"invocation object semantic relocation manifest differs for {function['symbol']}")
        _, target_sec, target_relocs = target_coff.function(function["symbol"])
        need(target_sec["size"] == function["size"], f"frozen object function size changed for {function['symbol']}")
        target_norm = normalize_code(target_sec["bytes"], target_relocs)
        need(target_norm.hex().upper() == function["normalized_code_be_hex"].upper(), f"frozen object PPC differs from targeted receipt for {function['symbol']}")
        results.append(verify_function(coff, function, image, map_symbols, selected_path))

    update_relocs = next(f["relocations"] for f in receipt["functions"] if f["symbol"] == UPDATE)
    update_actual = coff.function(UPDATE)[2]
    call70 = [r for r in update_actual if r["type"] == 0x06 and canonical_symbol(r["symbol"]) == canonical_symbol(POST70)]
    need({r["offset"] for r in call70} == {0x0C8, 0x334}, "Update must retain both event-70 helper calls at +0xC8 and +0x334")
    helper = next(f for f in receipt["functions"] if canonical_symbol(f["symbol"]) == canonical_symbol(POST70))
    helper_actual = coff.function(helper["symbol"])[2]
    helper_calls = {r["offset"]: r["symbol"] for r in helper_actual if r["type"] == 0x06}
    need(helper_calls.get(0x50, "").startswith("?GetOutputEventQueue@"), "event-70 helper lost GetOutputEventQueue call")
    need(helper_calls.get(0x60, "").startswith("?AddEvent@"), "event-70 helper lost AddEvent call")
    return {"result": "pass", "link_folder": str(link_dir),
            "link_context": link_context,
            "prior_r144_linked_receipt_sha256": TARGETS["previous_linked_receipts"]["R141_BOOT_VIDEOS_LINKED.json"]["sha256"],
            "invocation_sha256": sha256(invocation_path),
            "link_result_sha256": sha256(result_path),
            "bound_link_outputs": bound_outputs,
            "selected_bootvideos_object": {"source": selected["source"], "path": str(selected_path),
                                           "sha256": selected_hash,
                                           "private_compile_sha256": expected_obj["sha256"].upper(),
                                           "matches_private_compile_object": selected_hash == expected_obj["sha256"].upper()},
            "functions": results,
            "assertions": ["function PPC matches frozen R141 receipt after relocation-field normalization",
                           "semantic COFF relocation manifest matches after anonymous namespace hash normalization; linked call/data targets resolve",
                           "event 21 reads kind at +0, loads the name pointer at +8, then dereferences it",
                           "event 70 payload and 16-byte AddEvent dispatch remain",
                           "both stage-transition and final video-finished event-510 checks remain"]}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("link_dir", type=Path, help="exact link folder containing inventory, seal, invocation, map, PE, and result")
    ap.add_argument("--receipt", type=Path, default=Path(__file__).resolve().with_name("targeted-receipt.json"), help="frozen R141 targeted receipt")
    ap.add_argument("--expected-head", default=DEFAULT_HEAD, help="HEAD pinned in source-inventory.json and seal.json")
    ap.add_argument("--report", type=Path, help="write the successful linked receipt to a new file (never overwrites)")
    args = ap.parse_args()
    try:
        result = verify(args.link_dir, args.receipt, args.expected_head)
        if args.report:
            report_path = args.report.resolve()
            need(report_path.parent.is_dir(), f"report directory does not exist: {report_path.parent}")
            need(not report_path.exists(), f"refusing to overwrite existing report: {report_path}")
            result["linked_receipt_path"] = str(report_path)
            report_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return 0
    except Exception as e:
        print(json.dumps({"result": "fail", "error": str(e)}, indent=2), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
