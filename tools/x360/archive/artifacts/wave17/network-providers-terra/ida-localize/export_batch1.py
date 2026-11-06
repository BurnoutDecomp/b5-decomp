import ida_hexrays, ida_funcs, ida_lines, idautils, idc, json, traceback, os
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\network-providers-terra\ida-batch1'
os.makedirs(out,exist_ok=True)
ready=ida_hexrays.init_hexrays_plugin()
addresses=[0x82B263F8,0x82B26C60,0x82B258A8,0x82B25B6C,0x82B1C8B8,0x82B1AE70,0x82B22D68,0x82B1F6C8,0x82B1EE78,0x82B27798,0x82B20C90,0x82B21B70,0x82B27DF8,0x82AD5F40,0x82AD4920,0x82B0F1B8]
summary=[]
for ea in addresses:
 row={'address':'0x%08X'%ea,'hexrays_ready':ready}
 try:
  fn=ida_funcs.get_func(ea);row['start']='0x%08X'%fn.start_ea if fn else None;row['end']='0x%08X'%fn.end_ea if fn else None;row['name']=idc.get_func_name(ea)
  if fn:
   cf=ida_hexrays.decompile(ea);row['pseudocode']=str(cf) if cf else None
   row['assembly']='\n'.join('0x%08X  %s'%(x,ida_lines.tag_remove(idc.generate_disasm_line(x,0) or '')) for x in idautils.FuncItems(fn.start_ea))
 except Exception:row['error']=traceback.format_exc()
 with open(os.path.join(out,'0x%08X.json'%ea),'w',encoding='utf-8') as f:json.dump(row,f,indent=2)
 summary.append({'address':row['address'],'start':row.get('start'),'end':row.get('end'),'name':row.get('name'),'error':row.get('error'),'pseudo_chars':len(row.get('pseudocode') or '')})
with open(os.path.join(out,'INDEX.json'),'w',encoding='utf-8') as f:json.dump(summary,f,indent=2)
idc.qexit(0)
