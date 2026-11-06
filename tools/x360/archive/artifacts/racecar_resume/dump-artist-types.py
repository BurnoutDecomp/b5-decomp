import ida_typeinf, ida_pro, json
prefixes=['BrnWorld::RaceCarEntityModule','BrnReplays::RaceCarEntitySerialiser','BrnReplays::BaseSerialiser','rw::math::','BrnWorld::StoredStompeeData','BrnWorld::RaceCarNeedsHidingEvent','CgsModule::ModuleSingleBuffered']
out=[];til=ida_typeinf.get_idati()
for i in range(1,ida_typeinf.get_ordinal_limit(til)):
 name=ida_typeinf.get_numbered_type_name(til,i)
 if not name or not (any(name.startswith(s) for s in prefixes) or 'RaceCarEntity' in name or 'Serialiser' in name):continue
 t=ida_typeinf.tinfo_t();ok=t.get_numbered_type(til,i);u=ida_typeinf.udt_type_data_t();members=[]
 if ok and t.get_udt_details(u):
  for m in u:members.append({'name':m.name,'offset_bits':m.offset,'size_bits':m.size,'type':str(m.type),'type_size':m.type.get_size(),'alignment':m.type.get_alignment(),'declalign':m.type.get_declalign()})
  out.append({'ordinal':i,'name':name,'size':t.get_size(),'alignment':t.get_alignment(),'declalign':t.get_declalign(),'udt_sda':u.sda,'udt_effalign':u.effalign,'udt_unpadded_size':u.unpadded_size,'members':members})
open(r'D:\Reverse\BP-X360-Experimental-Artifacts\racecar_resume\artist-racecar-types.json','w').write(json.dumps(out,indent=2))
ida_pro.qexit(0)
