import idaapi,ida_funcs,idautils,ida_name,idc,json
r=[]
for ea in idautils.Functions(0x82b35000,0x82b38200):
 f=ida_funcs.get_func(ea);r.append({'ea':hex(ea),'end':hex(f.end_ea),'name':ida_name.get_name(ea)})
for ea in [0x82b36f20,0x82b37150]:
 r.append({'refs_to_'+hex(ea):[hex(x.frm) for x in idautils.XrefsTo(ea,0)]})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/ssl-functions.json','w').write(json.dumps(r,indent=2));idaapi.qexit(0)
