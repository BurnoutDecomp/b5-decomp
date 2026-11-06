import ida_bytes,ida_funcs,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers')
for a in [0x8227E568,0x8227C2E8]:
 f=ida_funcs.get_func(a);(out/('%08X.bin'%a)).write_bytes(ida_bytes.get_bytes(a,f.end_ea-a))
idc.qexit(0)
