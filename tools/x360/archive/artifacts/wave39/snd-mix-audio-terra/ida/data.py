import idaapi,ida_bytes,ida_funcs,ida_name,idautils,idc,json
out={}
for a in [0x821565D4,0x8327760c,0x83277728,0x83277800]:
 out[hex(a)]={'name':ida_name.get_name(a),'bytes':ida_bytes.get_bytes(a,24).hex(),'xrefs':[[hex(x.frm),ida_name.get_name(ida_funcs.get_func(x.frm).start_ea) if ida_funcs.get_func(x.frm) else ''] for x in idautils.XrefsTo(a)]}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave39/snd-mix-audio-terra/ida-data.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
