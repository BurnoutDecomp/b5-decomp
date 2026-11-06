import idc
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917');rows=[]
for a,b in [(0x82c5c3c0,0x82c5c410),(0x82c5c5b8,0x82c5c5f0)]:
 while a<b:rows.append('%08X %s'%(a,idc.generate_disasm_line(a,0)));a+=4
(out/'vehicle-global-init-original.txt').write_text('\n'.join(rows));idc.qexit(0)
