import ida_idaapi,ida_xref,ida_funcs,ida_name,ida_lines,ida_pro,json
out={}
for name,addr in [('Compressor1',0x82f8c46c),('Delay',0x82f8ca24),('RawPuller2',0x82f8f768),('SinePlayer',0x82f9009c),('VuMeter',0x82f905e4)]:
 rows=[]
 for delta in range(0,0x34,4):
  x=ida_xref.get_first_dref_to(addr+delta)
  while x!=ida_idaapi.BADADDR:
   f=ida_funcs.get_func(x);rows.append({'target':hex(addr+delta),'at':hex(x),'function':ida_name.get_name(f.start_ea) if f else None,'line':ida_lines.tag_remove(ida_lines.generate_disasm_line(x,0) or '')});x=ida_xref.get_next_dref_to(addr+delta,x)
 out[name]=rows
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/stage-descriptor-xrefs.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)
