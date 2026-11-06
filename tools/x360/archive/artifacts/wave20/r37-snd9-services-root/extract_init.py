import pathlib
import struct

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r37-snd9-services-root")
with (out / "init.txt").open("w", encoding="utf-8") as stream:
    spans = [(0x82C73B68, 0x82C73BB0)]
    for address in (0x82B85E88, 0x82B7B360, 0x82B859B8):
        function = ida_funcs.get_func(address)
        assert function and function.start_ea == address
        spans.append((address, function.end_ea))
    for start, end in spans:
        stream.write(f"Function {start:08X} {idc.get_func_name(start)} to {end:08X}\n")
        for current in range(start, end, 4):
            stream.write(f"{current:08X} {idc.get_bytes(current, 4).hex()} {idc.generate_disasm_line(current, 0)}\n")
    for address in (0x83277C9C, 0x83277830, 0x83277834, 0x82F883D0, 0x82F883DC, 0x8327772C):
        raw = idc.get_bytes(address, 4)
        stream.write(f"Data {address:08X} {raw.hex()} {struct.unpack('>I', raw)[0]}\n")
        for x in idautils.DataRefsTo(address):
            stream.write(f"{x:08X} {idc.get_func_name(x)} {idc.generate_disasm_line(x, 0)}\n")
ida_pro.qexit(0)
