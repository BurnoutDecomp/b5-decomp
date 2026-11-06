import pathlib

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r35-snd-panning-root")
with (out / "artist.txt").open("w", encoding="utf-8") as stream:
    for address in (0x82B73000, 0x82B7CBD8):
        function = ida_funcs.get_func(address)
        assert function and function.start_ea == address
        stream.write(f"Function {address:08X} {idc.get_func_name(address)} to {function.end_ea:08X}\n")
        stream.write("Callers " + " ".join(f"{source:08X}" for source in idautils.CodeRefsTo(address, False)) + "\n")
        for current in range(function.start_ea, function.end_ea, 4):
            stream.write(f"{current:08X} {idc.get_bytes(current, 4).hex()} {idc.generate_disasm_line(current, 0)}\n")
    for address in (0x83277660, 0x83277838, 0x83277847, 0x83277C94, 0x83277700):
        stream.write(f"Data {address:08X} " + " ".join(f"{source:08X}" for source in idautils.DataRefsTo(address)) + "\n")
ida_pro.qexit(0)
