import hashlib
import json
from pathlib import Path
import struct
import sys


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import x360rd


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_function(row, prefix):
    data = (ROOT / "native" / Path(row["object"]).name).read_bytes()
    machine, _, _, table, count, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional_size == 0
    strings = table + count * 18
    symbols = {}
    index = 0
    while index < count:
        position = table + index * 18
        name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, position)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind
        index += 1 + aux

    [(name, value, section, kind)] = [
        symbol for symbol in symbols.values() if symbol[0].startswith(prefix) and symbol[2] > 0
    ]
    assert value == 0 and kind == 0x20
    header = 20 + (section - 1) * 40
    assert data[header:header + 5] == b".text"
    size, offset, relocations = struct.unpack_from("<III", data, header + 16)
    count = struct.unpack_from("<H", data, header + 32)[0]
    references = {}
    for index in range(count):
        address, symbol, kind = struct.unpack_from("<IIH", data, relocations + 10 * index)
        if kind != 0x12:
            references[address] = symbols[symbol][0]
    return name, struct.unpack_from(f">{size // 4}I", data, offset), references


def check(code, offset, *words):
    assert code[offset // 4:offset // 4 + len(words)] == words, hex(offset)


def check_reference(references, offset, prefix):
    assert references[offset].startswith(prefix), (hex(offset), references[offset])


def excerpt(code, references, first, last, base=0):
    return {
        f"{base + offset:08x}": f"{code[offset // 4]:08x}" + (
            f" {references[offset]}" if offset in references else ""
        )
        for offset in range(first, last + 1, 4)
    }


report_path = ROOT / "native" / "compile-results.json"
report = json.loads(report_path.read_text())
assert report["passed"] == 4 and report["failed"] == 0
rows = {Path(row["source"]).stem: row for row in report["results"]}
assert set(rows) == {
    "AptActionInterpreter", "AptActionInterpreterMemberOps", "AptActionDispatch", "AptActionRun"
}

type_test, helper, helper_refs = read_function(
    rows["AptActionInterpreter"], "?isObjectOfType@AptActionInterpreter@@SAH"
)
_, cast, cast_refs = read_function(rows["AptActionInterpreterMemberOps"], "?_FunctionAptActionCastOp@")
_, instance, instance_refs = read_function(
    rows["AptActionInterpreterMemberOps"], "?_FunctionAptActionInstanceOf@"
)
_, dispatch, dispatch_refs = read_function(rows["AptActionDispatch"], "?InitDispatchTable@")

# Both handlers pass the two value registers directly to the static symbol.
check(cast, 0x48, 0x83CAFFFC, 0x7C89582E, 0x7FC3F378, 0x4BFFFFAD, 0x546B063E)
check_reference(cast_refs, 0x54, type_test)
check(instance, 0x40, 0x7C69582E, 0x808AFFFC, 0x4BFFFFB9, 0x546B063E)
check_reference(instance_refs, 0x48, type_test)

# The underflow branch takes the live count in r4 to the common collapse call.
# The normal path tests the low byte for 1, resolves the singleton, and sets r4=2.
check(instance, 0x14, 0x809F0000, 0x2F040002, 0x40980010)
check(instance, 0x28, 0x48000040)
check_reference(instance_refs, 0x24, "?gpUndefinedValue@@")
check(instance, 0x50, 0x396BFFFF, 0x7D6B0034, 0x5563DFFE, 0x4BFFFFA5)
check_reference(instance_refs, 0x5C, "?Create@AptBoolean@@")
check(instance, 0x64, 0x38800002, 0x7FE3FB78, 0x4BFFFF95)
check_reference(instance_refs, 0x6C, "?stackPopAndPush@AptActionInterpreter@@")
check(dispatch, 0x5FC, 0x394A0000, 0x914B0150)
check_reference(dispatch_refs, 0x504, "?_FunctionAptActionInstanceOf@")
check_reference(dispatch_refs, 0x5FC, "?_FunctionAptActionInstanceOf@")

# Virtual slots, raw tags, and current-before-next branch witnesses.
check(helper, 0x18, 0x816B000C)
check(helper, 0x38, 0x816B000C)
check(helper, 0x58, 0x816B0008)
check(helper, 0x68, 0x83A3000C)
check(helper, 0x74, 0x2F0B000C)
check(helper, 0xFC, 0x2F0B0025)
check(helper, 0x128, 0x7F03E840, 0x409A0008, 0x3BC00001)
check(helper, 0x138, 0x816B0008)
check(helper, 0x144, 0x80630008, 0x2B030000, 0x409AFFDC)
check(helper, 0x9C, 0x83E30008, 0x2B1F0000, 0x419A004C, 0x7F1FE840, 0x419A00DC)
check(helper, 0xB8, 0x816B000C)
check(helper, 0xD8, 0x816B0008)
check(helper, 0x15C, 0x2F0B0013)
check_reference(helper_refs, 0x178, "?DoesImplementObject@AptObject@@")
check(helper, 0x1A0, 0x2F090022)
check(helper, 0x1B4, 0x2F0B0023)
check(helper, 0x1BC, 0x2F0B0024)
check(helper, 0x1E8, 0x2F090013)

# Original dispatcher points directly to the opcode body.
assert x360rd.u32(0x82815570) == 0x7C8A2378 and x360rd.u32(0x82F264A8) == 4
assert x360rd.u32(0x82F73068 + 0x54 * 4) == 0x82AEA788
artist_instance = struct.unpack(">35I", x360rd.rd(0x82AEA788, 0x8C))
artist_helper = struct.unpack(">116I", x360rd.rd(0x82AEA5B8, 0x1D0))
artist_collapse = struct.unpack(">40I", x360rd.rd(0x82ADBAB8, 0xA0))
check(artist_instance, 0x14, 0x809F0000, 0x2F040002, 0x40980010)
check(artist_instance, 0x28, 0x48000048)
check(artist_instance, 0x40, 0x7C69582E, 0x808AFFFC, 0x4BFFFDE9, 0x546B063E)
check(artist_instance, 0x6C, 0x38800002, 0x7FE3FB78)

result = {
    "base": "bdbd812370b68b9d3fd1b3b4f27f6fa3d7632704",
    "candidate_commit": "8d170ac38aeb8663983a757fea49ca7660f411ab",
    "historical_reference": "898b4729147545bded35c2db8ef069417dc909bc",
    "scope": "XDK PPC COFF and ARTIST raw evidence. No strict link or runtime test.",
    "compile_report_sha256": digest(report_path.read_bytes()),
    "object_sha256": {
        stem: digest((ROOT / "native" / Path(row["object"]).name).read_bytes())
        for stem, row in rows.items()
    },
    "candidate": {
        "dispatch_slot": excerpt(dispatch, dispatch_refs, 0x5FC, 0x600),
        "cast_arguments": excerpt(cast, cast_refs, 0x48, 0x58),
        "instanceof": excerpt(instance, instance_refs, 0x14, 0x6C),
        "type_test_virtual_and_prototype": excerpt(helper, helper_refs, 0x14, 0x74),
        "type_test_generic_current_link": excerpt(helper, helper_refs, 0x9C, 0xE8),
        "type_test_cih_loop": excerpt(helper, helper_refs, 0xFC, 0x14C),
        "type_test_object": excerpt(helper, helper_refs, 0x15C, 0x178),
    },
    "artist": {
        "dispatch_slot_82f731b8": "82aea788",
        "instanceof": excerpt(artist_instance, {}, 0x14, 0x74, base=0x82AEA788),
        "type_test_virtual_and_prototype": excerpt(artist_helper, {}, 0x18, 0x64, base=0x82AEA5B8),
        "type_test_cih_loop": excerpt(artist_helper, {}, 0x98, 0xCC, base=0x82AEA5B8),
        "type_test_generic_current_link": excerpt(artist_helper, {}, 0x108, 0x14C, base=0x82AEA5B8),
        "stack_collapse_guard": excerpt(artist_collapse, {}, 0x18, 0x20, base=0x82ADBAB8),
    },
    "remaining_differences": [
        "The candidate calls the existing AptBoolean::Create singleton selector.",
        "ARTIST loads the singleton directly in the handler.",
        "The type-test instruction layout differs.",
        "The inherited AptObject interface count layout is outside this recovery.",
    ],
}
print(json.dumps(result, indent=2))
