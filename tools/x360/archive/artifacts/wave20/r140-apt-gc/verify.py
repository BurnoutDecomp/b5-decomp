import hashlib
import json
import struct
from pathlib import Path

import pefile


ROOT = Path(__file__).resolve().parent
BASE = "ef3f9b543b9b2482e11d5c77914da04595d2b884"
BASE_IMAGE = ROOT.parent / "r137-link" / "native-game.pe"
REPORT = ROOT / "native" / "compile-results.json"


def words(data):
    return struct.unpack(f">{len(data) // 4}I", data)


def comdat(data, name):
    machine, _, _, symbols, count, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional_size == 0
    strings = symbols + 18 * count
    index = 0
    while index < count:
        pos = symbols + 18 * index
        symbol, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, pos)
        if symbol[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", symbol, 4)[0]
            symbol = data[start:data.index(b"\0", start)]
        if symbol.rstrip(b"\0").decode("ascii", errors="replace") == name:
            assert value == 0 and section > 0 and kind == 0x20
            header = 20 + (section - 1) * 40
            assert data[header:header + 5] == b".text"
            size, offset = struct.unpack_from("<II", data, header + 16)
            return data[offset:offset + size]
        index += 1 + aux
    raise ValueError(name)


def branch_target(offset, word, opcode):
    assert word >> 16 == opcode and not word & 3
    displacement = word & 0xFFFC
    return offset + (displacement if displacement < 0x8000 else displacement - 0x10000)


def check(code, offset, *expected):
    assert code[offset // 4:offset // 4 + len(expected)] == expected, hex(offset)


def digest(data):
    return hashlib.sha256(data).hexdigest()


report = json.loads(REPORT.read_text())
assert report["passed"] == 1 and report["failed"] == 0
result, = report["results"]
assert result["source"] == "src/SDKs/EATech/include/Apt/AptValue/AptValue.cpp"
obj = (ROOT / "native" / Path(result["object"]).name).read_bytes()
release = words(comdat(obj, "?Release@AptValue@@UAAXXZ"))
delete_this = comdat(obj, "?DeleteThis@AptValue@@UAAXXZ")

# Reference count is written before branching when the old count was not one.
check(release, 0x20, 0x2B0A0001, 0x512B71A2, 0x917F0004)
epilogue = branch_target(0x2C, release[0x2C // 4], 0x409A)
check(release, epilogue, 0x38210060, 0x8181FFF8, 0x7D8803A6, 0xEBE1FFF0, 0x4E800020)

# Suspension clear skips the predicate. Suspension set calls slot +0x30 and
# tests the returned low byte. A true result returns, and false falls through.
check(release, 0x38, 0x2B0B0000)
fallthrough = branch_target(0x3C, release[0x3C // 4], 0x419A)
check(release, 0x40, 0x817F0000, 0x816B0030, 0x7D6903A6, 0x4E800421,
      0x546B063E, 0x2B0B0000)
assert branch_target(0x58, release[0x58 // 4], 0x409A) == epilogue
assert fallthrough == 0x5C
check(release, fallthrough, 0x817F0004, 0x556ACFFE)

# Ordinary immediate teardown still dispatches ForceDelete at +0x2C.
force_delete = branch_target(0x94, release[0x94 // 4], 0x419A)
check(release, force_delete, 0x817F0000, 0x7FE3FB78, 0x816B002C, 0x7D6903A6, 0x4E800421)

image = pefile.PE(str(BASE_IMAGE))


def linked_bytes(address, size):
    rva = address - image.OPTIONAL_HEADER.ImageBase
    section = next(s for s in image.sections if s.VirtualAddress <= rva < s.VirtualAddress + s.SizeOfRawData)
    offset = section.PointerToRawData + rva - section.VirtualAddress
    return image.__data__[offset:offset + size]


# The R137 sweep still calls +0x20 and this TU's ordinary DeleteThis body is
# byte-identical to the published link. It calls the scalar destructor at +0x38.
check(words(linked_bytes(0x82739250, 16)), 0, 0x817F0000, 0x816B0020, 0x7D6903A6, 0x4E800421)
assert delete_this == linked_bytes(0x8274BFF0, len(delete_this))
check(words(delete_this), 0x0C, 0x38800001, 0x816B0038, 0x7D6903A6, 0x4E800420)

receipt = {
    "base": BASE,
    "source": result["source"],
    "object_sha256": digest(obj),
    "compile_report_sha256": digest(REPORT.read_bytes()),
    "artist": "0x82AE31D0",
    "candidate_release_offsets": {
        "last_reference_branch": "0x2c",
        "gc_predicate_slot_load": "0x44",
        "true_return_branch": "0x58",
        "false_fallthrough": hex(fallthrough),
        "normal_force_delete_slot_load": hex(force_delete + 8),
    },
    "baseline_sweep_delete_slot_load": "0x82739254",
    "delete_this_bytes_unchanged": len(delete_this),
    "runtime": "not run",
}
(ROOT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
