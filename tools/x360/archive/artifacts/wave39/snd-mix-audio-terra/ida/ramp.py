import idaapi,ida_bytes,ida_funcs,ida_name,idautils,idc,json
out={}
for a in [0x8214B190,0x8214B1A4,0x820581AC,0x8214B1A8,0x8214B1B0,0x8214B1B4,0x8214B1B8,0x8214B1BC,0x8214B1C0,0x8214B1C4,0x8214B1C8]:
 out[hex(a)]={'name':ida_name.get_name(a),'bytes':ida_bytes.get_bytes(a,24).hex(),'xrefs':[[hex(x.frm),ida_name.get_name(ida_funcs.get_func(x.frm).start_ea) if ida_funcs.get_func(x.frm) else ''] for x in idautils.XrefsTo(a)]}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave39/snd-mix-audio-terra/ramp-data.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)

