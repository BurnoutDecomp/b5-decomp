import idaapi,idc,ida_bytes,json
out=[]
for ea in range(0x82c08ee8,0x82c08f08,4):
 out.append([hex(ea),(ida_bytes.get_bytes(ea,4) or b'').hex(),idc.generate_disasm_line(ea,0)])
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/entropy-probe/savegpr.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
