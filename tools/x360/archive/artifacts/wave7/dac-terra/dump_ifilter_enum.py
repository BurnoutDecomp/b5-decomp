import ida_typeinf,ida_pro,json
n='rw::audio::core::IFilter::FilterOutputMode';t=ida_typeinf.tinfo_t();f=t.get_named_type(ida_typeinf.get_idati(),n);e=ida_typeinf.enum_type_data_t();out={'found':f,'ok':t.get_enum_details(e),'members':[]}
for m in e:out['members'].append({'name':m.name,'value':m.value})
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/ifilter-enum.json','w').write(json.dumps(out,indent=2));ida_pro.qexit(0)
