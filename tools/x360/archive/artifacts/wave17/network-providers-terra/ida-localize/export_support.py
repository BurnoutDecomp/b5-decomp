import ida_hexrays, ida_funcs, ida_lines, idautils, idc, json
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-localize\support.json'
ida_hexrays.init_hexrays_plugin(); result={}
for ea in [0x82C10930,0x82B302C8]:
 f=ida_funcs.get_func(ea); cf=ida_hexrays.decompile(ea); result[hex(ea)]={'name':idc.get_func_name(ea),'start':hex(f.start_ea),'end':hex(f.end_ea),'pseudocode':str(cf),'assembly':[{'address':hex(x),'text':ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')} for x in idautils.FuncItems(f.start_ea)]}
with open(out,'w',encoding='utf-8') as o:json.dump(result,o,indent=2)
idc.qexit(0)
