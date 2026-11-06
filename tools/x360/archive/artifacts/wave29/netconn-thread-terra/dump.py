import idautils,idc,ida_funcs,ida_hexrays,ida_lines,ida_pro
p=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave29\netconn-thread-terra\worker-and-imports.txt'
names=['CreateThread','RtlLeaveCriticalSection','RtlEnterCriticalSection','GetExitCodeThread','ExitThread']
with open(p,'w',encoding='utf-8') as f:
 for ea in idautils.Functions():
  n=idc.get_func_name(ea)
  if n not in names and ea != 0x82B12B70: continue
  fn=ida_funcs.get_func(ea); f.write('\n=== %08X %s end=%08X ===\n'%(ea,n,fn.end_ea))
  try:f.write(str(ida_hexrays.decompile(ea))+'\n')
  except Exception as e:f.write('DECOMP %s\n'%e)
  for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
  f.write('XREFS\n')
  for x in idautils.CodeRefsTo(ea,0):
   ff=ida_funcs.get_func(x); f.write('%08X in %s\n'%(x,idc.get_func_name(ff.start_ea) if ff else '?'))
ida_pro.qexit(0)
