import ida_funcs,idautils,idc,ida_hexrays,ida_lines,ida_xref,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\network-evidence.txt','w',encoding='utf-8') as f:
 for target in [0x8297DC98,0x82927920]:
  for x in idautils.CodeRefsTo(target,0):
   fn=ida_funcs.get_func(x); f.write('xref %08x -> %08x in %s %08x\n'%(x,target,idc.get_func_name(fn.start_ea) if fn else '?',fn.start_ea if fn else 0))
 for ea in [0x82B13248,0x82B29A50]:
  f.write('\n=== %08X %s ===\n'%(ea,idc.get_func_name(ea)))
  try: f.write(str(ida_hexrays.decompile(ea))+'\n')
  except Exception as e: f.write(str(e))
  for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
