import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BASE = "bdbd812370b68b9d3fd1b3b4f27f6fa3d7632704"
REPORT = ROOT / "native" / "compile-results.json"
SOURCE = "src/SDKs/EATech/include/Apt/AptActionInterpreterCompare.cpp"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_coff(data):
    machine, _, _, table, count, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional_size == 0
    strings = table + count * 18
    symbols = {}
    index = 0
    while index < count:
        pos = table + index * 18
        name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, pos)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind
        index += 1 + aux
    return symbols


def comdat(data, symbols, prefix):
    [(_, value, section, kind)] = [sym for sym in symbols.values() if sym[0].startswith(prefix) and sym[2] > 0]
    assert value == 0 and kind == 0x20
    header = 20 + (section - 1) * 40
    assert data[header:header + 5] == b".text"
    size, offset, reloc = struct.unpack_from("<III", data, header + 16)
    count = struct.unpack_from("<H", data, header + 32)[0]
    references = {}
    for index in range(count):
        address, symbol, kind = struct.unpack_from("<IIH", data, reloc + 10 * index)
        if kind != 0x12:  # PPC PAIR follows a split address relocation
            references[address] = symbols[symbol][0]
    return struct.unpack_from(f">{size // 4}I", data, offset), references


def check(code, offset, *expected):
    assert code[offset // 4:offset // 4 + len(expected)] == expected, hex(offset)


def target(code, offset, opcode):
    word = code[offset // 4]
    assert word >> 16 == opcode and not word & 3, hex(offset)
    bits = 26 if opcode == 0x4800 else 16
    displacement = word & ((1 << bits) - 4)
    return offset + (displacement if displacement < 1 << (bits - 1) else displacement - (1 << bits))


report = json.loads(REPORT.read_text())
assert report["passed"] == 1 and report["failed"] == 0
[row] = report["results"]
assert row["source"] == SOURCE
obj = (ROOT / "native" / Path(row["object"]).name).read_bytes()
symbols = read_coff(obj)
code, references = comdat(obj, symbols, "?_FunctionAptActionEquals2@")
strings, _ = comdat(obj, symbols, "?IsDefinedStringType@")


def call(offset, prefix):
    assert code[offset // 4] & 0xFC000003 == 0x48000001, hex(offset)
    assert references[offset].startswith(prefix), (hex(offset), references[offset])


# Identify top r25 and under r26, and the string predicate's definedness tests.
check(code, 0x14, 0x81780000, 0x81580008, 0x5569103A, 0x396BFFFE,
      0x7D295214, 0x556B103A, 0x8329FFFC, 0x7F4B502E)
check(strings, 0x0C, 0x2F090001)
check(strings, 0x14, 0x556A2FFE, 0x2B0A0000)
check(strings, 0x34, 0x2F090021)
check(strings, 0x3C, 0x556B2FFE, 0x2B0B0000)

# Both coercion flags recognize tag 6 plus the defined bit before they can
# call the string predicate. If a string search returns -1, its flag is zero.
for operand, header, passed, predicate_branch, destination, helper, find, flag0, flag1 in (
    (25, 0x4AC, 0x4D0, 0x4E0, 0x520, 0x4E8, 0x50C, 0x3BC00000, 0x3BC00001),
    (26, 0x524, 0x548, 0x550, 0x590, 0x558, 0x57C, 0x3BE00000, 0x3BE00001),
):
    check(code, header, 0x81600004 | (operand << 16), 0x556A067E, 0x2F0A0006)
    assert target(code, header + 0xC, 0x409A) == header + 0x20
    check(code, header + 0x10, 0x556B2FFE, 0x2B0B0000, 0x39600001)
    assert target(code, header + 0x1C, 0x409A) == passed
    check(code, header + 0x20, 0x39600000, 0x556B063E, 0x2B0B0000)
    assert target(code, predicate_branch, 0x409A) == destination
    call(helper, "?IsDefinedStringType@")
    assert target(code, helper + 0xC, 0x419A) == find + 0xC
    call(find, "?UTF8_Find@EAStringC@@")
    check(code, find + 4, 0x2F03FFFF)
    assert target(code, find + 8, 0x409A) == destination
    check(code, find + 0xC, flag0)
    check(code, destination, flag1)

# Int/Float and Float/Int preserve conversion order. A false flag keeps the
# second toInteger, including the no-dot string path in either operand order.
check(code, 0x5C4, 0x7F23CB78)
call(0x5C8, "?toInteger@AptValue@@")
check(code, 0x5CC, 0x57EB063E, 0x7C7F1B78, 0x2B0B0000, 0x7F43D378)
assert target(code, 0x5DC, 0x409A) == 0x5F4
call(0x5E0, "?toInteger@AptValue@@")
check(code, 0x600, 0xFC00069C, 0xFFE00018)
assert target(code, 0x608, 0x4800) == 0x688
call(0x688, "?toFloat@AptValue@@")

check(code, 0x63C, 0x7F43D378)
call(0x640, "?toInteger@AptValue@@")
check(code, 0x644, 0x57CB063E, 0x7C7F1B78, 0x2B0B0000, 0x7F23CB78)
assert target(code, 0x654, 0x419A) == 0x1D0
call(0x1D0, "?toInteger@AptValue@@")
call(0x658, "?toFloat@AptValue@@")
check(code, 0x66C, 0xFC00069C, 0xFC200018)
assert target(code, 0x674, 0x4800) == 0x68C
check(code, 0x68C, 0xEC1F0828, 0x3D600000, 0xFDA00210, 0xC00B0000)
assert references[0x690] == references[0x698] == "__real@3a83126f"

# Defined Float/Float keeps its separate exact equality, with no tolerance.
check(code, 0x1E4, 0x2F1C0006)
check(code, 0x1EC, 0x2F1B0006)
check(code, 0x1F4, 0x57AB2FFE, 0x2B0B0000)
check(code, 0x214, 0x7F23CB78)
call(0x218, "?toFloat@AptValue@@")
check(code, 0x21C, 0x7F43D378, 0xFFE00890)
call(0x224, "?toFloat@AptValue@@")
check(code, 0x228, 0xFF1F0800)
assert target(code, 0x22C, 0x419A) == 0x188

receipt = {
    "base": BASE,
    "source": SOURCE,
    "object_sha256": digest(obj),
    "compile_report_sha256": digest(REPORT.read_bytes()),
    "artist": {"Equals2": "0x82AFA368", "top_float_flag": "0x82AFAA08", "under_float_flag": "0x82AFAAA8"},
    "coff": {
        "top_float_flag_set": "0x520", "under_float_flag_set": "0x590",
        "no_dot_flag_clear": ["0x518", "0x588"],
        "mixed_float_conversions": ["0x658", "0x688"],
        "no_dot_integer_conversions": ["0x1d0", "0x5e0"],
        "float_float_exact_compare": "0x228",
        "mixed_tolerance": "0x3a83126f",
    },
    "stored_title_trace": {
        "file": "run-r106-title-font-diagnostic/host-output.log",
        "opcode_49_dispatch_lines": [337378, 337566],
        "numeric_coercion_operands": "not captured",
    },
    "runtime": "not run",
}
(ROOT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
