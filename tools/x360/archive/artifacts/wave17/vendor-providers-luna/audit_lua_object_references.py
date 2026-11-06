#!/usr/bin/env python3
import hashlib, json, mmap, pathlib, struct, sys

def coff_references(path, targets_lower):
    result={'definitions':[],'undefined':[],'relocations':[]}
    try:f=open(path,'rb')
    except OSError as e:return result,'open: '+str(e)
    with f:
        if f.seek(0,2)<20:return result,'short object'
        f.seek(0); mm=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
        try:
            machine,nsec,timestamp,symptr,nsyms,opts,flags=struct.unpack_from('<HHLLLHH',mm,0)
            if not symptr or not nsyms:return result,None
            strptr=symptr+nsyms*18
            if strptr+4>len(mm):return result,'bad string table offset'
            strsize=struct.unpack_from('<L',mm,strptr)[0]; strings=mm[strptr:min(len(mm),strptr+strsize)]
            names={}; i=0
            while i<nsyms:
                off=symptr+i*18
                if off+18>len(mm):return result,'bad symbol table extent'
                field=mm[off:off+8]
                if field[:4]==b'\0\0\0\0':
                    noff=struct.unpack_from('<L',field,4)[0]; end=strings.find(b'\0',noff); raw=strings[noff:end if end>=0 else len(strings)]
                else:raw=field.split(b'\0',1)[0]
                value,sec,typ,storage,aux=struct.unpack_from('<LhHBB',mm,off+8)
                name=raw.decode('ascii','replace'); names[i]=name; low=name.lower()
                if low in targets_lower and storage==2:
                    row={'name':name,'symbol_index':i,'section':sec,'value':value}
                    if sec>0:result['definitions'].append(row)
                    elif sec==0:result['undefined'].append(row)
                i+=1+aux
            base=20+opts
            for si in range(nsec):
                ho=base+si*40
                if ho+40>len(mm):break
                relptr=struct.unpack_from('<L',mm,ho+24)[0]; nrel=struct.unpack_from('<H',mm,ho+32)[0]; ch=struct.unpack_from('<L',mm,ho+36)[0]
                if not relptr or not nrel:continue
                start=0
                if nrel==0xffff and ch&0x01000000:
                    nrel=max(0,struct.unpack_from('<L',mm,relptr)[0]-1);start=1
                for ri in range(start,nrel):
                    ro=relptr+ri*10
                    if ro+10>len(mm):break
                    symidx=struct.unpack_from('<L',mm,ro+4)[0]; name=names.get(symidx)
                    if name and name.lower() in targets_lower:result['relocations'].append({'name':name,'symbol_index':symidx,'section_index':si+1,'relocation_index':ri})
            return result,None
        finally:mm.close()

inv_path=pathlib.Path(sys.argv[1]);out_path=pathlib.Path(sys.argv[2]);inv=json.loads(inv_path.read_text(encoding='utf-8'))
targets=['luaopen_os','luaL_openlibs','luaopen_io','luaopen_base','luaopen_package','luaopen_table','luaopen_string','luaopen_math','luaopen_debug','system','_system','_popen','_pclose','popen','pclose']; target_lower={x.lower() for x in targets}
rows={x.lower():{'definitions':[],'undefined':[],'relocations':[]} for x in targets}
report={'schema':'r19-lua-object-reference-boundary-v1','diagnostic_head':inv.get('head'),'diagnostic_scope':inv.get('scope'),'invocation_sha256':hashlib.sha256(inv_path.read_bytes()).hexdigest(),'object_count':len(inv['objects']),'scanned':0,'machine_histogram':{},'symbols':rows,'errors':[]}
for obj in inv['objects']:
 path=pathlib.Path(obj['object']);refs,error=coff_references(path,target_lower)
 if error:report['errors'].append({'source':obj['source'],'object':str(path),'error':error});continue
 report['scanned']+=1
 try:
  with open(path,'rb') as f:machine=struct.unpack('<H',f.read(2))[0]
  report['machine_histogram'][hex(machine)]=report['machine_histogram'].get(hex(machine),0)+1
 except Exception:pass
 for kind in ('definitions','undefined','relocations'):
  for row in refs[kind]:
   key=row['name'].lower();enriched=dict(row);enriched.update({'source':obj['source'],'object':str(path),'outside_lua_source':not obj['source'].startswith('vendor/lua/src/')});rows[key][kind].append(enriched)
for key in rows:
 for kind in rows[key]:rows[key][kind].sort(key=lambda r:(r['source'],r.get('section_index',0),r.get('value',0)))
out_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('objects scanned',report['scanned'],'of',report['object_count'],'errors',len(report['errors']))
for key in ('luaopen_os','lual_openlibs','system','luaopen_io','luaopen_base','luaopen_package','luaopen_table','luaopen_string','luaopen_math','luaopen_debug'):
 r=rows[key]; print(key,'definitions',[x['source'] for x in r['definitions']],'undefined',[x['source'] for x in r['undefined']],'relocation sources',[x['source'] for x in r['relocations']])
print('audit path',out_path,'sha256',hashlib.sha256(out_path.read_bytes()).hexdigest())
