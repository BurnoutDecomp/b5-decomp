import ida_funcs,idautils,idc,ida_hexrays,ida_lines,ida_pro
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave25\xnfriends-terra\helpers.txt','w',encoding='utf-8') as f:
 for ea in idautils.Functions():
  name=idc.get_func_name(ea)
  if not (name in ('sub_82B38C60','_findFriendByName','_compareFriendsAndCB') or 0x82B38B00<=ea<0x82B38DE8 or 0x82B3900C<=ea<0x82B392C0): continue
  fn=ida_funcs.get_func(ea); f.write('\n=== %08X %s end=%08X ===\n'%(ea,name,fn.end_ea))
  try: f.write(str(ida_hexrays.decompile(ea))+'\n')
  except Exception as e: f.write('ERROR %s\n'%e)
  for x in idautils.FuncItems(ea): f.write('%08X %s\n'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')))
ida_pro.qexit(0)
