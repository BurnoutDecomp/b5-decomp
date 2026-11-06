import ida_typeinf,ida_pro,json
names=['DelayFilter','IFilter'];out=[];til=ida_typeinf.get_idati()
for name in names:
 n='rw::audio::core::'+name;t=ida_typeinf.tinfo_t();found=t.get_named_type(til,n);u=ida_typeinf.udt_type_data_t();ms=[]
 if found and t.get_udt_details(u):
  for m in u:ms.append({'name':m.name,'offset':hex(m.offset//8),'size':hex(m.size//8),'type':str(m.type)})
 out.append({'name':n,'found':found,'size':t.get_size() if found else None,'align':t.get_alignment() if found else None,'members':ms})
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/delayfilter-prostreet-named.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)


