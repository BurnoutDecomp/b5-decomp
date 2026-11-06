import pathlib
import struct

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r35-snd-panning-root")
with (out / "owners.txt").open("w", encoding="utf-8") as stream:
    for member in (0x82B85E78, 0x82B7D020):
        function = ida_funcs.get_func(member)
        assert function
        address = function.start_ea
        stream.write(f"Function {address:08X} {idc.get_func_name(address)} to {function.end_ea:08X}\n")
        stream.write("Callers " + " ".join(f"{source:08X}" for source in idautils.CodeRefsTo(address, False)) + "\n")
        for current in range(address, function.end_ea, 4):
            stream.write(f"{current:08X} {idc.get_bytes(current, 4).hex()} {idc.generate_disasm_line(current, 0)}\n")
    for address in (0x820037C8, 0x82004928, 0x82001DA0, 0x82001CC0, 0x82010C1C, 0x82010C20, 0x82047B38):
        raw = idc.get_bytes(address, 4)
        stream.write(f"Float {address:08X} {raw.hex()} {struct.unpack('>f', raw)[0]}\n")
ida_pro.qexit(0)
