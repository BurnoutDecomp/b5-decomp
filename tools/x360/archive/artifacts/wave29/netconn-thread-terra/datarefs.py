import idautils,idc,ida_funcs,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave29\netconn-thread-terra\worker-datarefs.txt','w') as f:
 for x in idautils.XrefsTo(0x82B12B70):
  fn=ida_funcs.get_func(x.frm); f.write('%08X type=%s in %s %s\n'%(x.frm,x.type,idc.get_func_name(fn.start_ea) if fn else '?',idc.generate_disasm_line(x.frm,0) or ''))
ida_pro.qexit(0)
