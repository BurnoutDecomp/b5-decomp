import struct,pathlib,json
out=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/netconn-thread-terra'); items=[]
for fn in ('netconn_friends_x360.obj','netconn_owner_x360.obj'):
 b=(out/fn).read_bytes(); machine,n,ts,sp,ns,opt,ch=struct.unpack_from('<HHIIIHH',b,0); strbase=sp+ns*18
 def name8(raw):
  if raw[:4]==b'\0'*4:
   o=struct.unpack_from('<I',raw,4)[0]; end=b.find(b'\0',strbase+o); return b[strbase+o:end].decode('ascii',errors='replace')
  return raw.rstrip(b'\0').decode('ascii',errors='replace')
 syms={}; i=0
 while i<ns:
  off=sp+i*18; raw,val,sec,typ,storage,aux=struct.unpack_from('<8sIhHBB',b,off); syms[i]={'name':name8(raw),'section':sec,'value':val,'storage':storage}; i+=1+aux
 sections=[]
 for j in range(n):
  off=20+opt+j*40; raw,vs,va,sz,data,rel,lin,nr,nl,fl=struct.unpack_from('<8sIIIIIIHHI',b,off); rs=[]
  for k in range(nr):
   rva,si,rt=struct.unpack_from('<IIH',b,rel+10*k); sym=syms.get(si,{'name':f'<aux:{si}>'}); rs.append({'offset':rva,'type':hex(rt),'symbol':sym['name']})
  public=[v['name'] for v in syms.values() if v['section']==j+1 and v['storage']==2]
  if public and any('NetConnX360Close' in x or x=='NetConnStartup' for x in public): sections.append({'index':j+1,'name':raw.rstrip(b'\0').decode(errors='replace'),'public':public,'code_hex':b[data:data+sz].hex(),'relocations':rs})
 items.append({'file':fn,'machine':hex(machine),'sections':sections})
(out/'coff-relocations.json').write_text(json.dumps(items,indent=2));
for f in items:
 print(f['file'],f['machine'])
 for sec in f['sections']:
  print('section',sec['index'],sec['public']); print('relocs',[(r['offset'],r['type'],r['symbol']) for r in sec['relocations'] if 'Close' in r['symbol'] or 'RtlLeave' in r['symbol'] or r['symbol']=='CreateThread'])
