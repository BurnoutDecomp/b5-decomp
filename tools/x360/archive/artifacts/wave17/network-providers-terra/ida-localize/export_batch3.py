import ida_hexrays,ida_funcs,ida_lines,idautils,idc,json,traceback,os
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-batch3';os.makedirs(out,exist_ok=True);ida_hexrays.init_hexrays_plugin()
for ea in [0x82B24F18]:
 row={'address':'0x%08X'%ea}
 try:
  f=ida_funcs.get_func(ea);row['name']=idc.get_func_name(ea);row['start']='0x%08X'%f.start_ea;row['end']='0x%08X'%f.end_ea;row['pseudocode']=str(ida_hexrays.decompile(ea));row['assembly']='\n'.join('0x%08X  %s'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')) for x in idautils.FuncItems(f.start_ea))
 except Exception:row['error']=traceback.format_exc()
 with open(os.path.join(out,'0x%08X.json'%ea),'w',encoding='utf-8') as f:json.dump(row,f,indent=2)
idc.qexit(0)
