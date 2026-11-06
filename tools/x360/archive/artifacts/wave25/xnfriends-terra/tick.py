import idautils,idc,ida_funcs,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\tick-xrefs.txt','w') as f:
 for x in idautils.XrefsTo(0x8324EB2C):
  fn=ida_funcs.get_func(x.frm); f.write('%08x %s %s\n'%(x.frm,idc.get_func_name(fn.start_ea) if fn else 'data',idc.generate_disasm_line(x.frm,0)))
ida_pro.qexit(0)
