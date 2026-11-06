import ida_hexrays,ida_funcs,ida_lines,idautils,idc,json,os
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-batch4';os.makedirs(out,exist_ok=True);ida_hexrays.init_hexrays_plugin()
for ea in [0x82B2AD78]:
 f=ida_funcs.get_func(ea);row={'address':'0x%08X'%ea,'name':idc.get_func_name(ea),'start':'0x%08X'%f.start_ea,'end':'0x%08X'%f.end_ea,'pseudocode':str(ida_hexrays.decompile(ea)),'assembly':'\n'.join('0x%08X  %s'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')) for x in idautils.FuncItems(f.start_ea))}
 with open(os.path.join(out,'0x%08X.json'%ea),'w',encoding='utf-8') as o:json.dump(row,o,indent=2)
idc.qexit(0)
