import idaapi, ida_funcs, ida_name, ida_bytes, idautils, idc, json
rows=[]
for ea in (0x83010F96,0x83010F97,0x830110E0):
    refs=[]
    for x in idautils.XrefsTo(ea, 0):
        f=ida_funcs.get_func(x.frm)
        refs.append({'from':hex(x.frm),'kind':x.type,'function':ida_name.get_name(f.start_ea) if f else None,'line':idc.generate_disasm_line(x.frm,0)})
    rows.append({'address':hex(ea),'name':ida_name.get_name(ea),'data':(ida_bytes.get_bytes(ea,16) or b'').hex(),'refs':refs})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/global-xrefs.json','w').write(json.dumps(rows,indent=2))
idaapi.qexit(0)
