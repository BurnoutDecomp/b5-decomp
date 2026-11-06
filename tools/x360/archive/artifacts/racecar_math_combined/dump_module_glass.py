import ida_bytes,ida_funcs,idc
from pathlib import Path
a=0x822E87B8;f=ida_funcs.get_func(a);Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_combined/822E87B8.bin').write_bytes(ida_bytes.get_bytes(a,f.end_ea-a));idc.qexit(0)
