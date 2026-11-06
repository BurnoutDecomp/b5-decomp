import ida_bytes,idc
from pathlib import Path
out=[]
for a in (0x82c4a0b0,0x82c4a0d8,0x82c4a110):
 for i in range(a,a+0x28,4):out.append('%08X %s'%(i,idc.generate_disasm_line(i,0)))
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/racecar_consumers/effects-crt-disasm.txt').write_text('\n'.join(out));idc.qexit(0)

