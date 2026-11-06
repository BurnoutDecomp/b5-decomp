import idc
from pathlib import Path
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/wheelblur-init-disasm.txt').write_text('\n'.join('%08X %s'%(a,idc.generate_disasm_line(a,0)) for a in range(0x82c4b158,0x82c4b188,4)));idc.qexit(0)
