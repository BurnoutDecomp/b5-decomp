import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x822E79F8;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/822E79F8.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
