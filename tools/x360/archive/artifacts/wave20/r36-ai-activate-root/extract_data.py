import pathlib
import struct

import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r36-ai-activate-root")
addresses = (0x8300D6F0, 0x8300D714, 0x82F30170, 0x82F30174, 0x82F30180, 0x82F30184,
             0x8300D604, 0x8300D605, 0x8300D606, 0x8300D608, 0x820047C8)
with (out / "data.txt").open("w", encoding="utf-8") as stream:
    for address in addresses:
        raw = idc.get_bytes(address, 4)
        stream.write(f"Data {address:08X} {raw.hex()} {struct.unpack('>f', raw)[0]}\n")
        for reference in idautils.DataRefsTo(address):
            line = idc.generate_disasm_line(reference, 0)
            stream.write(f"{reference:08X} {idc.get_func_name(reference)} {line}\n")
ida_pro.qexit(0)
