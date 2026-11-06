import idautils,idc,ida_funcs,ida_hexrays,ida_lines,ida_pro
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave28\dirty-diagnostics-terra\functions.txt'
with open(out,'w',encoding='utf-8') as f:
 for ea in idautils.Functions():
  n=idc.get_func_name(ea)
  if not ('DirtyErr' in n or 'NetPrintMem' in n): continue
  fn=ida_funcs.get_func(ea); f.write('\n=== %08X %s end=%08X ===\n'%(ea,n,fn.end_ea))
  try: f.write(str(ida_hexrays.decompile(ea))+'\n')
  except Exception as e: f.write('ERROR %s\n'%e)
  for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
