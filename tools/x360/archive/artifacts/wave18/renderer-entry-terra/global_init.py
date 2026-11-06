import idaapi,idc,ida_bytes,json
rows=[]
for ea in range(0x82c6c6c0,0x82c6c748,4):
 rows.append([hex(ea),(ida_bytes.get_bytes(ea,4) or b'').hex(),idc.generate_disasm_line(ea,0)])
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/global-init.json','w').write(json.dumps(rows,indent=2))
idaapi.qexit(0)
