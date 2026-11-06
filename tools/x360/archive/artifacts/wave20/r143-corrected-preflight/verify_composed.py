"""Compare the composed R143 native object bodies with the frozen Apt proofs."""

import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
HEAD = "33204730e0de3476715c3f73e8231ae911f1a28a"
TREE = "8d44b165d78b7dbc7cf3d7480ed4dc2b86c076ef"
PARENT = "c76f007ddcd27ed3f73b1fe7f5a752c3116e9d03"
CLASS_PROOF = WAVE / "r143-apt-class-flags"
OPCODE_PROOF = WAVE / "r142-apt-composed-plus-count"
CLASS_RECEIPT = "eec281d0210ab14ff3443252b5bc1202bf610cb0a9558d4697a7d393e4722d95"
OPCODE_RECEIPT = "5883ba38dc127022bd333ead22a9ff1924d506f17c267c24791cd999320c458d"
CLASS_SECTIONS = {
    "AptObject": ("?GetHasClass@AptObject@@", "?SetHasClass@AptObject@@", "?SetImplementedObjects@",
                  "?GetImplementedObjects@", "?DoesImplementObject@"),
    "AptNativeFunction": ("??0AptNativeFunction@@", "??_GAptNativeFunction@@", "??_7AptNativeFunction@@"),
    "AptKey": ("??0AptKey@@", "??_GAptKey@@", "??_7AptKey@@"),
    "AptActionInterpreterStackOps": ("?_createObject@", "?_FunctionAptActionCallMethod@"),
    "AptValueFindChild": ("?AptResolveSpecialName@@",),
}
OPCODE_SECTIONS = {
    "AptActionDispatch": ("?InitDispatchTable@",),
    "AptActionInterpreter": ("?isObjectOfType@", "?stackPopAndPush@"),
    "AptActionInterpreterCompare": ("?_FunctionAptActionEquals2@", "?IsDefinedStringType@",
                                    "?_FunctionAptActionStrictEquals@"),
    "AptActionInterpreterMemberOps": ("?_FunctionAptActionInstanceOf@", "?_FunctionAptActionCastOp@"),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stable(name):
    return re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)


def read_sections(data, prefixes):
    machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional == 0
    strings = table + count * 18
    symbols, aliases = {}, {}
    index = 0
    while index < count:
        pos = table + index * 18
        name, value, section, _, storage, aux = struct.unpack_from("<8sIhHBB", data, pos)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        name = name.rstrip(b"\0").decode("ascii", errors="replace")
        symbols[index] = name, value, section
        if storage == 105 and aux == 1:
            aliases[name] = struct.unpack_from("<I", data, pos + 18)[0]
        index += 1 + aux

    result = {}
    for prefix in prefixes:
        [(name, value, section)] = [row for row in symbols.values() if row[0].startswith(prefix) and row[2] > 0]
        header = 20 + 40 * (section - 1)
        section_name = b".rdata" if prefix.startswith("??_7") else b".text"
        assert value == 0 and data[header:header + len(section_name)] == section_name
        size, offset, relocs = struct.unpack_from("<III", data, header + 16)
        count = struct.unpack_from("<H", data, header + 32)[0]
        refs = {}
        for index in range(count):
            off, symbol, kind = struct.unpack_from("<IIH", data, relocs + index * 10)
            target, target_value, target_section = symbols[symbol]
            if kind == 0x12:
                refs[off, kind] = symbol  # PPC PAIR addend
            elif target_section == section and target.startswith("$LN"):
                refs[off, kind] = target_value
            else:
                refs[off, kind] = stable(target)
        result[prefix] = stable(name), data[offset:offset + size], refs
    return result, {stable(name): stable(symbols[target][0]) for name, target in aliases.items()}


def checked_receipt(folder, digest):
    data = (folder / "RECEIPT.json").read_bytes()
    assert sha(data) == digest
    return json.loads(data)


def git(root, *args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(root), *args], text=True).strip()


