import pathlib

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r37-snd9-services-root")
addresses = (0x83277CB0, 0x83277838, 0x83277864, 0x83277866, 0x83277888,
             0x83277830, 0x83277834, 0x83277C9C, 0x83277910, 0x83277C14)
with (out / "refs.txt").open("w", encoding="utf-8") as stream:
    for address in addresses:
        stream.write(f"Data {address:08X}\n")
        for reference in idautils.DataRefsTo(address):
            function = ida_funcs.get_func(reference)
            stream.write(f"{reference:08X} {function.start_ea if function else 0:08X} "
                         f"{idc.get_func_name(reference)} {idc.generate_disasm_line(reference, 0)}\n")
ida_pro.qexit(0)
