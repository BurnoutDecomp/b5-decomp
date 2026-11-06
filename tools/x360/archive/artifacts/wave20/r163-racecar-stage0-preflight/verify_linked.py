"""R163 RaceCar Prepare and dynamic vector CRT binding; no runtime claim."""
import argparse,hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
if not __debug__: raise RuntimeError("assertions required")
wave=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20')
m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m']
m['fixed_equal'].__globals__['masks'][2]=0xffffffff
batch=wave/'r163-batch'; candidate='ac550e0556e38f534e1d15cbb5a468b132155286'; tree='a55cbd71baea90e5a9c0f45d7e2d81f6206bda44'; parent='cfa383bfbddd126156185e122979242f5a680683'
source='src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp'; root=Path('C:/Users/adri1/.codex/worktrees/bp360/native-sound')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read_json(p):return json.loads(p.read_text(encoding='utf-8'))
def objfn(obj,name):
 d=obj.read_bytes(); machine,n,_,table,count,opt,_=struct.unpack_from('<HHIIIHH',d);assert machine==0x1f2 and opt==0
 i=0;hits=[]
 while i<count:
  s,value,sec=m['object_symbol'](d,i);storage,aux=struct.unpack_from('<BB',d,table+18*i+16)
  if s==name and 1<=sec<=n: hits.append((value,sec,storage))
  i+=1+aux
 assert len(hits)==1 and hits[0][0]==0,(name,hits)
 sec=hits[0][1];_,_,_,size,raw,rel,_,nr,_,_=struct.unpack_from('<8sIIIIIIHHI',d,20+40*(sec-1))
 return d[raw:raw+size],[struct.unpack_from('<IIH',d,rel+10*j) for j in range(nr)],sec
def target(code,hi,lo):return m['data_target'](code,hi,lo)&0xffffffff
def sym(obj,rels,off,kind):
 i,= [i for o,i,k in rels if o==off and k==kind];return m['object_symbol'](obj.read_bytes(),i)
