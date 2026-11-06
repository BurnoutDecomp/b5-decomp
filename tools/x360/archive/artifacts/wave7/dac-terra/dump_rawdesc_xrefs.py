import ida_idaapi,ida_xref,ida_funcs,ida_name,ida_lines,ida_pro,json
addr=0x82F8F768; out=[];x=ida_xref.get_first_dref_to(addr)
while x!=ida_idaapi.BADADDR:
 f=ida_funcs.get_func(x);out.append({'at':hex(x),'function':ida_name.get_name(f.start_ea) if f else None,'line':ida_lines.tag_remove(ida_lines.generate_disasm_line(x,0) or '')});x=ida_xref.get_next_dref_to(addr,x)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/rawpuller-descriptor-xrefs.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)


