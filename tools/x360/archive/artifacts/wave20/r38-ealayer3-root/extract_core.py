import pathlib
import struct

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r38-ealayer3-root")
with (out / "core.txt").open("w", encoding="utf-8") as stream:
    for base in (0x8215A944, 0x8215A96C):
        stream.write(f"Vtable {base:08X}\n")
        for address in range(base, base + 0x20, 4):
            target, = struct.unpack(">I", idc.get_bytes(address, 4))
            stream.write(f"{address:08X} {target:08X} {idc.get_func_name(target)}\n")
        for ref in idautils.DataRefsTo(base):
            stream.write(f"REF {ref:08X} {idc.get_func_name(ref)}\n")
    for start in (0x82B94F50, 0x82B93A98, 0x82B953A0):
        function = ida_funcs.get_func(start)
        assert function and function.start_ea == start
        stream.write(f"Function {start:08X} {idc.get_func_name(start)} to {function.end_ea:08X}\n")
        for address in range(start, function.end_ea, 4):
            stream.write(f"{address:08X} {idc.get_bytes(address, 4).hex()} {idc.generate_disasm_line(address, 0)}\n")
ida_pro.qexit(0)
