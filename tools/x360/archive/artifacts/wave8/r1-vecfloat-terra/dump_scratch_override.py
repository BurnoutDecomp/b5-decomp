import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x822A21B0;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/822A21B0.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
