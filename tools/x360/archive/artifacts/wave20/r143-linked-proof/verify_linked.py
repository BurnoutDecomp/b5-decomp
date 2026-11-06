"""Verify the corrected R143 Apt providers in a sealed Xbox 360 PE and map."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import pefile


ROOT = Path(__file__).resolve().parent
EXPECTED_TREE = "8d44b165d78b7dbc7cf3d7480ed4dc2b86c076ef"
PREFLIGHT_HEAD = "33204730e0de3476715c3f73e8231ae911f1a28a"
INTEGRATED_R142 = "c76f007ddcd27ed3f73b1fe7f5a752c3116e9d03"
PREFLIGHT = ROOT.parent / "r143-corrected-preflight"
PREFLIGHT_CANDIDATE_SHA = "3dda0c764bd79ff0b2b7e956183b84668cfc67a2473c38d7b67a6f758ff39842"
PREFLIGHT_REPORT_SHA = "d54d1abf5bf314caacc0e4b76cb967bf306f5dd03307b0f26af3f5476109dfc6"
PREFLIGHT_OBJECTS = {
    "AptActionDispatch": "fefcde1603174220f54baa6c08b789479e3726a3450361ece7817db76174dece",
    "AptActionInterpreter": "5efdb39852b5a055c22ba675d7e0d072272685eddb147ed11eb5b75a7e0c203d",
    "AptActionInterpreterCompare": "740d522b4aaac87a7813e72d683f52286dd92f3feda55d7c0586a28ce24dbb14",
    "AptActionInterpreterMemberOps": "cbdd663d02f90838ed5aa64c904a03cb2507861cc1a6dd7cc3baea05fbbcd2b7",
    "AptActionInterpreterStackOps": "3f7a6c06e82529c1c9bf7013649a0a258e38bce0883c851baf60e2a2e60ec827",
    "AptCIH": "b930154f4bcc94d7c6907b5379a061f59b81d938d01a08e6b43844059998579b",
    "AptDisplayList": "91ee97277cc9960a139c547cb9df18ac82c71e29bb7fd4552b114abe38367423",
    "AptKey": "fd1610f5e39857f78e03d2fcdab3f7ca9953ed6540d50e6205f65add62b9dad3",
    "AptNativeFunction": "daf8dd7247bb36523d1533a5878043a0772635df19ce7f9f5b0670f428990e47",
    "AptObject": "918f7fdfe634bf1dd24a1dc98f40bc5407b93d294245c406dae15dbaca4b7fcc",
    "AptValueFindChild": "2f3cfd58dc3bad741a9f1f2e6cfed31bfd7af7cc8cfe13d66b4f704ee3719df9",
}
APT = "src/SDKs/EATech/include/Apt"
FUNCTIONS = {
    "AptActionDispatch": ("InitDispatchTable",),
    "AptActionInterpreter": ("isObjectOfType", "stackPopAndPush"),
    "AptActionInterpreterCompare": ("_FunctionAptActionEquals2", "IsDefinedStringType", "_FunctionAptActionStrictEquals"),
    "AptActionInterpreterMemberOps": ("_FunctionAptActionInstanceOf", "_FunctionAptActionCastOp"),
    "AptObject": ("SetImplementedObjects", "GetImplementedObjects", "DoesImplementObject", "GetHasClass", "SetHasClass"),
    "AptActionInterpreterStackOps": ("_createObject", "_FunctionAptActionCallMethod"),
    "AptKey": ("AptKeyCtor", "AptKeyDtor", "AptKeyTable"),
    "AptNativeFunction": ("AptNativeFunctionCtor", "AptNativeFunctionDtor", "AptNativeFunctionTable"),
    "AptValueFindChild": ("AptResolveSpecialName",),
    "AptCIH": ("IsInCtor", "SetInCtor"),
    "AptDisplayList": ("AssociateInstToClass",),
}
PREFIX = {
    "GetHasClass": "?GetHasClass@AptObject@@",
    "SetHasClass": "?SetHasClass@AptObject@@",
    "AptKeyCtor": "??0AptKey@@",
    "AptKeyDtor": "??_GAptKey@@",
    "AptKeyTable": "??_7AptKey@@",
    "AptNativeFunctionCtor": "??0AptNativeFunction@@",
    "AptNativeFunctionDtor": "??_GAptNativeFunction@@",
    "AptNativeFunctionTable": "??_7AptNativeFunction@@",
}
DISPATCH = {
    0x49: ("_FunctionAptActionEquals2", 0x4FC, 0x5F0),
    0x54: ("_FunctionAptActionInstanceOf", 0x504, 0x5FC),
    0x66: ("_FunctionAptActionStrictEquals", 0x514, 0x614),
}
TABLE_SYMBOL = "?sGlobalTable@AptActionInterpreter@@2PAP6AXPAV1@PAULocalContextT@1@@ZA"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stable_name(name):
    return re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)


def coff_functions(data, names):
    machine, _, _, table, count, optional, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optional == 0
    strings = table + 18 * count
    symbols = {}
    weak_aliases = {}
    index = 0
    while index < count:
        position = table + 18 * index
        name, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, position)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind
        if storage == 105:
            assert aux == 1
            weak_aliases[index] = struct.unpack_from("<I", data, position + 18)[0]
        index += 1 + aux

    functions = {}
    for method in names:
        prefix = PREFIX.get(method, f"?{method}@")
        [(name, value, section, kind)] = [
            row for row in symbols.values() if row[0].startswith(prefix) and row[2] > 0
        ]
        is_table = prefix.startswith("??_7")
        assert value == 0 and kind == (0 if is_table else 0x20), name
        header = 20 + 40 * (section - 1)
        expected_section = b".rdata" if is_table else b".text"
        assert data[header:header + len(expected_section)] == expected_section, name
        size, offset, relocations = struct.unpack_from("<III", data, header + 16)
        count = struct.unpack_from("<H", data, header + 32)[0]
        bindings = {}
        for index in range(count):
            off, symbol, kind = struct.unpack_from("<IIH", data, relocations + 10 * index)
            if kind == 0x12:  # The frozen PPC address pairs carry a zero addend.
                assert symbol == 0 and bindings[off][0] in (0x10, 0x11), (name, hex(off))
                continue
            target, value, target_section, _ = symbols[symbol]
            local = value if target_section == section and target.startswith("$LN") else None
            alias = symbols[weak_aliases[symbol]][0] if symbol in weak_aliases else None
            assert off not in bindings
            bindings[off] = kind, target, local, alias
        functions[method] = name, data[offset:offset + size], bindings
    return functions


def canonical_bindings(bindings):
    return {
        offset: (kind, stable_name(target) if local is None else local, stable_name(alias) if alias else None)
        for offset, (kind, target, local, alias) in bindings.items()
    }


def read_map(data):
    entries = {}
    for line in data.decode(errors="replace").splitlines():
        row = re.match(r"\s+[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8})\s+(.+)", line)
        if row:
            entries.setdefault(row[1], set()).add((int(row[2], 16), row[3].split()[-1]))
    return entries


def map_address(entries, symbol, provider=None):
    rows = entries.get(symbol, set())
    addresses = {addr for addr, owner in rows if provider is None or owner.endswith(provider)}
    assert len(addresses) == 1, (symbol, provider, rows)
    return addresses.pop()


def signed(value, bits):
    sign = 1 << (bits - 1)
    return (value ^ sign) - sign


def word(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def expect_words(data, offset, *words):
    assert struct.unpack_from(f">{len(words)}I", data, offset) == words, hex(offset)


def verify_function(entries, read, address, code, bindings):
    linked = read(address, len(code))
    expected = bytearray(code)
    bound = {}
    for offset, (kind, symbol, local, alias) in bindings.items():
        location = address + offset
        if local is not None:
            destination = address + local
        elif alias is not None:
            destination = map_address(entries, alias)
            if symbol in entries:
                assert map_address(entries, symbol) == destination, (symbol, alias)
        else:
            destination = map_address(entries, symbol)
        original = word(code, offset)
        if kind == 0x06:
            assert original & 0xFC000002 == 0x48000000, (hex(location), symbol)
            value = original & 0xFC000003 | ((destination - location) & 0x03FFFFFC)
            assert location + signed(value & 0x03FFFFFC, 26) == destination, (hex(location), symbol)
        elif kind == 0x10:
            assert original & 0xFFFF == 0, (hex(location), symbol)
            value = original | (((destination + 0x8000) >> 16) & 0xFFFF)
        elif kind == 0x11:
            assert original & 0xFFFF == 0, (hex(location), symbol)
            value = original | (destination & 0xFFFF)
        elif kind == 0x02:
            assert original == 0, (hex(location), symbol)
            value = destination
        else:
            raise AssertionError((hex(location), kind, symbol))
        assert word(linked, offset) == value, (hex(location), symbol, hex(word(linked, offset)), hex(value))
        struct.pack_into(">I", expected, offset, value)
        bound[hex(offset)] = {"symbol": symbol, "target": hex(destination), "type": hex(kind)}
        if alias is not None:
            bound[hex(offset)]["alias_target"] = alias
    assert linked == expected, f"Fixed PPC bytes differ at {address:#x}"
    return linked, {"address": hex(address), "bytes": len(code), "sha256": sha(linked), "bindings": bound}


def verify_dispatch(linked, proofs, entries):
    code = linked["InitDispatchTable"]
    init = int(proofs["InitDispatchTable"]["address"], 16)
    bindings = proofs["InitDispatchTable"]["bindings"]
    table = map_address(entries, TABLE_SYMBOL)
    assert int(bindings["0x8"]["target"], 16) == table and bindings["0x8"]["type"] == "0x10"
    assert int(bindings["0x10"]["target"], 16) == table and bindings["0x10"]["type"] == "0x11"
    assert word(code, 0x8) >> 16 == 0x3D60 and word(code, 0x10) >> 16 == 0x396B
    slots = {}
    for opcode, (handler, high, low) in DISPATCH.items():
        addr = int(proofs[handler]["address"], 16)
        high_binding = bindings[hex(high)]
        low_binding = bindings[hex(low)]
        assert high_binding["symbol"] == low_binding["symbol"] == proofs[handler]["symbol"]
        assert high_binding["type"] == "0x10" and low_binding["type"] == "0x11"
        assert int(high_binding["target"], 16) == int(low_binding["target"], 16) == addr
        assert word(code, high) >> 16 == 0x3D20 and word(code, low) >> 16 == 0x394A
        frame_offset = word(code, high + 4) & 0xFFFF
        assert word(code, high + 4) == 0x91210000 | frame_offset
        assert word(code, low - 4) == 0x81410000 | frame_offset
        expect_words(code, low + 4, 0x914B0000 | (opcode * 4))
        slots[hex(opcode)] = {
            "initializer_store": hex(init + low + 4), "slot": hex(table + opcode * 4), "handler": hex(addr),
        }
    return slots


def verify_paths(linked, proofs, entries, read):
    def target(method, offset, expected):
        binding = proofs[method]["bindings"][hex(offset)]
        assert binding["symbol"] == proofs[expected]["symbol"], (method, offset, binding)
        assert binding["target"] == proofs[expected]["address"], (method, offset, binding)

    helper = linked["isObjectOfType"]
    assert "@@SAHPAVAptValue@@" in proofs["isObjectOfType"]["symbol"]
    expect_words(helper, 0xC, 0x7C7F1B78, 0x7C9E2378)
    expect_words(helper, 0x18, 0x816B000C)
    expect_words(helper, 0x38, 0x816B000C)
    expect_words(helper, 0x68, 0x83A3000C)
    expect_words(helper, 0xA8, 0x7F1FE840, 0x419A00DC)
    expect_words(helper, 0xB8, 0x816B000C)
    expect_words(helper, 0xD8, 0x816B0008)
    expect_words(helper, 0x128, 0x7F03E840, 0x409A0008, 0x3BC00001)
    expect_words(helper, 0x144, 0x80630008, 0x2B030000, 0x409AFFDC)
    target("isObjectOfType", 0x178, "DoesImplementObject")

    instance = linked["_FunctionAptActionInstanceOf"]
    expect_words(instance, 0x14, 0x809F0000, 0x2F040002, 0x40980010)
    expect_words(instance, 0x28, 0x48000040)
    expect_words(instance, 0x40, 0x7C69582E, 0x808AFFFC)
    expect_words(instance, 0x64, 0x38800002, 0x7FE3FB78)
    target("_FunctionAptActionInstanceOf", 0x48, "isObjectOfType")
    target("_FunctionAptActionInstanceOf", 0x6C, "stackPopAndPush")
    collapse = linked["stackPopAndPush"]
    expect_words(collapse, 0x18, 0x817E0000, 0x7F0BE800, 0x41980078)

    cast = linked["_FunctionAptActionCastOp"]
    expect_words(cast, 0x48, 0x83CAFFFC, 0x7C89582E, 0x7FC3F378)
    target("_FunctionAptActionCastOp", 0x54, "isObjectOfType")
    target("_FunctionAptActionCastOp", 0x2C, "stackPopAndPush")
    target("_FunctionAptActionCastOp", 0x70, "stackPopAndPush")

    strict = proofs["_FunctionAptActionStrictEquals"]["bindings"]
    assert strict["0x3b0"]["symbol"].startswith("?EqualNoCase@EAStringC@@")
    assert strict["0x3c4"]["symbol"].startswith("?Create@AptBoolean@@")
    target("_FunctionAptActionStrictEquals", 0x3D4, "stackPopAndPush")
    assert strict["0x370"]["symbol"] == strict["0x378"]["symbol"] == "__real@3a83126f"
    epsilon = map_address(entries, "__real@3a83126f")
    assert read(epsilon, 4) == bytes.fromhex("3a83126f")
    jump_table = {off: row["target"] for off, row in strict.items() if row["type"] == "0x2"}
    assert len(jump_table) == 33

    access_spec = {
        "SetImplementedObjects": (31, {0x3C: 0x9BBF001C}),
        "GetImplementedObjects": (31, {0x14: 0x897F001C}),
        "DoesImplementObject": (30, {0x68: 0x897E001C, 0x90: 0x891E001C}),
    }
    count_accesses = {}
    for method, (base, expected) in access_spec.items():
        accesses = {}
        for offset in range(0, len(linked[method]), 4):
            value = word(linked[method], offset)
            if 32 <= value >> 26 <= 45 and (value >> 16) & 31 == base and 0x1C <= value & 0xFFFF <= 0x1F:
                accesses[offset] = value
        assert accesses == expected, (method, accesses)
        address = int(proofs[method]["address"], 16)
        count_accesses[method] = {hex(address + off): f"{value:08x}" for off, value in accesses.items()}
    return {
        "type_test": "static r3 object / r4 class; virtual hash gate and current-before-next prototype loops",
        "underflow": "InstanceOf preserves live count in r4; stackPopAndPush guards top >= count",
        "cast_op": "reversed operand positions and static type-test call",
        "strict_equals": {"epsilon": hex(epsilon), "epsilon_word": "3a83126f", "jump_table_targets": jump_table},
        "interface_count_accesses": count_accesses,
    }


def verify_class_paths(linked, proofs):
    expect_words(linked["GetHasClass"], 0, 0x8163001C, 0x55634FFE, 0x4E800020)
    expect_words(linked["SetHasClass"], 4, 0x3D600080)
    expect_words(linked["SetHasClass"], 0x10, 0x8143001C, 0x554A024E, 0x7D4B5B78, 0x9163001C)

    create = linked["_createObject"]
    expect_words(create, 0x804, 0x816B0014, 0x7D6903A6, 0x4E800421, 0x817E001C)
    expect_words(create, 0x81C, 0x656B0040)
    expect_words(create, 0x824, 0x917E001C)
    expect_words(create, 0x974, 0x556B0290, 0x917E001C)
    call_method = linked["_FunctionAptActionCallMethod"]
    expect_words(call_method, 0x5F8, 0x817E001C, 0x656B0040, 0x917E001C)
    expect_words(call_method, 0x808, 0x814B001C, 0x554A0290, 0x914B001C)
    resolver = linked["AptResolveSpecialName"]
    expect_words(resolver, 0x300, 0x817F001C, 0x556B57FE)
    is_in_ctor_call = proofs["AptResolveSpecialName"]["bindings"]["0x3b0"]
    assert is_in_ctor_call["symbol"] == proofs["IsInCtor"]["symbol"]
    assert is_in_ctor_call["target"] == proofs["IsInCtor"]["address"]

    classes = {}
    for name, lookup, size_offset, size_instruction in (
        ("AptKey", "?objectMemberLookup@AptKey@@", 0x48, 0x38A00020),
        ("AptNativeFunction", "?objectMemberLookup@AptObject@@", 0x3C, 0x38800024),
    ):
        table = proofs[f"{name}Table"]
        slots = table["bindings"]
        dtor = proofs[f"{name}Dtor"]
        ctor = proofs[f"{name}Ctor"]
        assert len(linked[f"{name}Table"]) == 60 and len(slots) == 15
        for slot, method in (("0x10", "GetHasClass"), ("0x14", "SetHasClass")):
            assert slots[slot]["symbol"] == proofs[method]["symbol"]
            assert slots[slot]["target"] == proofs[method]["address"]
        assert slots["0x18"]["symbol"].startswith(lookup)
        assert slots["0x38"]["alias_target"] == dtor["symbol"]
        assert slots["0x38"]["target"] == dtor["address"]
        assert dtor["bindings"]["0x28"]["symbol"] == "??1AptObject@@UAA@XZ"
        assert any(ref["symbol"] == table["symbol"] for ref in ctor["bindings"].values())
        expect_words(linked[f"{name}Dtor"], size_offset, size_instruction)
        classes[name] = {
            "table": table["address"], "constructor": ctor["address"], "deleting_destructor": dtor["address"],
            "lookup": slots["0x18"], "base_destructor_call": dtor["bindings"]["0x28"],
            "allocation_size": size_instruction & 0xFFFF,
        }
    expect_words(linked["AptNativeFunctionCtor"], 0x40, 0x93DF0020)

    expect_words(linked["IsInCtor"], 0, 0x8163000C, 0x55632FFE, 0x4E800020)
    if "SetInCtor" in linked:
        expect_words(linked["SetInCtor"], 0, 0x8163000C, 0x508BD908, 0x9163000C, 0x4E800020)
    associate = linked["AssociateInstToClass"]
    expect_words(associate, 0x2C4, 0x817E000C)
    expect_words(associate, 0x2CC, 0x656B0800)
    expect_words(associate, 0x2DC, 0x7FC4F378)
    expect_words(associate, 0x2E4, 0x917E000C)
    expect_words(associate, 0x2EC, 0x817E000C, 0x556B0146, 0x917E000C)
    bracket = proofs["AssociateInstToClass"]
    call = bracket["bindings"]["0x2e8"]
    assert call["symbol"].startswith("?callFunction@AptActionInterpreter@@")
    bracket_address = int(bracket["address"], 16)
    return {
        "class_flags": {"has_class": "0x00800000", "in_main_instance": "0x00400000"},
        "class_tables": classes,
        "cih_constructor_flag": {
            "field": "0x0c", "mask": "0x08000000", "resolver_call": is_in_ctor_call,
            "associate_set": hex(bracket_address + 0x2E4), "associate_clear": hex(bracket_address + 0x2F4),
            "call_function": call,
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path)
    parser.add_argument("--expected-head", default=PREFLIGHT_HEAD)
    parser.add_argument("--output", type=Path, help="Defaults to R143_APT_LINKED.json next to this script")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_head):
        parser.error("--expected-head must be a full Git commit ID")

    link = args.link_dir.resolve()
    result = json.loads((link / "result.json").read_bytes())
    seal_bytes = (link / "seal.json").read_bytes()
    seal = json.loads(seal_bytes)
    invocation = json.loads((link / "invocation.json").read_bytes())
    image = json.loads((link / "image-verification.json").read_bytes())
    assert result["exit_code"] == 0 and result["all_post_checks_pass"] is True
    assert sha(seal_bytes) == invocation["sealed_sha256"]
    assert image["head"] == seal["head"] == args.expected_head

    git = ["git", "--no-optional-locks", "-C", seal["source_root"]]
    for head in (args.expected_head, PREFLIGHT_HEAD):
        tree = subprocess.check_output([*git, "rev-parse", f"{head}^{{tree}}"], text=True).strip()
        assert tree == EXPECTED_TREE, (head, tree)
    subprocess.run([*git, "merge-base", "--is-ancestor", INTEGRATED_R142, args.expected_head], check=True)

    def verified_output(name):
        [record] = [row for path, row in result["outputs"].items() if Path(path).name == name]
        data = (link / name).read_bytes()
        assert len(data) == record["size"] and sha(data) == record["sha256"], name
        return data

    xex = verified_output("native-game.exe")
    assert xex[:4] == b"XEX2" and sha(xex) == image["image_sha256"]
    pe = verified_output("native-game.pe")
    map_data = verified_output("native-game.map")
    entries = read_map(map_data)
    pe_image = pefile.PE(data=pe, fast_load=True)
    assert pe_image.FILE_HEADER.Machine == 0x1F2

    def read(address, size):
        rva = address - pe_image.OPTIONAL_HEADER.ImageBase
        section = next(s for s in pe_image.sections if s.VirtualAddress <= rva and
                       rva + size <= s.VirtualAddress + s.SizeOfRawData)
        offset = section.PointerToRawData + rva - section.VirtualAddress
        return pe[offset:offset + size]

    [rsp_name] = [arg[1:] for arg in invocation["command"] if arg.startswith("@")]
    rsp_bytes = (link / Path(rsp_name).name).read_bytes()
    assert sha(rsp_bytes) == invocation["rsp_sha256"]
    rsp = rsp_bytes.decode().splitlines()
    candidate_bytes = (PREFLIGHT / "CANDIDATE.json").read_bytes()
    assert sha(candidate_bytes) == PREFLIGHT_CANDIDATE_SHA
    candidate = json.loads(candidate_bytes)
    assert (candidate["head"], candidate["tree"], candidate["base"]) == (PREFLIGHT_HEAD, EXPECTED_TREE, INTEGRATED_R142)
    report_bytes = (PREFLIGHT / "compile-results.json").read_bytes()
    assert sha(report_bytes) == PREFLIGHT_REPORT_SHA
    report = json.loads(report_bytes)
    assert report["passed"] == len(FUNCTIONS) and report["failed"] == 0
    preflight_rows = {Path(row["source"]).stem: row for row in report["results"]}

    linked = {}
    proofs = {}
    providers = {}
    compiled_only = {}
    for stem, names in FUNCTIONS.items():
        preflight = preflight_rows[stem]
        source = preflight["source"]
        assert source.startswith(f"{APT}/") and preflight["passed"] and preflight["machine"] == 0x1F2
        [selected] = [row for row in invocation["objects"] if row["source"] == source]
        [sealed] = [row for row in seal["selected"] if row["source"] == source]
        assert selected == sealed, source
        path = Path(selected["object"])
        assert rsp.count(f'"{path}"') == 1, path
        obj = path.read_bytes()
        assert sha(obj) == selected["sha256"], path
        frozen_name = Path(preflight["object"]).name
        frozen_obj = (PREFLIGHT / frozen_name).read_bytes()
        assert path.name == frozen_name and sha(frozen_obj) == PREFLIGHT_OBJECTS[stem]
        selected_functions = coff_functions(obj, names)
        frozen_functions = coff_functions(frozen_obj, names)
        for method, (name, code, refs) in selected_functions.items():
            frozen_name, frozen_code, frozen_refs = frozen_functions[method]
            assert stable_name(name) == stable_name(frozen_name) and code == frozen_code, name
            assert canonical_bindings(refs) == canonical_bindings(frozen_refs), name
            if method == "SetInCtor" and name not in entries:
                assert not refs
                expect_words(code, 0, 0x8163000C, 0x508BD908, 0x9163000C, 0x4E800020)
                compiled_only[method] = {
                    "symbol": name, "sha256": sha(code), "status": "selected COFF matched; symbol absent from map",
                }
                continue
            address = map_address(entries, name, path.name)
            linked[method], proofs[method] = verify_function(entries, read, address, code, refs)
            proofs[method]["symbol"] = name
        providers[source] = {
            "selected_object": str(path), "selected_sha256": selected["sha256"], "frozen_sha256": PREFLIGHT_OBJECTS[stem],
        }

    slots = verify_dispatch(linked, proofs, entries)
    witnesses = verify_paths(linked, proofs, entries, read)
    class_witnesses = verify_class_paths(linked, proofs)
    receipt = {
        "schema": "r143-apt-linked-v1",
        "head": args.expected_head,
        "tree": EXPECTED_TREE,
        "preflight_head": PREFLIGHT_HEAD,
        "integrated_r142": INTEGRATED_R142,
        "preflight_candidate_sha256": PREFLIGHT_CANDIDATE_SHA,
        "preflight_compile_report_sha256": PREFLIGHT_REPORT_SHA,
        "seal_sha256": sha(seal_bytes),
        "response_sha256": sha(rsp_bytes),
        "xex_sha256": sha(xex),
        "pe_sha256": sha(pe),
        "map_sha256": sha(map_data),
        "providers": providers,
        "dispatch_initializer_slots": slots,
        "witnesses": witnesses,
        "class_witnesses": class_witnesses,
        "functions": proofs,
        "compiled_only": compiled_only,
        "scope": "Selected frozen COFF bodies and relocations matched the sealed linked PE. Static evidence.",
    }
    output = args.output or ROOT / "R143_APT_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
