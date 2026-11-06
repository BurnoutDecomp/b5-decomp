import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x826415E8;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/826415E8.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
