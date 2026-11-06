import hashlib,json,pathlib,struct
W=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); O=W/'r174-sound-descriptors-brn-global-emitter'
d={};exec((W/'r155-apt-view-linked/verify_r155_apt_view.py').read_text().split('\nprevious = json.loads')[0],d)
p=W/'native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex'; image=p.read_bytes(); sha=lambda x:hashlib.sha256(x).hexdigest(); assert sha(image)=='6822f1c1f5d49e0b5df6a34d9fcc619bd331f4a4a5fe3081a0b10b2a03399655'; offset=struct.unpack_from('>I',image,8)[0]-0x82000000
read=lambda a,n:image[offset+a:offset+a+n]
rows=[('BrnStateManager','BrnStateManager',-1,0x82f2e7ec,0x82c61b58,0x82682aa8,0x82682ab8,0x826fb5f0,0x82cd1de4),('BrnGlobalStateManager','GlobalStateManager',0,0x82f2f85c,0x82c62d88,0x826867d8,0x826867e8,0x826fe380,0x82cd1fe0),('BrnEmitterStateManager','EmitterStateManager',7,0x82f2f84c,0x82c62d78,0x826867a8,0x826867b8,0x82701518,0x82cd1fdc)]
results=[]
for file,cls,id,addr,thunk,getter,getname,factory,slot in rows:
 path=O/(file+'.obj');data,m,syms,secs=d['coff'](path); assert m==0x1f2
 def sym(prefix):
  found=[x for x in syms.values() if x['name'].startswith(prefix) and x['section']>0];assert len(found)==1,(prefix,found);return found[0]
 def raw(s):return secs[s['section']]['raw'][s['value']:]
 def rels(s,n):return [(x['offset']-s['value'],x['type'],x['symbol']) for x in secs[s['section']]['relocs'] if s['value']<=x['offset']<s['value']+n and x['type']!=0x12]
 desc=sym('?sTypeInfo@'+cls+'@');orig=struct.unpack('>4I',read(addr,16)); assert orig[0]==id&0xffffffff and orig[2:]==(0x82f2faa0,factory);assert read(orig[1],len(cls)+1)==cls.encode()+b'\0'
 assert raw(desc)[:16]==struct.pack('>4I',id&0xffffffff,0,0,0)
 rr=rels(desc,16);assert [(x,y) for x,y,_ in rr]==[(4,2),(8,2),(12,2)]
 string,base,create=[z for _,_,z in rr];assert string['section']>0 and raw(string).startswith(cls.encode()+b'\0');assert base['name'].startswith('?sTypeInfo@StateManager@Logic@CgsSound@@'); assert create['name'].startswith('?CreateObject@'+cls+'@')
 checks={}
 for prefix,original,length in [('?GetTypeInfo@'+cls+'@',getter,12),('?GetStaticTypeInfo@'+cls+'@',getter,12),('?GetTypeName@'+cls+'@',getname,16),('??__Eg'+cls+'TypeRegistration@',thunk,12)]:
  s=sym(prefix); code=raw(s)[:length]; origcode=read(original,length);rel=rels(s,length); assert [(x,y) for x,y,_ in rel]==([(0,16),(4,17),(8,6)] if prefix.startswith('??__E') else [(0,16),(4,17)])
  assert all(x[2]['name']==desc['name'] for x in rel[:2]);
  if prefix.startswith('??__E'):assert rel[2][2]['name'].startswith('?AddToClassTypeInfoArray@StateManager@')
  a=bytearray(code);b=bytearray(origcode)
  for off,typ,_ in rel:
   mask=0x03fffffc if typ==6 else 0xffff
   for buf in (a,b):struct.pack_into('>I',buf,off,struct.unpack_from('>I',buf,off)[0]&~mask)
  assert a==b,(cls,prefix,code.hex(),origcode.hex())
  checks[prefix]={'artist_va':hex(original),'artist':origcode.hex(),'coff':code.hex(),'relocations':[{'offset':hex(x),'type':hex(y),'symbol':z['name']} for x,y,z in rel]}
 assert struct.unpack('>I',read(slot,4))[0]==thunk
 w=struct.unpack('>I',read(thunk+8,4))[0]; disp=w&0x03fffffc; disp=disp-(1<<26) if disp&(1<<25) else disp; assert thunk+8+disp==0x8268dfe8
 init=sym('??__Eg'+cls+'TypeRegistration@'); crt=[(idx,sec) for idx,sec in secs.items() if sec['name']=='.CRT$XCU' and any(r['symbol']['name']==init['name'] and r['type']==2 for r in sec['relocs'])]; assert len(crt)==1
 results.append({'class':cls,'object_sha256':sha(data),'artist_descriptor':{'address':hex(addr),'words':[hex(x) for x in orig],'name':cls,'crt_slot':hex(slot),'thunk':hex(thunk)},'coff_descriptor':{'symbol':desc['name'],'raw':raw(desc)[:16].hex(),'relocations':[{'offset':hex(x),'type':hex(y),'target':z['name']} for x,y,z in rr]},'checks':checks,'crt_section':crt[0][0]})
out={'state':'pass','artist_image_sha256':sha(image),'base':'2ddfc33a3ba62295655a2f897711e5d343df20b2','tree':'088a9731a493058f90c755dbe4b9e3636abb12b7','results':results,'limitations':['COFF proof only; CRT final link retention and relative ordering not verified','typed registrar adds unused object storage','no full link or runtime']};(O/'COFF_PROOF.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS',sha((O/'COFF_PROOF.json').read_bytes()))
