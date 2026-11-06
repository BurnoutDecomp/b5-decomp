import ida_bytes,ida_name,ida_segment,idautils,idc
from pathlib import Path
a=0x82cdb6b9;lines=['name='+str(ida_name.get_name(a)),'segment='+str(ida_segment.get_segm_name(ida_segment.getseg(a))),'data='+str(ida_bytes.get_bytes(a-8,17).hex())]
for x in idautils.XrefsTo(a):lines.append('%08X %s'%(x.frm,idc.generate_disasm_line(x.frm,0)))
Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/render-visible-config.txt').write_text('\n'.join(lines));idc.qexit(0)
