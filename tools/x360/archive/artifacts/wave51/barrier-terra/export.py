import ida_auto,ida_hexrays,ida_funcs,idc,ida_lines,json
ida_auto.auto_wait(); ea=0x82BCA7B8; f=ida_funcs.get_func(ea)
r={'pseudocode':str(ida_hexrays.decompile(ea)),'asm':[]}
for addr in range(f.start_ea,f.end_ea,4): r['asm'].append('%08X %s'%(addr,ida_lines.tag_remove(idc.generate_disasm_line(addr,0) or '')))
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave51/barrier-terra/artist-addbarrier.json','w').write(json.dumps(r,indent=2));idc.qexit(0)
