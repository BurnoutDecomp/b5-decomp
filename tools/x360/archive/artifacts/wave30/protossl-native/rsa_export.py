import idaapi,ida_funcs,ida_name,idautils,idc,json
idaapi.init_hexrays_plugin();out=[]
for a in idautils.Functions(0x82b41b00,0x82b42350):
 f=ida_funcs.get_func(a)
 try: pseudo=str(idaapi.decompile(a))
 except Exception as e:pseudo=str(e)
 out.append({'addr':hex(a),'end':hex(f.end_ea),'name':ida_name.get_name(a),'pseudocode':pseudo,'assembly':[[hex(x),idc.generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea)]})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/rsa-functions.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
