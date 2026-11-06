import idaapi,ida_funcs,ida_bytes,ida_name,idautils,idc,json
addresses=[0x82c34ec8,0x82c343d0,0x82c37a58,0x82c377d0,0x82c37bf8,0x82c37968,0x82c379e0]
for s in ['XHVCreateEngine','XHVEngineCreate']:
 ea=ida_name.get_name_ea(idaapi.BADADDR,s)
 if ea!=idaapi.BADADDR:addresses.append(ea)
out=[]
for a in addresses:
 f=ida_funcs.get_func(a)
 if f:
  out.append({'addr':hex(a),'name':ida_name.get_name(a),'size':f.end_ea-f.start_ea,'bytes':ida_bytes.get_bytes(f.start_ea,f.end_ea-f.start_ea).hex(),'calls':[[hex(x),idc.generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea) if idc.print_insn_mnem(x) in ['bl','bctrl','bctr']]})
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave40/xhv-sdk-terra/artist-targets.json','w').write(json.dumps(out,indent=2));idaapi.qexit(0)
