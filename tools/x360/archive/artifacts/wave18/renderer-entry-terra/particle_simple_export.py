import idaapi,ida_funcs,idautils,idc,json
idaapi.init_hexrays_plugin()
a=0x8291de88
f=ida_funcs.get_func(a)
rows={'function':hex(f.start_ea) if f else None,'pseudocode':str(idaapi.decompile(a)) if f else None,'assembly':[[hex(x),idc.generate_disasm_line(x,0)] for x in idautils.Heads(f.start_ea,f.end_ea)] if f else []}
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/renderer-entry-terra/particle-simple-job.json','w').write(json.dumps(rows,indent=2)); idaapi.qexit(0)
