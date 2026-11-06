"""Focused ARTIST versus XDK vehicle-manager descriptor, getters and CRT proof."""
import hashlib,json,pathlib,runpy,struct,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
P=pathlib.Path;out=P(__file__).resolve().parent;wave=out.parent;m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();report=json.loads((out/'compile-results.json').read_text());assert all(x['exit']==0 for x in report['results'])
order=json.loads((wave/'r173-state-descriptor/ARTIST_CRT_ORDER.json').read_text());rows={}
def read_symbols(data):
 _,n,_,table,count,opt,_=struct.unpack_from('<HHIIIHH',data);assert opt==0;i=0;result=[]
 while i<count:
  name,value,sec=m['object_symbol'](data,i);storage,aux=struct.unpack_from('<BB',data,table+i*18+16);result.append((i,name,value,sec,storage));i+=1+aux
 return result
def section(data,sec):return struct.unpack_from('<8sIIIIIIHHI',data,20+40*(sec-1))
def rels(data,sec):
 _,_,_,size,raw,rel,_,nr,_,_=section(data,sec);return [struct.unpack_from('<IIH',data,rel+10*j) for j in range(nr)]
def name_ref(data,rels,off,kind):
 i,=[i for o,i,k in rels if o==off and k==kind];return m['object_symbol'](data,i)[0]
for name,id,descriptor,thunk,getter,namer,factory in [('PlayerVehicleStateManager',1,0x82f2e87c,0x82c61d60,0x82683ff8,0x82684008,0x827022d8),('AIVehicleStateManager',2,0x82f2e88c,0x82c61d98,0x82684018,0x82684028,0x82702358)]:
 entry,=[x for x in order['entries'] if x['name']==name]; assert int(entry['descriptor'],16)==descriptor and int(entry['thunk'],16)==thunk and entry['id']==id
 rid,rname,rbase,rfactory=struct.unpack('>4I',x360rd.rd(descriptor,16));assert rid==id and rbase==0x82f2faa0 and rfactory==factory and x360rd.rd(rname,len(name)+1)==name.encode()+b'\0'
 assert struct.unpack('>I',x360rd.rd(int(entry['crt_slot'],16),4))[0]==thunk
 assert x360rd.rd(thunk,8)==bytes.fromhex('3d6082f3386b')+struct.pack('>H',descriptor&0xffff)
 assert m['branch_target'](thunk+8,struct.unpack('>I',x360rd.rd(thunk+8,4))[0])==0x8268dfe8
 assert x360rd.rd(getter,12)==bytes.fromhex('3d6082f3386b')+struct.pack('>H',descriptor&0xffff)+bytes.fromhex('4e800020')
 assert x360rd.rd(namer,16)==bytes.fromhex('3d6082f3396b')+struct.pack('>H',descriptor&0xffff)+bytes.fromhex('806b00044e800020')
 p=out/('Brn'+name+'.obj');data=p.read_bytes();symbols=read_symbols(data);desc,=[x for x in symbols if x[1].startswith('?sTypeInfo@'+name+'@Vehicles@BrnSound@@')];_,ds,dv,sec,_=desc; sn,_,_,size,raw,_,_,_,_,_=section(data,sec);assert sn.rstrip(b'\0')==b'.data' and data[raw+dv:raw+dv+16]==struct.pack('>I',id)+bytes(12)
 rr=rels(data,sec);types=[name_ref(data,rr,dv+o,2) for o in (4,8,12)];assert types[0].startswith('??_C@') and types[1].startswith('?sTypeInfo@StateManager@Logic@CgsSound@@') and types[2].startswith('?CreateObject@'+name+'@Vehicles@BrnSound@@')
 idx,=[i for o,i,k in rr if o==dv+4 and k==2];assert m['symbol_literal'](data,idx)==name
 init,=[x for x in symbols if x[1].startswith('??__Eg'+name+'TypeRegistration@')];_,initname,iv,isec,_=init;sn,_,_,sz,raw,_,_,_,_,_=section(data,isec);ic=data[raw:raw+sz];ir=rels(data,isec);assert sn==b'.text$yc' and iv==0 and ic[:8]==bytes.fromhex('3d600000386b0000') and struct.unpack_from('>I',ic,8)[0]&0xfc000003==0x48000000
 assert name_ref(data,ir,0,16)==ds and name_ref(data,ir,4,17)==ds and name_ref(data,ir,8,6).startswith('?AddToClassTypeInfoArray@StateManager@Logic@CgsSound@@')
 crt=[]
 for secn in range(1,struct.unpack_from('<H',data,2)[0]+1):
  sn,_,_,sz,raw,_,_,_,_,_=section(data,secn)
  if sn==b'.CRT$XCU':
   for off,i,k in rels(data,secn):
    if k==2 and m['object_symbol'](data,i)[0]==initname:assert data[raw+off:raw+off+4]==bytes(4);crt.append(off)
 assert len(crt)==1
 methods={}
 for method,expected in [('GetStaticTypeInfo','3d600000386b00004e800020'),('GetTypeInfo','3d600000386b00004e800020'),('GetTypeName','3d600000396b0000806b00044e800020')]:
  s,=[x for x in symbols if x[1].startswith('?'+method+'@'+name+'@Vehicles@BrnSound@@') and x[3]>0];symbol=s[1];code,rel=m['collect'](p,symbol);assert code==bytes.fromhex(expected) and name_ref(data,rel,0,16)==ds and name_ref(data,rel,4,17)==ds;methods[method]={'symbol':symbol,'bytes':len(code)}
 rows[name]={'object_sha256':sha(p),'artist_descriptor':hex(descriptor),'artist_thunk':hex(thunk),'artist_vtable_getter':hex(getter),'artist_vtable_name_getter':hex(namer),'descriptor_symbol':ds,'descriptor_object_offset':hex(dv),'descriptor_relocations':types,'initializer_symbol':initname,'crt_offset':hex(crt[0]),'methods':methods}
r={'state':'pass','base':'2ddfc33a3ba62295655a2f897711e5d343df20b2','tree':'088a9731a493058f90c755dbe4b9e3636abb12b7','compile_report_sha256':sha(out/'compile-results.json'),'managers':rows,'scope':'ARTIST raw descriptor/CRT/getters and focused XDK COFF relocations; no linked image, CRT ordering or runtime proof'};(out/'PROOF.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS',sha(out/'PROOF.json'))
