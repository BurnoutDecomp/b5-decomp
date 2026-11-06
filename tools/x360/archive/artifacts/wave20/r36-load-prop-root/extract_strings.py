import pathlib

import ida_pro
import idautils
import idc

out = pathlib.Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r36-load-prop-root")
with (out / "artist-strings.txt").open("w", encoding="utf-8") as stream:
    for address in range(0x822F2EF0, 0x822F3DBC, 4):
        for reference in idautils.DataRefsFrom(address):
            value = idc.get_strlit_contents(reference, -1, idc.STRTYPE_C)
            if value and len(value) >= 4 and all(32 <= byte < 127 or byte in (9, 10, 13) for byte in value):
                stream.write(f"{address:08X} {reference:08X} {value.decode()!r}\n")
ida_pro.qexit(0)