report_path = ROOT / "compile-results.json"
report = json.loads(report_path.read_text())
rows = {Path(row["source"]).stem: row for row in report["results"]}
assert report["state"] == "complete" and (report["requested"], report["passed"], report["failed"]) == (11, 11, 0)
assert rows.keys() == (CLASS_SECTIONS | OPCODE_SECTIONS).keys() | {"AptCIH", "AptDisplayList"}
assert all(row["passed"] and row["machine"] == 0x1F2 for row in rows.values())
src_arg = next(arg for arg in next(iter(rows.values()))["command"] if arg.startswith("/I"))
assert all(src_arg in row["command"] for row in rows.values())
source_root = Path(src_arg[2:])
assert source_root.name == "src"
checkout = source_root.parent
assert git(checkout, "rev-parse", "HEAD", "HEAD^", "HEAD^{tree}").splitlines() == [HEAD, PARENT, TREE]
assert not git(checkout, "diff", "HEAD", "--name-only")

class_receipt = checked_receipt(CLASS_PROOF, CLASS_RECEIPT)
opcode_receipt = checked_receipt(OPCODE_PROOF, OPCODE_RECEIPT)
preserved = {}
objects = {}
for stem, prefixes in (CLASS_SECTIONS | OPCODE_SECTIONS).items():
    row = rows[stem]
    new_path = Path(row["object"])
    if stem in CLASS_SECTIONS:
        old_path = CLASS_PROOF / "native" / new_path.name
        old_sha = class_receipt["objects_sha256"][stem]
    else:
        old_path = OPCODE_PROOF / "native" / new_path.name
        old_sha = opcode_receipt["artifacts"][f"native\\{new_path.name}"]
    old, new = old_path.read_bytes(), new_path.read_bytes()
    assert sha(old) == old_sha, stem
    old_sections, old_aliases = read_sections(old, prefixes)
    new_sections, new_aliases = read_sections(new, prefixes)
    for prefix, (name, code, refs) in new_sections.items():
        assert (name, code, refs) == old_sections[prefix], (stem, prefix)
        if prefix.startswith("??_7"):
            dtor = refs[0x38, 2]
            assert new_aliases[dtor] == old_aliases[dtor]
        preserved[name] = {"bytes": len(code), "relocations": len(refs)}
    objects[stem] = sha(new)

# The dispatch COMDAT equality includes each store and its function relocation.
dispatch_path = Path(rows["AptActionDispatch"]["object"])
[(_, dispatch, relocations)] = read_sections(dispatch_path.read_bytes(), ("?InitDispatchTable@",))[0].values()
def word(offset):
    return struct.unpack_from(">I", dispatch, offset)[0]


opcodes = {}
for opcode, handler, high, low in (
    (0x49, "_FunctionAptActionEquals2", 0x4FC, 0x5F0),
    (0x54, "_FunctionAptActionInstanceOf", 0x504, 0x5FC),
    (0x66, "_FunctionAptActionStrictEquals", 0x514, 0x614),
):
    symbol = relocations[high, 0x10]
    assert symbol.startswith(f"?{handler}@AptActionInterpreter@@") and relocations[low, 0x11] == symbol
    assert word(high) >> 16 == 0x3D20 and word(low) >> 16 == 0x394A
    frame_offset = word(high + 4) & 0xFFFF
    assert word(high + 4) == 0x91210000 | frame_offset
    assert word(low - 4) == 0x81410000 | frame_offset
    assert word(low + 4) == 0x914B0000 | (opcode * 4)
    opcodes[hex(opcode)] = {"handler": handler, "initializer_store": hex(low + 4)}

receipt = {
    "head": HEAD, "tree": TREE, "parent": PARENT,
    "compile_report_sha256": sha(report_path.read_bytes()), "compiled": report["passed"],
    "frozen_class_receipt_sha256": CLASS_RECEIPT, "frozen_opcode_receipt_sha256": OPCODE_RECEIPT,
    "objects_sha256": objects, "preserved_sections": preserved, "dispatch": opcodes,
    "stage": "compile-only",
}
output = ROOT / "R143_COMPOSED_COFF.json"
with output.open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(output, sha(output.read_bytes()))
