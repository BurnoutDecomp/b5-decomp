import ida_bytes,idc
from pathlib import Path
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/8227E7A8.bin').write_bytes(ida_bytes.get_bytes(0x8227E7A8,0x200))
idc.qexit(0)
