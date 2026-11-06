import ida_bytes,idc,ida_pro,json
rows=[]
for i in range(180):
 a=0x82F812B8+i*8; code=ida_bytes.get_dword(a); ptr=ida_bytes.get_dword(a+4); raw=idc.get_strlit_contents(ptr,-1,idc.STRTYPE_C); rows.append({'index':i,'code':code,'pointer':ptr,'name':raw.decode('utf-8') if raw is not None else None})
out=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave28\dirty-diagnostics-terra\error-table.json'
with open(out,'w') as f: json.dump(rows,f,indent=2)
with open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave28\dirty-diagnostics-terra\strings.txt','w') as f:
 for a in [0x82F81870,0x82F81878,0x82146A38]: f.write('%08x %r\n'%(a,idc.get_strlit_contents(a,-1,idc.STRTYPE_C)))
ida_pro.qexit(0)
