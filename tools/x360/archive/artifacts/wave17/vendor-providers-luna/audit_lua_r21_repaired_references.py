#!/usr/bin/env python3
import hashlib,json,mmap,pathlib,struct,sys

def refs_for(path, targets):
 r={'definitions':[],'undefined':[],'relocations':[]}
 with open(path,'rb') as f:
  if f.seek(0,2)<20:return r,'short'
  f.seek(0);m=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
  try:
   machine,nsec,ts,sp,ns,opts,flags=struct.unpack_from('<HHLLLHH',m,0)
   if not sp or not ns:return r,None
   st=sp+ns*18; size=struct.unpack_from('<L',m,st)[0]; strings=m[st:min(len(m),st+size)]; names={};i=0
   while i<ns:
    off=sp+i*18; field=m[off:off+8]
    if field[:4]==b'\0\0\0\0':
     noff=struct.unpack_from('<L',field,4)[0];end=strings.find(b'\0',noff);raw=strings[noff:end if end>=0 else len(strings)]
    else:raw=field.split(b'\0',1)[0]
    name=raw.decode('ascii','replace');val,sec,typ,storage,aux=struct.unpack_from('<LhHBB',m,off+8);names[i]=name
    if name.lower() in targets and storage==2:
     row={'name':name,'symbol_index':i,'section':sec,'value':val}
     (r['definitions'] if sec>0 else r['undefined'] if sec==0 else r['definitions']).append(row)
    i+=1+aux
   base=20+opts
   for si in range(nsec):
    h=base+si*40
    if h+40>len(m):break
    rp=struct.unpack_from('<L',m,h+24)[0];nr=struct.unpack_from('<H',m,h+32)[0];ch=struct.unpack_from('<L',m,h+36)[0]
    if not rp or not nr:continue
    start=0
    if nr==0xffff and ch&0x01000000:nr=max(0,struct.unpack_from('<L',m,rp)[0]-1);start=1
    for ri in range(start,nr):
     ro=rp+ri*10
     if ro+10>len(m):break
     idx=struct.unpack_from('<L',m,ro+4)[0];name=names.get(idx)
     if name and name.lower() in targets:r['relocations'].append({'name':name,'section_index':si+1,'relocation_index':ri})
   return r,None
  finally:m.close()

manifest_path=pathlib.Path(sys.argv[1]); outpath=pathlib.Path(sys.argv[2]);j=json.loads(manifest_path.read_text(encoding='utf-8'))
objects=j['selected']+j['excluded']; targets={'luaopen_os','lual_openlibs','luaopen_io','luaopen_base','luaopen_package','luaopen_table','luaopen_string','luaopen_math','luaopen_debug','system','_system','_popen','_pclose','popen','pclose'}
report={'schema':'r21-repaired-lua-object-reference-boundary-v1','head':j.get('head'),'manifest_path':str(manifest_path),'manifest_sha256':hashlib.sha256(manifest_path.read_bytes()).hexdigest(),'object_count':len(objects),'scanned':0,'errors':[],'symbols':{t:{'definitions':[],'undefined':[],'relocations':[]} for t in sorted(targets)}}
for obj in objects:
 path=pathlib.Path(obj['object']); ref,error=refs_for(path,targets)
 if error:report['errors'].append({'source':obj['source'],'object':str(path),'error':error});continue
 report['scanned']+=1
 for kind in ('definitions','undefined','relocations'):
  for row in ref[kind]:
   target=row['name'].lower();entry=dict(row);entry.update({'source':obj['source'],'object':str(path),'outside_lua_source':not obj['source'].startswith('vendor/lua/src/')})
   report['symbols'][target][kind].append(entry)
for target in report['symbols']:
 for kind in report['symbols'][target]:report['symbols'][target][kind].sort(key=lambda x:(x['source'],x.get('section_index',0),x.get('value',0)))
outpath.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('R21 repaired objects scanned',report['scanned'],'of',report['object_count'],'errors',len(report['errors']))
for key in ('luaopen_os','lual_openlibs','system','luaopen_io','luaopen_base','luaopen_package','luaopen_table','luaopen_string','luaopen_math','luaopen_debug'):
 r=report['symbols'][key];print(key,'defs',[x['source'] for x in r['definitions']],'undefined',[x['source'] for x in r['undefined']],'outside relocs',sorted(set(x['source'] for x in r['relocations'] if x['outside_lua_source'])))
print('audit hash',hashlib.sha256(outpath.read_bytes()).hexdigest())
