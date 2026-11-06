"""Check the Apt class masks and inherited layout in Xbox 360 XDK COFF."""

import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PARENT = "0d5bbd3aa196213d7f7c0e032f7eb1ee6a1e7622"
REPORT = ROOT / "native" / "compile-results.json"
ARTIST = ROOT.parent / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex"


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Coff:
    def __init__(self, path):
        self.data = data = path.read_bytes()
        machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", data)
        assert machine == 0x1F2 and optional == 0
        strings = table + count * 18
        self.symbols = {}
        weak = {}
        index = 0
        while index < count:
            position = table + index * 18
            name, value, section, _, storage, aux = struct.unpack_from("<8sIhHBB", data, position)
            if name[:4] == bytes(4):
                start = strings + struct.unpack_from("<I", name, 4)[0]
                name = data[start:data.index(b"\0", start)]
            name = name.rstrip(b"\0").decode("ascii", errors="replace")
            self.symbols[index] = name, value, section
            if storage == 105 and aux == 1:
                weak[name] = struct.unpack_from("<I", data, position + 18)[0]
            index += 1 + aux
        self.aliases = {name: self.symbols[target][0] for name, target in weak.items()}

    def section(self, prefix, section_name):
        [(_, value, section)] = [s for s in self.symbols.values() if s[0].startswith(prefix) and s[2] > 0]
        header = 20 + 40 * (section - 1)
        assert value == 0 and self.data[header:header + len(section_name)] == section_name
        size, offset, relocs = struct.unpack_from("<III", self.data, header + 16)
        count = struct.unpack_from("<H", self.data, header + 32)[0]
        refs = {}
        for index in range(count):
            off, symbol, kind = struct.unpack_from("<IIH", self.data, relocs + index * 10)
            if kind != 0x12:
                refs[off] = self.symbols[symbol][0]
        return struct.unpack_from(f">{size // 4}I", self.data, offset), refs


