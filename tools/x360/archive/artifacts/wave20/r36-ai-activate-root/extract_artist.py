import pathlib
import struct

import ida_funcs
import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r36-ai-activate-root")
with (out / "artist.txt").open("w", encoding="utf-8") as stream:
    for address in (0x8277E578, 0x82767428):
        function = ida_funcs.get_func(address)
        assert function and function.start_ea == address
        stream.write(f"Function {address:08X} {idc.get_func_name(address)} to {function.end_ea:08X}\n")
        for current in range(address, function.end_ea, 4):
            refs = []
            for ref in idautils.DataRefsFrom(current):
                value = idc.get_strlit_contents(ref, -1, idc.STRTYPE_C)
                if value and all(32 <= byte < 127 for byte in value):
                    refs.append(f"{ref:08X} {value.decode()!r}")
            stream.write(f"{current:08X} {idc.get_bytes(current, 4).hex()} {idc.generate_disasm_line(current, 0)} {' '.join(refs)}\n")
    for offset in range(0, 0x30, 4):
        address = 0x820CE988 + offset
        raw = idc.get_bytes(address, 4)
        target, = struct.unpack(">I", raw)
        stream.write(f"Vtable {offset:02X} {target:08X} {idc.get_func_name(target)}\n")
ida_pro.qexit(0)
