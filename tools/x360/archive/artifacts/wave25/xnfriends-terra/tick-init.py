import ida_funcs,idautils,idc,ida_hexrays,ida_lines,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\tick-init.txt','w') as f:
 fn=ida_funcs.get_func(0x82b29d24); f.write('%08x %s\n'%(fn.start_ea,idc.get_func_name(fn.start_ea))); f.write(str(ida_hexrays.decompile(fn.start_ea))+'\n')
 for x in idautils.FuncItems(fn.start_ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