def check(code, offset, *words):
    assert code[offset // 4:offset // 4 + len(words)] == words, hex(offset)


report = json.loads(REPORT.read_text())
plan = json.loads((ROOT / "compile-plan.json").read_text())
assert report["failed"] == 0 and report["passed"] == len(plan["sources"])
assert {row["source"] for row in report["results"]} == set(plan["sources"])
assert all(row["passed"] and row["machine"] == 0x1F2 for row in report["results"])
objects = {Path(row["source"]).stem: Coff(Path(row["object"])) for row in report["results"]
           if row["source"] in plan["changed"]}

artist = ARTIST.read_bytes()
assert sha(artist) == "6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655"
payload = struct.unpack_from(">I", artist, 8)[0]


def original(address, count):
    return struct.unpack_from(f">{count}I", artist, payload + address - 0x82000000)


assert original(0x82AF0068, 3) == (0x8163001C, 0x55634FFE, 0x4E800020)
assert original(0x82AF0058, 2) == (0x516ABA10, 0x9143001C)

obj = objects["AptObject"]
get_has_class, _ = obj.section("?GetHasClass@AptObject@@", b".text")
assert get_has_class == original(0x82AF0068, 3)
set_has_class, _ = obj.section("?SetHasClass@AptObject@@", b".text")
check(set_has_class, 0x04, 0x3D600080)
check(set_has_class, 0x10, 0x8143001C, 0x554A024E, 0x7D4B5B78, 0x9163001C)
for method, offsets in {
    "SetImplementedObjects": [(0x3C, 0x9BBF001C)],
    "GetImplementedObjects": [(0x14, 0x897F001C)],
    "DoesImplementObject": [(0x68, 0x897E001C), (0x90, 0x891E001C)],
}.items():
    body, _ = obj.section(f"?{method}@AptObject@@", b".text")
    for offset, word in offsets:
        check(body, offset, word)

stack = objects["AptActionInterpreterStackOps"]
create, _ = stack.section("?_createObject@", b".text")
check(create, 0x804, 0x816B0014, 0x7D6903A6, 0x4E800421)
check(create, 0x810, 0x817E001C, 0x815E0000, 0x7FC3F378, 0x656B0040, 0x814A0008, 0x917E001C)
check(create, 0x96C, 0x817E001C, 0x7FC3F378, 0x556B0290, 0x917E001C)
assert create.count(0x816B0014) == 1  # one virtual SetHasClass dispatch
call, _ = stack.section("?_FunctionAptActionCallMethod@", b".text")
check(call, 0x5F8, 0x817E001C, 0x656B0040, 0x917E001C)
check(call, 0x808, 0x814B001C, 0x554A0290, 0x914B001C)
assert 0x816B0014 not in call
child, child_refs = objects["AptValueFindChild"].section("?AptResolveSpecialName@@", b".text")
check(child, 0x300, 0x817F001C, 0x556B57FE)
check(child, 0x3E0, 0x816B0010, 0x7D6903A6, 0x4E800421)
assert child_refs[0x3B0].startswith("?IsInCtor@AptCIH@@")

tables = {}
for name, address, lookup, ctor_word, init_offset, ctor_call, delete_offset, delete_word in (
    ("AptNativeFunction", 0x82145A14, "AptObject", (0x18, 0x38800009), 0x54,
     (0x5C, "?SetAllowDelayedDeletion@AptValue@@"), 0x3C, 0x38800024),
    ("AptKey", 0x82145B10, "AptKey", (0x10, 0x38800010), 0x48,
     (0x50, "?setGCRoot@AptValue@@"), 0x48, 0x38A00020),
):
    obj = objects[name]
    table, refs = obj.section(f"??_7{name}@@", b".rdata")
    assert len(table) == 15 and len(refs) == 15
    expected_lookup = f"?objectMemberLookup@{lookup}@@"
    for offset, expected in [(0x10, "?GetHasClass@AptObject@@"),
                             (0x14, "?SetHasClass@AptObject@@"),
                             (0x18, expected_lookup)]:
        assert refs[offset].startswith(expected), (name, offset, refs[offset])
    assert refs[0x38].startswith((f"??_E{name}@@", f"??_G{name}@@"))
    assert original(address + 0x10, 3) == (0x82AF0068, 0x82AF0048,
                                          0x82AD6480 if lookup == "AptObject" else 0x82AF8780)
    ctor, ctor_refs = obj.section(f"??0{name}@@", b".text")
    check(ctor, *ctor_word)
    check(ctor, init_offset, 0x915F001C)
    assert ctor_refs[ctor_call[0]].startswith(ctor_call[1])
    if name == "AptNativeFunction":
        check(ctor, 0x40, 0x93DF0020)  # callback register into object +0x20
    dtor_name = obj.aliases.get(refs[0x38], refs[0x38])
    assert dtor_name.startswith(f"??_G{name}@@")
    dtor, dtor_refs = obj.section(dtor_name, b".text")
    assert dtor_refs[0x28] == "??1AptObject@@UAA@XZ"
    check(dtor, delete_offset, delete_word)
    tables[name] = {"artist": hex(address), "slots": {hex(k): refs[k] for k in (0x10, 0x14, 0x18, 0x38)},
                    "delete_size": delete_word & 0xFFFF}

receipt = {
    "base": PARENT,
    "compile_report_sha256": sha(REPORT.read_bytes()),
    "compiled": report["passed"],
    "objects_sha256": {name: sha(obj.data) for name, obj in objects.items()},
    "class_word": {"offset": "0x1c", "has_class": "0x00800000", "main_instance": "0x00400000",
                   "count": "byte access verified in all three methods"},
    "main_instance_sites": {"create": ["0x81c", "0x974"], "call_method": ["0x5fc", "0x80c"],
                            "find_child": ["0x304", "0x3e0"]},
    "inherited_tables": tables,
    "native_callback_offset": "0x20",
    "cih_constructor_state": "uses the existing AptCIH::IsInCtor accessor",
    "runtime": "not run",
}
(ROOT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