def prove(folder,head,seal,xex,pe,maphash):
 git=lambda *args:subprocess.check_output(['git','-C',str(root),*args]).decode().strip()
 assert git('rev-parse',head+'^{tree}')==tree and git('rev-parse',head+'^')==parent
 assert git('rev-parse',candidate+'^{tree}')==tree
 assert sha(batch/'TARGETED_OBJECT_BINDING.json')=='55949604fdde92e98b47dfcb90c9e3b088b1a3cdb48c1c723e80c507e9ae0e8c'
 assert sha(batch/'COMPOSED_PPC_CHECK.json')=='ae32feea82e726f7fbc4e36832566a2da283dbb19e80edc7181d2ff3f8422d61'
 assert sha(batch/'ARTIST_VECTOR_INIT.json')=='53d3717f29d9754cfb3387b57a98c75be29346e5fa43950db33de3bb5350df7f'
 binding=read_json(batch/'TARGETED_OBJECT_BINDING.json'); proof=read_json(batch/'COMPOSED_PPC_CHECK.json'); original=read_json(batch/'ARTIST_VECTOR_INIT.json')
 assert all(x['head']==candidate and x['tree']==tree for x in (binding,proof,original))
 inv=read_json(folder/'invocation.json');result=read_json(folder/'result.json');inventory=read_json(folder/'source-inventory.json');image=read_json(folder/'image-verification.json')
 assert inventory['head']==head and result['exit_code']==0 and result['all_post_checks_pass'] and image['head']==head and image['image_sha256']==xex and image['magic']=='XEX2'
 assert sha(folder/'seal.json')==seal
 for name,h in [('native-game.exe',xex),('native-game.pe',pe),('native-game.map',maphash)]:
  path=folder/name;assert sha(path)==h and result['outputs'][str(path)]['sha256']==h
 item,=[x for x in inv['objects'] if x['source']==source]; obj=Path(item['object']);assert sha(obj)==item['sha256']
 ref=Path(binding['results'][0]['artifacts']['object']['path']);assert sha(ref)==binding['results'][0]['artifacts']['object']['sha256']==proof['object_sha256']
 assert git('rev-parse',head+':'+source)==binding['results'][0]['git_blob']; raw=subprocess.check_output(['git','-C',str(root),'show',head+':'+source]);matches=[x for x in inventory['files'] if x['path'].replace('\\','/').lower().endswith(source.lower())]; row,=matches; content=Path(row['path']).read_bytes();assert sha(Path(row['path']))==row['sha256'] and content.replace(b'\r\n',b'\n')==raw
 address,load=m['linked_reader'](folder); symbol=proof['prepare_symbol']; code,rels,section=objfn(obj,symbol); old,oldrels,_=objfn(ref,symbol);assert len(code)==proof['prepare_coff_bytes'];m['fixed_equal'](old,code,oldrels)
 va=address(symbol);linked=load(va,len(code));m['fixed_equal'](code,linked,rels)
 for off,i,k in rels:
  if k==2:
   name,v,sec=m['object_symbol'](obj.read_bytes(),i);assert sec==section and 0<=v<len(code) and name.startswith('$LN');assert struct.unpack_from('>I',linked,off)[0]==va+v+struct.unpack_from('>I',code,off)[0]
 # Four exact assertion messages, line numbers, and common file, each linked to matching bytes.
 for entry in proof['assertions']:
  fire=int(entry['fire_offset'],16); name,_,_=sym(obj,rels,fire,6);assert name.startswith('?FireAssert@Assert@CgsDev@@');assert m['branch_target'](va+fire,struct.unpack_from('>I',linked,fire)[0])==address(name)
  assert struct.unpack_from('>I',code,fire-12)[0]&0xffff==entry['line']
  i,= [i for o,i,k in rels if o==fire-16 and k==16];assert m['symbol_literal'](obj.read_bytes(),i)==entry['message']; ptr=target(linked,fire-16,fire-8);assert load(ptr,len(entry['message'].encode())+1)==entry['message'].encode()+b'\0'
 file=proof['prepare_assert_path'];assert load(target(linked,0x18,0x28),len(file)+1)==file.encode()+b'\0'
 # Pin key operand words to the exact reviewed preflight, including the two VMX loads.
 for off in (0x9c,0xa0,0xa4,0xc4,0xc8,0xcc,0xf0,0xf4,0x100,0x104,0x110,0x114,0x118,0x11c):assert code[off:off+4]==old[off:off+4],hex(off)
 call=0x120; name,_,_=sym(obj,rels,call,6);assert name.startswith('?Construct@WorldMap2D@CgsWorld@@');calltarget=m['branch_target'](va+call,struct.unpack_from('>I',linked,call)[0]);assert calltarget==address(name)
 # Both selected COFF CRT relocations must occupy adjacent entries in the same .CRT$XCU section.
 data=obj.read_bytes(); _,count,_,table,n,_,_=struct.unpack_from('<HHIIIHH',data); crtsections=[]
 for sec in range(1,count+1):
  section_name,_,_,size,rawptr,relptr,_,nrel,_,_=struct.unpack_from('<8sIIIIIIHHI',data,20+(sec-1)*40)
  if section_name==b'.CRT$XCU': crtsections.append((size,rawptr,relptr,nrel))
 assert len(crtsections)==1
 size,rawptr,relptr,nrel=crtsections[0]; assert size>=12
 refs=[struct.unpack_from('<IIH',data,relptr+10*i) for i in range(nrel)]
 for v in proof['vectors']:
  offset=4*v['crt_index_in_object']; assert data[rawptr+offset:rawptr+offset+4]==bytes(4)
  assert [m['object_symbol'](data,i)[0] for o,i,k in refs if o==offset and k==2]==[v['initializer']]
 a=address('__xc_a');z=address('__xc_z');assert a<z and (z-a)%4==0;crt=load(a,z-a+4);vectors=[]; initaddrs=[]
 for v in proof['vectors']:
  init=v['initializer'];c,r,sec=objfn(obj,init);oc,orr,_=objfn(ref,init);assert len(c)==v['ppc_bytes'] and c[4:8]==bytes.fromhex('39400000') and c[0x14:0x1c]==bytes.fromhex('9141fff89141fffc');m['fixed_equal'](oc,c,orr);vaddr=address(init);lc=load(vaddr,len(c));m['fixed_equal'](c,lc,r)
  hi,lo=map(lambda x:int(x,16),v['address_pair']); actual=target(linked,hi,lo);assert target(lc,0x20,0x2c)==actual
  for h,l,want in [(0,8,bytes.fromhex(v['initialized_bytes'][:8])),(12,28,bytes.fromhex(v['initialized_bytes'][8:16]))]:assert load(target(lc,h,l),4)==want
  assert bytes.fromhex(v['initialized_bytes'][16:])==bytes(8)
  initaddrs.append(vaddr);vectors.append({'name':v['name'],'initializer_va':hex(vaddr),'storage_va':hex(actual),'initialized_bytes':v['initialized_bytes'],'initial_value_source':'linked initializer rdata and zero stores'})
 slots=[a+i for i in range(0,len(crt)-7,4) if struct.unpack_from('>II',crt,i)==tuple(initaddrs)]; assert len(slots)==1,slots
 return {'state':'pass','head':head,'tree':tree,'parent':parent,'seal_sha256':seal,'xex_sha256':xex,'pe_sha256':pe,'map_sha256':maphash,'selected_object_sha256':sha(obj),'reference_object_sha256':sha(ref),'prepare_va':hex(va),'world_map_construct_va':hex(calltarget),'vectors':vectors,'crt_origin_slot':hex(slots[0]),'crt_size_slot':hex(slots[0]+4),'scope':'selected COFF and linked PE/static CRT only; no execution proof'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('link',type=Path);p.add_argument('--head',required=True);p.add_argument('--seal',required=True);p.add_argument('--xex',required=True);p.add_argument('--pe',required=True);p.add_argument('--map',required=True);args=p.parse_args(); result=prove(args.link.resolve(),args.head,args.seal,args.xex,args.pe,args.map); out=Path(__file__).parent/'R163_LINKED_MAP.json'
 with out.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
 print(out,sha(out))
