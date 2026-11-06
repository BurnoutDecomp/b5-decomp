"""Verify the native Apt interface count accesses in the targeted XDK COFF."""

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BASE = "bdbd812370b68b9d3fd1b3b4f27f6fa3d7632704"
SOURCE = "src/SDKs/EATech/include/Apt/AptObject.cpp"
REPORT = ROOT / "native" / "compile-results.json"
BASE_LINK = ROOT.parent / "r140-link"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def functions(data):
    machine, _, _, table, count, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional_size == 0
    strings = table + 18 * count
    symbols = {}
    index = 0
    while index < count:
        name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, table + 18 * index)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind
        index += 1 + aux

    result = {}
    for name, value, section, kind in symbols.values():
        if kind != 0x20 or section <= 0 or value != 0:
            continue
        header = 20 + (section - 1) * 40
        if data[header:header + 5] != b".text":
            continue
        size, offset, relocations = struct.unpack_from("<III", data, header + 16)
        count = struct.unpack_from("<H", data, header + 32)[0]
        refs = {}
        for index in range(count):
            off, symbol, rel_type = struct.unpack_from("<IIH", data, relocations + 10 * index)
            if rel_type != 0x12:  # PPC PAIR follows a split address relocation
                refs[off] = symbols[symbol][0]
        result[name] = struct.unpack_from(f">{size // 4}I", data, offset), refs
    return result


def method(symbols, name):
    [body] = [body for symbol, body in symbols.items() if symbol.startswith(f"?{name}@AptObject@@")]
    return body


def class_word_accesses(code, base):
    # PPC D-form byte/word/halfword loads and stores touching +1C..+1F.
    return {hex(index * 4): f"{word:08x}" for index, word in enumerate(code)
            if 32 <= word >> 26 <= 45 and (word >> 16) & 31 == base
            and 0x1C <= word & 0xFFFF <= 0x1F}


report = json.loads(REPORT.read_text())
assert report["passed"] == 1 and report["failed"] == 0
[row] = report["results"]
assert row["source"] == SOURCE
candidate = (ROOT / "native" / Path(row["object"]).name).read_bytes()
image = json.loads((BASE_LINK / "image-verification.json").read_text())
assert image["head"] == BASE
invocation = json.loads((BASE_LINK / "invocation.json").read_text())
[selected] = [r for r in invocation["objects"] if r["source"] == SOURCE]
baseline = Path(selected["object"]).read_bytes()
assert sha(baseline) == selected["sha256"]
old, new = functions(baseline), functions(candidate)

# Preserve every other function body and relocation map, including both flag
# methods and the base constructor's field initialization.
assert old.keys() == new.keys()
changed = {name for name in old if old[name] != new[name]}
assert {name.split("@")[0] for name in changed} == {
    "?SetImplementedObjects", "?GetImplementedObjects", "?DoesImplementObject",
}

setter, setter_refs = method(new, "SetImplementedObjects")
getter, _ = method(new, "GetImplementedObjects")
scan, scan_refs = method(new, "DoesImplementObject")
assert setter_refs[0x34].startswith("?Set@AptNativeHash@@")
assert scan_refs[0x8C].startswith("?Lookup@AptNativeHash@@")
assert getter[0x1C // 4] == 0x91640000  # store zero-extended count to *pnCountOut

accesses = {
    "SetImplementedObjects": class_word_accesses(setter, 31),
    "GetImplementedObjects": class_word_accesses(getter, 31),
    "DoesImplementObject": class_word_accesses(scan, 30),
}
assert accesses == {
    "SetImplementedObjects": {"0x3c": "9bbf001c"},  # stb r29,0x1C(r31)
    "GetImplementedObjects": {"0x14": "897f001c"},  # lbz r11,0x1C(r31)
    "DoesImplementObject": {"0x68": "897e001c", "0x90": "891e001c"},
}

receipt = {
    "base": BASE,
    "source": SOURCE,
    "object_sha256": sha(candidate),
    "baseline_object_sha256": sha(baseline),
    "compile_report_sha256": sha(REPORT.read_bytes()),
    "artist_count_store": "0x82af58a4",
    "artist_count_loads": ["0x82ae6904", "0x82ae690c", "0x82ae69c0", "0x82ae69e8"],
    "coff_class_word_accesses": accesses,
    "unchanged_function_count": len(old) - len(changed),
    "lower_24_bits": "single byte store leaves bytes +0x1d through +0x1f intact",
    "runtime": "not run",
}
(ROOT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
