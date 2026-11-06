"""Bind the frozen Equals2 COFF evidence to a composed Xbox 360 link."""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile


ROOT = Path(__file__).resolve().parent
SOURCE = "src/SDKs/EATech/include/Apt/AptActionInterpreterCompare.cpp"
TARGETED_RECEIPT_SHA256 = "9444b5485d589cf7eb6aba098fed7b7ef0f137d7a78c5dc9c5084c4aaae00fc5"
FUNCTIONS = ("?_FunctionAptActionEquals2@", "?IsDefinedStringType@")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stable_name(name):
    return re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name)


def coff_functions(data):
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
    for prefix in FUNCTIONS:
        [(name, value, section, kind)] = [s for s in symbols.values() if s[0].startswith(prefix) and s[2] > 0]
        assert value == 0 and kind == 0x20
        header = 20 + 40 * (section - 1)
        assert data[header:header + 5] == b".text"
        size, offset, relocs = struct.unpack_from("<III", data, header + 16)
        count = struct.unpack_from("<H", data, header + 32)[0]
        code = data[offset:offset + size]
        bindings = []
        for index in range(count):
            off, symbol, kind = struct.unpack_from("<IIH", data, relocs + 10 * index)
            target = symbols[symbol][0] if kind != 0x12 else None
            bindings.append((off, kind, target))
        functions[prefix] = name, code, bindings
    return functions


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


def verify_function(entries, read, address, code, bindings):
    linked = read(address, len(code))
    expected = bytearray(code)
    bound = {}
    high = {}
    for offset, kind, symbol in bindings:
        if kind == 0x12:  # PPC PAIR for a preceding HA or LO field
            continue
        location = address + offset
        original, = struct.unpack_from(">I", code, offset)
        actual, = struct.unpack_from(">I", linked, offset)
        destination = map_address(entries, symbol)
        if kind == 0x06:  # PPC relative branch with a 24-bit word displacement
            assert actual & 0xFC000003 == original & 0xFC000003
            assert location + signed(actual & 0x03FFFFFC, 26) == destination, (hex(location), symbol)
        elif kind == 0x10:  # high-adjusted immediate
            assert actual >> 16 == original >> 16
            assert actual & 0xFFFF == ((destination + 0x8000) >> 16) & 0xFFFF
            high[symbol] = actual & 0xFFFF
        elif kind == 0x11:  # signed low immediate of the same address
            assert actual >> 16 == original >> 16
            assert ((high[symbol] << 16) + signed(actual & 0xFFFF, 16)) & 0xFFFFFFFF == destination
        else:
            raise AssertionError((hex(location), kind, symbol))
        struct.pack_into(">I", expected, offset, actual)
        bound[hex(offset)] = {"symbol": symbol, "address": hex(destination), "coff_type": hex(kind)}
    assert linked == expected, f"Fixed PPC bytes differ at {address:#x}"
    return {"address": hex(address), "bytes": len(code), "sha256": sha(linked), "bindings": bound}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("link_dir", type=Path)
    parser.add_argument("--expected-head")
    parser.add_argument("--output", type=Path, help="Defaults to R141_APT_EQUALS2_LINKED.json in link_dir")
    args = parser.parse_args()
    link = args.link_dir.resolve()
    result = json.loads((link / "result.json").read_text())
    image = json.loads((link / "image-verification.json").read_text())
    seal_bytes = (link / "seal.json").read_bytes()
    seal = json.loads(seal_bytes)
    invocation = json.loads((link / "invocation.json").read_text())
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert sha(seal_bytes) == invocation["sealed_sha256"]
    assert image["head"] == seal["head"]
    if args.expected_head:
        assert image["head"] == args.expected_head

    def verified_output(name):
        [record] = [value for path, value in result["outputs"].items() if Path(path).name == name]
        data = (link / name).read_bytes()
        assert len(data) == record["size"] and sha(data) == record["sha256"], name
        return data

    xex = verified_output("native-game.exe")
    assert xex[:4] == b"XEX2" and sha(xex) == image["image_sha256"]
    pe = verified_output("native-game.pe")
    map_data = verified_output("native-game.map")
    entries = read_map(map_data)
    [selected] = [row for row in invocation["objects"] if row["source"] == SOURCE]
    [sealed_object] = [row for row in seal["selected"] if row["source"] == SOURCE]
    assert sealed_object == selected
    selected_path = Path(selected["object"])
    [rsp_name] = [arg[1:] for arg in invocation["command"] if arg.startswith("@")]
    rsp_bytes = (link / Path(rsp_name).name).read_bytes()
    assert sha(rsp_bytes) == invocation["rsp_sha256"]
    assert rsp_bytes.decode().splitlines().count(f'"{selected_path}"') == 1
    selected_obj = selected_path.read_bytes()
    assert sha(selected_obj) == selected["sha256"]
    targeted = ROOT / "native" / selected_path.name
    targeted_obj = targeted.read_bytes()
    receipt_bytes = (ROOT / "RECEIPT.json").read_bytes()
    assert sha(receipt_bytes) == TARGETED_RECEIPT_SHA256
    assert sha(targeted_obj) == json.loads(receipt_bytes)["object_sha256"]

    selected_functions = coff_functions(selected_obj)
    targeted_functions = coff_functions(targeted_obj)
    for prefix in FUNCTIONS:
        name, code, refs = selected_functions[prefix]
        old_name, old_code, old_refs = targeted_functions[prefix]
        assert stable_name(name) == stable_name(old_name) and code == old_code, name
        normalize = lambda rows: [(off, kind, stable_name(symbol) if symbol else None) for off, kind, symbol in rows]
        assert normalize(refs) == normalize(old_refs), name

    pe_image = pefile.PE(data=pe)

    def read(address, size):
        rva = address - pe_image.OPTIONAL_HEADER.ImageBase
        section = next(s for s in pe_image.sections if s.VirtualAddress <= rva and
                       rva + size <= s.VirtualAddress + s.SizeOfRawData)
        offset = section.PointerToRawData + rva - section.VirtualAddress
        return pe[offset:offset + size]

    proofs = {}
    for name, code, refs in selected_functions.values():
        address = map_address(entries, name, selected_path.name)
        proofs[name] = verify_function(entries, read, address, code, refs)
    constants = {}
    for symbol, payload in [("__real@3a83126f", bytes.fromhex("3a83126f")),
                            ("??_C@_01LFCBOECM@?4?$AA@", b".\0")]:
        address = map_address(entries, symbol)
        assert read(address, len(payload)) == payload
        constants[symbol] = {"address": hex(address), "bytes": payload.hex()}

    receipt = {
        "head": image["head"], "source": SOURCE,
        "targeted_receipt_sha256": TARGETED_RECEIPT_SHA256,
        "seal_sha256": sha(seal_bytes), "response_sha256": sha(rsp_bytes),
        "selected_object": str(selected_path), "selected_object_sha256": sha(selected_obj),
        "xex_sha256": sha(xex), "pe_sha256": sha(pe), "map_sha256": sha(map_data),
        "functions": proofs, "constants": constants, "runtime": "not run",
    }
    output = args.output or link / "R141_APT_EQUALS2_LINKED.json"
    with output.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(receipt, stream, indent=2)
        stream.write("\n")
    print(output, sha(output.read_bytes()))


if __name__ == "__main__":
    main()
