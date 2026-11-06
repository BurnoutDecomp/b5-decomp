import ida_funcs, idautils, idc, ida_hexrays, ida_lines, ida_pro
p=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\functions.txt'
with open(p,'w',encoding='utf-8') as f:
 for ea in idautils.Functions():
  name=idc.get_func_name(ea)
  if 'XnFriends' not in name: continue
  fn=ida_funcs.get_func(ea); f.write('\n=== %08X %s end=%08X ===\n'%(ea,name,fn.end_ea))
  try: f.write(str(ida_hexrays.decompile(ea))+'\n')
  except Exception as e: f.write('DECOMP ERROR %s\n'%e)
  for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
