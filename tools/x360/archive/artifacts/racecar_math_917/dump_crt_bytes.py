import ida_bytes,idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917')
(out/'artist-crt.bin').write_bytes(ida_bytes.get_bytes(0x82C5C4B0,0x40))
(out/'artist-world-up-rdata.bin').write_bytes(ida_bytes.get_bytes(0x82181510,16))
(out/'artist-world-up-bss.bin').write_bytes(ida_bytes.get_bytes(0x82FB9050,16))
for a in (0x82001CC0,0x82001C98):
 (out/('%08X.bin'%a)).write_bytes(ida_bytes.get_bytes(a,4))
idc.qexit(0)
