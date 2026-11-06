import idc
from pathlib import Path
out=[]
for a in range(0x82c4bc80,0x82c4bd40,4):out.append('%08X %s'%(a,idc.generate_disasm_line(a,0)))
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/render-cull-init.txt').write_text('\n'.join(out));idc.qexit(0)
