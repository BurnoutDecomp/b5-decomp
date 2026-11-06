"""Verify the frozen R142 Apt providers in a sealed Xbox 360 PE and map."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

import pefile


ROOT = Path(__file__).resolve().parent
EXPECTED_TREE = "404175c3db01a565ad591e4e33a7abd4bc8433fa"
COMPOSED_HEAD = "59ebb09deaf773fb0abe9518981e2cf4fecaffc0"
PREFLIGHT_HEAD = "84e4f189ec00f5b4e915bf7d3c3ec86cd8d23f74"
INTEGRATED_R141 = "612566c561a4af40c46e84ab4c764a033e210f6a"
FROZEN_RECEIPT_SHA = "5883ba38dc127022bd333ead22a9ff1924d506f17c267c24791cd999320c458d"
APT = "src/SDKs/EATech/include/Apt"
FUNCTIONS = {
    "AptActionDispatch": ("InitDispatchTable",),
    "AptActionInterpreter": ("isObjectOfType", "stackPopAndPush"),
    "AptActionInterpreterCompare": ("_FunctionAptActionEquals2", "IsDefinedStringType", "_FunctionAptActionStrictEquals"),
    "AptActionInterpreterMemberOps": ("_FunctionAptActionInstanceOf", "_FunctionAptActionCastOp"),
    "AptObject": ("SetImplementedObjects", "GetImplementedObjects", "DoesImplementObject"),
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
    index = 0
    while index < count:
        name, value, section, kind, _, aux = struct.unpack_from("<8sIhHBB", data, table + 18 * index)
        if name[:4] == bytes(4):
            start = strings + struct.unpack_from("<I", name, 4)[0]
            name = data[start:data.index(b"\0", start)]
        symbols[index] = name.rstrip(b"\0").decode("ascii", errors="replace"), value, section, kind
        index += 1 + aux

    functions = {}
    for method in names:
        [(name, value, section, kind)] = [
            row for row in symbols.values() if row[0].startswith(f"?{method}@") and row[2] > 0
        ]
        assert value == 0 and kind == 0x20, name
        header = 20 + 40 * (section - 1)
        assert data[header:header + 5] == b".text", name
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
            assert off not in bindings
            bindings[off] = kind, target, local
        functions[method] = name, data[offset:offset + size], bindings
    return functions


def canonical_bindings(bindings):
    return {
        offset: (kind, stable_name(target) if local is None else local)
        for offset, (kind, target, local) in bindings.items()
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
    for offset, (kind, symbol, local) in bindings.items():
        location = address + offset
        destination = address + local if local is not None else map_address(entries, symbol)
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
            assert local is not None and original == 0, (hex(location), symbol)
            value = destination
        else:
            raise AssertionError((hex(location), kind, symbol))
        assert word(linked, offset) == value, (hex(location), symbol, hex(word(linked, offset)), hex(value))
        struct.pack_into(">I", expected, offset, value)
        bound[hex(offset)] = {"symbol": symbol, "target": hex(destination), "type": hex(kind)}
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path)
    parser.add_argument("--expected-head", default=COMPOSED_HEAD)
    parser.add_argument("--output", type=Path, help="Defaults to R142_APT_LINKED.json in link_dir")
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

    git = ["git", "-C", seal["source_root"]]
    for head in (args.expected_head, COMPOSED_HEAD, PREFLIGHT_HEAD):
        tree = subprocess.check_output([*git, "rev-parse", f"{head}^{{tree}}"], text=True).strip()
        assert tree == EXPECTED_TREE, (head, tree)
    subprocess.run([*git, "merge-base", "--is-ancestor", INTEGRATED_R141, args.expected_head], check=True)

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
    receipt_bytes = (ROOT / "RECEIPT.json").read_bytes()
    assert sha(receipt_bytes) == FROZEN_RECEIPT_SHA
    frozen_receipt = json.loads(receipt_bytes)
    assert frozen_receipt["head"] == PREFLIGHT_HEAD
    abi_bytes = (ROOT / "COFF_ABI.json").read_bytes()
    assert sha(abi_bytes) == frozen_receipt["artifacts"]["COFF_ABI.json"]
    frozen_abi = json.loads(abi_bytes)
    assert frozen_abi["head"] == PREFLIGHT_HEAD

    linked = {}
    proofs = {}
    providers = {}
    for stem, names in FUNCTIONS.items():
        source = f"{APT}/{stem}.cpp"
        [selected] = [row for row in invocation["objects"] if row["source"] == source]
        [sealed] = [row for row in seal["selected"] if row["source"] == source]
        assert selected == sealed, source
        path = Path(selected["object"])
        assert rsp.count(f'"{path}"') == 1, path
        obj = path.read_bytes()
        assert sha(obj) == selected["sha256"], path
        frozen = frozen_abi["objects"][stem]
        frozen_obj = (ROOT / "native" / frozen["file"]).read_bytes()
        assert path.name == frozen["file"] and sha(frozen_obj) == frozen["sha256"] and len(frozen_obj) == frozen["bytes"]
        selected_functions = coff_functions(obj, names)
        frozen_functions = coff_functions(frozen_obj, names)
        for method, (name, code, refs) in selected_functions.items():
            frozen_name, frozen_code, frozen_refs = frozen_functions[method]
            assert stable_name(name) == stable_name(frozen_name) and code == frozen_code, name
            assert canonical_bindings(refs) == canonical_bindings(frozen_refs), name
            address = map_address(entries, name, path.name)
            linked[method], proofs[method] = verify_function(entries, read, address, code, refs)
            proofs[method]["symbol"] = name
        providers[source] = {
            "selected_object": str(path), "selected_sha256": selected["sha256"], "frozen_sha256": frozen["sha256"],
        }

    slots = verify_dispatch(linked, proofs, entries)
    witnesses = verify_paths(linked, proofs, entries, read)
    receipt = {
        "schema": "r142-apt-linked-v1",
        "head": args.expected_head,
        "tree": EXPECTED_TREE,
        "composed_head": COMPOSED_HEAD,
        "preflight_head": PREFLIGHT_HEAD,
        "integrated_r141": INTEGRATED_R141,
        "preflight_receipt_sha256": FROZEN_RECEIPT_SHA,
        "seal_sha256": sha(seal_bytes),
        "response_sha256": sha(rsp_bytes),
        "xex_sha256": sha(xex),
        "pe_sha256": sha(pe),
        "map_sha256": sha(map_data),
        "providers": providers,
        "dispatch_initializer_slots": slots,
        "witnesses": witnesses,
        "functions": proofs,
        "scope": "Selected frozen COFF bodies and relocations matched the sealed linked PE. Static evidence.",
    }
    output = args.output or link / "R142_APT_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
