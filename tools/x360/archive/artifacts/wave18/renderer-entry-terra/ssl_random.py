import idaapi,ida_bytes,ida_funcs,ida_name,idautils,json
base=0x8324ee34
r={'raw':(ida_bytes.get_bytes(base,16) or b'').hex(),'refs':{hex(e):[{'at':hex(x.frm),'function':ida_name.get_name(ida_funcs.get_func(x.frm).start_ea) if ida_funcs.get_func(x.frm) else None} for x in idautils.XrefsTo(e,0)] for e in range(base,base+16,4)}}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/random-state.json','w').write(json.dumps(r,indent=2));idaapi.qexit(0)
