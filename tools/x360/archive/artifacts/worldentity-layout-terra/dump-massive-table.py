import ida_bytes, ida_name, ida_xref, ida_funcs, ida_pro, json, ida_nalt
start=0x82F2413C
r=[]
for i in range(-3,17):
 a=start+4*i;p=ida_bytes.get_dword(a); s=ida_bytes.get_strlit_contents(p,-1,ida_nalt.STRTYPE_C)
 r.append({'index':i,'address':hex(a),'value':hex(p),'target_name':ida_name.get_name(p),'string':s.decode('ascii','replace') if s else None,'item_head':hex(ida_bytes.get_item_head(a)),'item_size':ida_bytes.get_item_size(a),'name':ida_name.get_name(a)})
open(r'D:\Reverse\BP-X360-Experimental-Artifacts\worldentity-layout-terra\massive-name-table.json','w').write(json.dumps(r,indent=2));ida_pro.qexit(0)
