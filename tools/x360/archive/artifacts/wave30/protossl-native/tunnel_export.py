import idaapi,ida_funcs,ida_name,idautils,json
idaapi.init_hexrays_plugin();out=[]
for a in [0x82b2d878,0x82b2ddd0,0x82b2f078,0x82b2f368]:
 f=ida_funcs.get_func(a)
 try:p=str(idaapi.decompile(a))
 except Exception as e:p=str(e)
 out.append({'addr':hex(a),'name':ida_name.get_name(a),'pseudocode':p,'assembly':[[hex(x),__import__('idc').generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea)] if f else []})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/prototunnel-functions.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
