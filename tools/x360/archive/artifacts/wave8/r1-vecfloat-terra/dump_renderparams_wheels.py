import ida_bytes,ida_funcs,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra')
for a in (0x822A3220,0x822A31B8,0x822B8738):
 f=ida_funcs.get_func(a);(out/('%08X.bin'%a)).write_bytes(ida_bytes.get_bytes(a,f.end_ea-a))
idc.qexit(0)
