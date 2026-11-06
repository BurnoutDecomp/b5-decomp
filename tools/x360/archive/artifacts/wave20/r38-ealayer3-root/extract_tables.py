import json
import pathlib
import struct

import ida_bytes
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r38-ealayer3-root")
tables = [
    (0x82F89400, 512), (0x82F89600, 512), (0x82F89800, 512), (0x82F89A00, 512),
    (0x82F89C00, 512), (0x82F89E00, 550), (0x82F8A080, 554), (0x82F8A300, 524),
    (0x82F8A580, 650), (0x82F8A880, 598), (0x82F8AB00, 558), (0x82F8AD80, 1392),
    (0x82F8B300, 1238), (0x82F8B800, 1386), (0x82F8BD80, 1132),
]
def read(address, size):
    data = ida_bytes.get_bytes(address, size)
    assert data is not None and len(data) == size, (address, size)
    return data.hex()

addresses = (0x82159300, 0x82159780, 0x8215A8E8, 0x8215A944, 0x8215A96C)
result = {
    "huffman": [{"address": address, "size": size, "bytes": read(address, size)} for address, size in tables],
    "gain": {"address": 0x82159300, "size": 0x480, "bytes": read(0x82159300, 0x480)},
    "crossrefs": {hex(address): [{"address": hex(ref), "function": idc.get_func_name(ref)} for ref in idautils.DataRefsTo(address)] for address in addresses},
    "vtables": {hex(address): {"target": hex(struct.unpack(">I", bytes.fromhex(read(address, 4)))[0]), "raw": read(address, 4)} for address in addresses[2:]},
}
with (out / "tables-native.json").open("x", encoding="utf-8") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
ida_pro.qexit(0)
