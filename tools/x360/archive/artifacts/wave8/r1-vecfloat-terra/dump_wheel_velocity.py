import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x825D7008;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/825D7008.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
