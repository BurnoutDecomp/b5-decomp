import ida_auto,idautils,idc,ida_funcs,ida_lines,json
ida_auto.auto_wait(); out={}
for ea in [0x8324FF10,0x8324FF70]:
 x=[]
 for ref in idautils.XrefsTo(ea):
  f=ida_funcs.get_func(ref.frm); x.append({'addr':hex(ref.frm),'func':hex(f.start_ea) if f else None,'line':ida_lines.tag_remove(idc.generate_disasm_line(ref.frm,0) or '')})
 out[hex(ea)]=x
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave47/rw-allocator-terra/thread-global-xrefs.json','w').write(json.dumps(out,indent=2));idc.qexit(0)
