import idautils,idc,ida_funcs
from pathlib import Path
rows=[]
for a in [0x82fb9da0,0x82fb9ec0,0x82fb8b10]:
 for x in idautils.XrefsTo(a):
  f=ida_funcs.get_func(x.frm);rows.append('%08X %08X %s %s'%(a,x.frm,hex(f.start_ea) if f else '-',idc.generate_disasm_line(x.frm,0)))
Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_math_917/globals-xrefs.txt').write_text('\n'.join(rows));idc.qexit(0)
