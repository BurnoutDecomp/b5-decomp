import ida_funcs,idc
from pathlib import Path
lines=[]
for a in (0x82c4bd40,0x82c4bd30,0x82c4be38):
 f=ida_funcs.get_func(a);lines.append('%08X %s %s'%(a,hex(f.start_ea) if f else 'none',hex(f.end_ea) if f else 'none'))
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/glass-init-bounds.txt').write_text('\n'.join(lines));idc.qexit(0)
