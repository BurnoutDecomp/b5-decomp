import ida_typeinf,ida_pro,json,sys
out=[];til=ida_typeinf.get_idati()
for i in range(1,ida_typeinf.get_ordinal_limit(til)):
 name=ida_typeinf.get_numbered_type_name(til,i)
 if not name or ('RelocatorJob' not in name and 'RelocationTestParams' not in name):continue
 t=ida_typeinf.tinfo_t();ok=t.get_numbered_type(til,i);u=ida_typeinf.udt_type_data_t();members=[]
 if ok and t.get_udt_details(u):
  for m in u:members.append({'name':m.name,'offset_bits':m.offset,'size_bits':m.size,'type':str(m.type),'type_size':m.type.get_size(),'alignment':m.type.get_alignment()})
  out.append({'name':name,'size':t.get_size(),'alignment':t.get_alignment(),'sda':u.sda,'members':members})
open('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/relocator_job_terra/'+sys.argv[1]+'.json','w').write(json.dumps(out,indent=2))
ida_pro.qexit(0)

