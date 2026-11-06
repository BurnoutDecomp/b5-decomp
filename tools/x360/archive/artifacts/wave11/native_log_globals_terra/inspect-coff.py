import pathlib,struct,json,hashlib
folder=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave11\native_log_globals_terra'); gate=folder/'native-gate'
def parse(p):
 b=p.read_bytes(); machine,nsec,_,sp,ns,opt,_=struct.unpack_from('<HHIIIHH',b); assert machine==0x1f2; st=sp+18*ns; size=struct.unpack_from('<I',b,st)[0]; strings=b[st:st+size]
 def name(raw):
  if raw[:4]==b'\0'*4:
   off=struct.unpack_from('<I',raw,4)[0];return strings[off:].split(b'\0')[0].decode(errors='replace')
  return raw.split(b'\0')[0].decode(errors='replace')
 secs=[]
 for i in range(nsec):
  v=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*i);secs.append({'name':name(v[0]),'raw_size':v[3],'raw_ptr':v[4],'rel_ptr':v[5],'rel_count':v[7]})
 syms={};i=0
 while i<ns:
  off=sp+18*i; n=name(b[off:off+8]);value,sec,typ,storage,aux=struct.unpack_from('<IhHBB',b,off+8);syms[i]={'name':n,'value':value,'section':sec,'storage':storage};i+=1+aux
 targets=['?gpDebugPrint@Log@CgsDev@@3PAUStrStreamBase@2@A','?gpAssertPrint@Log@CgsDev@@3PAUStrStreamBase@2@A','?gxMessageFilterFlags@Message@CgsDev@@3_KA']; found=[]
 for sym in syms.values():
  if sym['name'] not in targets or sym['section']<=0:continue
  sec=secs[sym['section']-1];off=sym['value'];nr=8 if 'gxMessage' in sym['name'] else 4;raw=b[sec['raw_ptr']+off:sec['raw_ptr']+off+nr];rel=[]
  for j in range(sec['rel_count']):
   addr,idx,typ=struct.unpack_from('<IIH',b,sec['rel_ptr']+10*j)
   if addr==off:rel.append({'offset':addr,'type':typ,'symbol_index':idx,'target':syms[idx]['name']})
  found.append({'symbol':sym['name'],'section':sec['name'],'offset':off,'bytes_hex':raw.hex(),'relocations':rel})
 return {'object':str(p),'sha256':hashlib.sha256(b).hexdigest(),'machine':machine,'definitions':found}
a=parse(next(gate.glob('CgsLogOutput*.obj')));c=parse(next(gate.glob('CgsAssert*.obj')));d={x['symbol']:x for x in a['definitions']};dbg=d['?gpDebugPrint@Log@CgsDev@@3PAUStrStreamBase@2@A'];ass=d['?gpAssertPrint@Log@CgsDev@@3PAUStrStreamBase@2@A'];flag=d['?gxMessageFilterFlags@Message@CgsDev@@3_KA'];assert len(dbg['relocations'])==len(ass['relocations'])==1; assert 'gDefaultDebugOutput' in dbg['relocations'][0]['target'];assert 'gDefaultAssertOutput' in ass['relocations'][0]['target'];assert dbg['relocations'][0]['target']!=ass['relocations'][0]['target'];assert flag['bytes_hex']=='0000000000000001';assert not c['definitions'];out={'output':a,'assert':c,'verified':'distinct pointer relocations; big-endian filter bytes 0000000000000001; no definitions of these globals in CgsAssert.obj'};(folder/'coff-inspection.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
