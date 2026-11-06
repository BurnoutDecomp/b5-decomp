import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x8291D810;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/8291D810.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
