from pathlib import Path
import hashlib,json,time,traceback
import ida_typeinf,ida_netnode,ida_nalt,ida_pro
OUT=Path(__file__).resolve().parent
result={}
try:
    til=ida_typeinf.get_idati();rows=[];names=[]
    for i in range(1,ida_typeinf.get_ordinal_limit(til)):
        n=ida_typeinf.get_numbered_type_name(til,i)
        if not n or 'Serialiser' not in n:continue
        names.append({'ordinal':i,'name':n})
        if n not in ['BrnReplays::RaceCarEntitySerialiser','BrnReplays::BaseSerialiser']:continue
        t=ida_typeinf.tinfo_t();assert t.get_numbered_type(til,i)
        record=ida_typeinf.get_numbered_type(til,i)
        detached=ida_typeinf.tinfo_t(t);detached.detach()
        row={'ordinal':i,'name':n,'tid':hex(t.get_tid()),
             'numbered_raw':[x.hex() if isinstance(x,bytes) else str(x) for x in record],
             'detached_serialization':[x.hex() if isinstance(x,bytes) else str(x) for x in detached.serialize()]}
        node=ida_netnode.netnode(t.get_tid());row['type_netnode_name']=node.get_name()
        row['type_supvals']={}
        for tag in ['S','A','D','I','T','B']:
            samples=[];idx=node.supfirst(ord(tag))
            while idx!=ida_netnode.BADNODE and len(samples)<20:
                val=node.supval(idx,ord(tag));samples.append({'index':hex(idx),'length':len(val) if val else 0,'value_hex':val.hex() if isinstance(val,bytes) else str(val)});idx=node.supnext(idx,ord(tag))
            if samples:row['type_supvals'][tag]=samples
        rows.append(row)
    result['serialiser_types']=names;result['selected']=rows
    root=ida_netnode.netnode('Root Node');root_id=root.index();result['root_node_index']=hex(root_id)
    start=(root_id & 0xFF00000000000000) if root_id>0xFFFFFFFF else (root_id & 0xFF000000)
    n=ida_netnode.netnode(start);more=n.next();count=0;internal=[];started=time.monotonic()
    while more and count<200000 and time.monotonic()-started<30:
        name=n.get_name()
        if name:
            item={'index':hex(n.index()),'name':name}
            if any(k in name.lower() for k in ['dwarf','debug','elf','loader','input']):
                item['tags']={}
                for tag in ['S','A','D','B','I','T']:
                    idx=n.supfirst(ord(tag));samples=[]
                    while idx!=ida_netnode.BADNODE and len(samples)<20:
                        val=n.supval(idx,ord(tag));samples.append({'index':hex(idx),'length':len(val) if val else 0,'prefix_hex':val[:256].hex() if isinstance(val,bytes) else str(val)});idx=n.supnext(idx,ord(tag))
                    if samples:item['tags'][tag]=samples
            internal.append(item)
        count+=1;more=n.next()
    result['internal_nodes']={'start':hex(start),'examined':count,'complete':not more,'seconds':time.monotonic()-started,'named':internal}
except Exception:result['error']=traceback.format_exc()
(OUT/'decfigs-internal-metadata.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
ida_pro.qexit(0)
