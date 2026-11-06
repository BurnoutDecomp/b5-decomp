import ida_bytes,ida_funcs,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra')
for a in (0x825D5500,0x825D57D8):
 f=ida_funcs.get_func(a);(out/('%08X.bin'%a)).write_bytes(ida_bytes.get_bytes(a,f.end_ea-a))
idc.qexit(0)
