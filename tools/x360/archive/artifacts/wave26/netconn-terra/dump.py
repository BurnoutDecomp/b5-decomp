import ida_funcs,idautils,idc,ida_hexrays,ida_lines,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave26\netconn-terra\netconn-dump.txt','w',encoding='utf-8') as f:
 for a in idautils.Functions(0x82B11CE0,0x82B13B68):
  fn=ida_funcs.get_func(a); f.write('\n=== %08X %s end=%08X ===\n'%(a,idc.get_func_name(a),fn.end_ea))
  try: f.write(str(ida_hexrays.decompile(a))+'\n')
  except Exception as e: f.write('ERROR %s\n'%e)
  for x in idautils.FuncItems(a): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
