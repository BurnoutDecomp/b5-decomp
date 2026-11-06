import ida_hexrays, ida_funcs, ida_lines, idautils, idc, json, traceback, os
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-batch2';os.makedirs(out,exist_ok=True);ida_hexrays.init_hexrays_plugin();summary=[]
for ea in [0x82B10250,0x82B11088]:
 row={'address':'0x%08X'%ea}
 try:
  fn=ida_funcs.get_func(ea);row['name']=idc.get_func_name(ea);row['start']='0x%08X'%fn.start_ea;row['end']='0x%08X'%fn.end_ea;row['pseudocode']=str(ida_hexrays.decompile(ea));row['assembly']='\n'.join('0x%08X  %s'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')) for x in idautils.FuncItems(fn.start_ea))
 except Exception:row['error']=traceback.format_exc()
 with open(os.path.join(out,'0x%08X.json'%ea),'w',encoding='utf-8') as f:json.dump(row,f,indent=2)
 summary.append({'address':row['address'],'name':row.get('name'),'error':row.get('error')})
row={'address':'0x82B25B6C','note':'Experimental private IDB fragment with enclosing function deleted; recovered pseudocode must be checked against original complete function assembly.'}
try:
 row['delete_enclosing_ok']=ida_funcs.del_func(0x82B258A8);row['create_fragment_ok']=ida_funcs.add_func(0x82B25B6C,0x82B25D14)
 fn=ida_funcs.get_func(0x82B25B6C);row['start']='0x%08X'%fn.start_ea if fn else None;row['end']='0x%08X'%fn.end_ea if fn else None
 if fn:
  cf=ida_hexrays.decompile(0x82B25B6C);row['pseudocode']=str(cf) if cf else None
  row['assembly']='\n'.join('0x%08X  %s'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')) for x in idautils.FuncItems(fn.start_ea))
except Exception:row['error']=traceback.format_exc()
with open(os.path.join(out,'rank-parser-fragment-experimental.json'),'w',encoding='utf-8') as f:json.dump(row,f,indent=2)
summary.append({'address':row['address'],'error':row.get('error'),'pseudo_chars':len(row.get('pseudocode') or '')})
with open(os.path.join(out,'INDEX.json'),'w',encoding='utf-8') as f:json.dump(summary,f,indent=2)
idc.qexit(0)
