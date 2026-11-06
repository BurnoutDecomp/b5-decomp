import ida_bytes,ida_funcs,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917')
for a in [0x826000F8,0x825FFBD8,0x8261FF50,0x8262EBE8,0x82600558]:
 f=ida_funcs.get_func(a);(out/('%08X.bin'%a)).write_bytes(ida_bytes.get_bytes(a,f.end_ea-a))
idc.qexit(0)
