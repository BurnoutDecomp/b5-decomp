import ida_funcs,idautils,idc,ida_hexrays,ida_lines,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\enumeration.txt','w') as f:
 ea=0x82B122F0; f.write(str(ida_hexrays.decompile(ea))+'\n')
 for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
