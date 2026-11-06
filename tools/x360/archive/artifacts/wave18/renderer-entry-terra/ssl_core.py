import idaapi,ida_funcs,idautils,ida_name,idc,json
idaapi.init_hexrays_plugin(); out=[]
for a in [0x82b36fa0,0x82b356a0,0x82b35758,0x82b36f20,0x82b38108]:
 f=ida_funcs.get_func(a);out.append({'ea':hex(a),'name':ida_name.get_name(a),'pseudocode':str(idaapi.decompile(a)),'assembly':[[hex(x),idc.generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea)]})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/ssl-core.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
