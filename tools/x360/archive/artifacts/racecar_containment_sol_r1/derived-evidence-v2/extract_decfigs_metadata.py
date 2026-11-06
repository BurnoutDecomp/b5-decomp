from pathlib import Path
import hashlib, json, time, traceback
import ida_typeinf, ida_netnode, ida_segment, ida_bytes, ida_nalt, ida_pro, ida_ida

OUT = Path(__file__).resolve().parent
result = {}
try:
    result['input_file_path'] = ida_nalt.get_input_file_path()
    result['input_md5'] = ida_nalt.retrieve_input_file_md5().hex()
    result['database_platform'] = {'compiler':ida_ida.inf_get_cc_id(),'is_be':ida_ida.inf_is_be()}
    targets = ['BrnReplays::BaseSerialiser','BrnReplays::RaceCarEntitySerialiser',
               'BrnReplays::PropEntitySerialiser','BrnReplays::GuiModuleSerialiser',
               'BrnReplays::EffectsSerialiser','BrnReplays::SoundSerialiser',
               'BrnReplays::TrafficEntitySerialiser','BrnWorld::RaceCarEntityModule',
               'BrnWorld::PropEntityModule','BrnEffects::BrnCrashTriangleCache']
    def attributes(t):
        attrs = ida_typeinf.type_attrs_t()
        t.get_attrs(attrs,True)
        return [{'key':a.key,'value_hex':bytes(a.value).hex()} for a in attrs]
    til = ida_typeinf.get_idati()
    types = []
    for i in range(1,ida_typeinf.get_ordinal_limit(til)):
        name = ida_typeinf.get_numbered_type_name(til,i)
        if name not in targets: continue
        t = ida_typeinf.tinfo_t(); assert t.get_numbered_type(til,i)
        u = ida_typeinf.udt_type_data_t(); assert t.get_udt_details(u)
        serial = t.serialize()
        row = {'ordinal':i,'name':name,'declaration':str(t),'size':t.get_size(),
               'alignment':t.get_alignment(),'declalign_raw':t.get_declalign(),
               'sda_raw':u.sda,'sda_decoded_bytes':(1<<(u.sda-1)) if u.sda else None,
               'pack_raw':u.pack,'pack_decoded_bytes':(1<<u.pack) if u.pack else None,
               'effective_alignment':u.effalign,'total_size':u.total_size,'unpadded_size':u.unpadded_size,
               'taudt_bits':u.taudt_bits,'attributes':attributes(t),
               'serialization':[v.hex() if isinstance(v,bytes) else str(v) for v in serial],
               'members':[]}
        try: row['printed_definition'] = t._print(name,ida_typeinf.PRTYPE_MULTI|ida_typeinf.PRTYPE_DEF|ida_typeinf.PRTYPE_CPP)
        except Exception as e: row['printed_definition_error']=str(e)
        for m in u:
            row['members'].append({'name':m.name,'offset_bits':m.offset,'size_bits':m.size,
                'type':str(m.type),'type_size':m.type.get_size(),'type_alignment':m.type.get_alignment(),
                'type_declalign_raw':m.type.get_declalign(),'tafld_bits':m.tafld_bits,
                'is_baseclass':m.is_baseclass(),'attributes':attributes(m.type),
                'member_metadata':{k:getattr(m,k) for k in ['fda','effalign'] if hasattr(m,k)}})
        types.append(row)
    result['types']=types
    segments=[]
    s=ida_segment.get_first_seg()
    while s:
        name=ida_segment.get_segm_name(s)
        row={'name':name,'start':hex(s.start_ea),'end':hex(s.end_ea),'size':s.end_ea-s.start_ea,'class':ida_segment.get_segm_class(s)}
        if 'debug' in name.lower() and row['size'] < 512*1024*1024:
            data=ida_bytes.get_bytes(s.start_ea,row['size'])
            if data:
                path=OUT/('raw-section-'+name.replace('/','_')+'.bin');path.write_bytes(data)
                row.update(path=str(path),sha256=hashlib.sha256(data).hexdigest())
        segments.append(row);s=ida_segment.get_next_seg(s.start_ea)
    result['segments']=segments
    relevant=[]; node=ida_netnode.netnode();count=0;started=time.monotonic();more=node.start()
    while more and count<2000000 and time.monotonic()-started<40:
        name=node.get_name()
        if name and any(k in str(name).lower() for k in ['dwarf','debug','elf','input','load']):
            row={'index':hex(node.index()),'name':name,'tags':{}}
            for tag in ['S','B','A','D','I','T','F','C']:
                try:
                    index=node.supfirst(ord(tag));samples=[]
                    while index!=ida_netnode.BADNODE and len(samples)<6:
                        val=node.supval(index,ord(tag));samples.append({'index':hex(index),'value_length':len(val) if val else 0,'prefix_hex':val[:128].hex() if isinstance(val,bytes) else str(val)})
                        index=node.supnext(index,ord(tag))
                    if samples:row['tags'][tag]=samples
                except Exception as e: row['tags'][tag]={'error':str(e)}
            relevant.append(row)
        count+=1;more=node.next()
    result['netnode_scan']={'examined':count,'complete':not more,'seconds':time.monotonic()-started,'relevant_nodes':relevant}
except Exception:
    result['error']=traceback.format_exc()
(OUT/'decfigs-retained-metadata.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
ida_pro.qexit(0)
