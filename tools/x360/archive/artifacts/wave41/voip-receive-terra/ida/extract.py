import idaapi,ida_funcs,ida_name,idautils,idc,json
idaapi.init_hexrays_plugin();out=[]
for a in [0x82b3a190,0x82b39e10,0x82b3a360]:
 f=ida_funcs.get_func(a)
 try:p=str(idaapi.decompile(f.start_ea))
 except Exception as e:p=str(e)
 out.append({'query':hex(a),'start':hex(f.start_ea),'end':hex(f.end_ea),'name':ida_name.get_name(f.start_ea),'pseudocode':p,'assembly':[[hex(x),idc.generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea)]})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave41/voip-receive-terra/artist-receive.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
