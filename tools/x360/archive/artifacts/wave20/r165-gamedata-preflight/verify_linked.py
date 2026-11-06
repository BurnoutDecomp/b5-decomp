"""R165 GameData selected COFF to PE witnesses. No execution claim."""
import argparse,hashlib,json,re,runpy,struct,subprocess
from pathlib import Path
if not __debug__: raise RuntimeError('assertions required')
wave=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20');folder=wave/'r165-link';batch=wave/'r165-batch';root=Path('C:/Users/adri1/.codex/worktrees/bp360/native-sound');m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m'];m['fixed_equal'].__globals__['masks'][2]=0xffffffff
head='451d95889337e064441dec06a1726179187e48cf';tree='cdfbeb9f10e3b32c23e42da60c5ea72a5dcde843';parent='d9d487ef61d4bab2e80d9e4c2fc64c20a600b08a';source='src/GameSource/Resource/BrnGameDataModule.cpp';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text(encoding='utf8'))
def call_name(obj,rels,off):
 ix,=[i for o,i,k in rels if o==off and k==6];return m['object_symbol'](obj.read_bytes(),ix)[0]
def sym(obj,rels,off,kind):
 ix,=[i for o,i,k in rels if o==off and k==kind];return m['object_symbol'](obj.read_bytes(),ix)
def prove(pinsha):
 pins=folder/'ROOT_LINK_PINS.json';assert sha(pins)==pinsha; pin=read(pins);assert (pin['head'],pin['tree'],pin['parent'])==(head,tree,parent)
 for name in ('completion','seal','invocation','result','pe','map','xex','image','diagnostics'):
  item=pin['pins'][name];assert sha(Path(item['path']))==item['sha256'],name
 git=lambda *a:subprocess.check_output(['git','-C',str(root),*a]).decode().strip();assert git('rev-parse',head+'^{tree}')==tree and git('rev-parse',head+'^')==parent
 assert sha(batch/'COMPOSED_PPC_CHECK.json')=='878a05149a6c38bf0d402157930193beb304b97bca9751c9b78843b949c35d3b'
 comp=read(batch/'COMPOSED_PPC_CHECK.json');binding=read(batch/'TARGETED_OBJECT_BINDING.json'); assert comp['tree']==tree and sha(batch/'TARGETED_OBJECT_BINDING.json')==comp['targeted_binding_sha256'];donor=Path(comp['game_data']['original_proof']['path']);assert sha(donor)==comp['game_data']['original_proof']['sha256'] and read(donor)['state']=='pass'
 inv=read(folder/'invocation.json');result=read(folder/'result.json');image=read(folder/'image-verification.json');inventory=read(folder/'source-inventory.json');assert inventory['head']==head and result['exit_code']==0 and result['all_post_checks_pass'] and image['head']==head and image['magic']=='XEX2' and image['image_sha256']==pin['pins']['xex']['sha256']
 item,=[x for x in inv['objects'] if x['source']==source];obj=Path(item['object']);assert sha(obj)==item['sha256'];r,=[x for x in binding['results'] if x['source']==source];ref=Path(r['artifacts']['object']['path']);assert sha(ref)==r['artifacts']['object']['sha256'] and git('rev-parse',head+':'+source)==r['git_blob']
 rows=[x for x in inventory['files'] if x['path'].replace('\\','/').lower().endswith(source.lower())];row,=rows;assert sha(Path(row['path']))==row['sha256'];raw=subprocess.check_output(['git','-C',str(root),'show',head+':'+source]);assert Path(row['path']).read_bytes().replace(b'\r\n',b'\n')==raw
 header='src/GameSource/Resource/BrnGameDataModule.h';assert git('rev-parse',head+':'+header)=='5fcae7e5226ff166404dc048942d00995f5d505b'; hrow,=[x for x in inventory['files'] if x['path'].replace('\\','/').lower().endswith(header.lower())]; hfile=Path(hrow['path']);assert sha(hfile)==hrow['sha256'] and hfile.read_bytes().replace(b'\r\n',b'\n')==subprocess.check_output(['git','-C',str(root),'show',head+':'+header])
 assert b'DeferredGameDataRequest' not in obj.read_bytes() and b'[GameData] request handler DEFERRED' not in obj.read_bytes();assert b'[GameData] request handler DEFERRED' not in (folder/'native-game.pe').read_bytes();mapping=(folder/'native-game.map').read_text(errors='replace');assert 'DeferredGameDataRequest' not in mapping
 address,load=m['linked_reader'](folder); asm=Path(r['artifacts']['assembly']['path']).read_text(errors='replace');names=list(comp['game_data']['handlers'])+['ProcessGetGameDataEvent','ProcessUnloadGameDataEvent','ProcessInternalAcquireResponse','ProcessInternalUnloadResponse','PostGameDataResponse']; functions={}
 for n in names:
  match=re.search(r'; Begin code for function: (\?'+n+r'@GameDataModule@BrnResource@@\S+)',asm);assert match,n;symbol=match.group(1);code,rels=m['collect'](obj,symbol);old,orr=m['collect'](ref,symbol);m['fixed_equal'](old,code,orr);va=address(symbol);linked=load(va,len(code));m['fixed_equal'](code,linked,rels)
  for off,i,k in rels:
   if k==2:
    label,value,sec=m['object_symbol'](obj.read_bytes(),i);assert label.startswith('$LN') and 0<=value<len(code);assert struct.unpack_from('>I',linked,off)[0]==va+value+struct.unpack_from('>I',code,off)[0]
  functions[n]=(symbol,code,rels,va,linked)
 def edge(func,off,wanted):
  symbol,code,rel,va,linked=functions[func];name=call_name(obj,rel,off);assert wanted in name,(func,hex(off),name);dest=m['branch_target'](va+off,struct.unpack_from('>I',linked,off)[0]);assert dest==address(name),(name,hex(dest));return hex(dest)
 edges={}
 for n,v in comp['game_data']['handlers'].items():
  dispatch='ProcessGetGameDataEvent' if n=='ProcessGetICEMovieRequest' else 'ProcessUnloadGameDataEvent'; off=int(v['dispatch_offset'],16);edges[n]=edge(dispatch,off,n);assert int(edges[n],16)==functions[n][3]
 # Resolve essential provider calls used to construct and submit each request.
 for n in comp['game_data']['handlers']:
  rel=functions[n][2]
  required=['AddEvent@?$VariableEventQueue','IndexedPool']
  required+=['CgsIDConvertToString','HashString@ID@CgsResource'] if n=='ProcessGetICEMovieRequest' else ['SetFileName@BundleLoaderEvent']
  if n=='ProcessUnloadPropInstancesRequest': required+=['CgsIDConvertToString','strncmp','atoi']
  for wanted in required:
   matches=[o for o,i,k in rel if k==6 and wanted in m['object_symbol'](obj.read_bytes(),i)[0]]
   assert len(matches)==1,(n,wanted,matches)
   edges[n+':'+wanted]=edge(n,matches[0],wanted)
 # These unrelocated immediates and slot stores match the composed and donor checked object.
 for n in comp['game_data']['handlers']:
  c=functions[n][1]; assert (bytes.fromhex('38a00004 38c00018') if n=='ProcessGetICEMovieRequest' else bytes.fromhex('38a00003 38c00090')) in c
 for n,lit in [('ProcessUnloadTrafficLanesRequest','B5Traffic.bndl'),('ProcessUnloadAILanesRequest','AI.dat'),('ProcessUnloadPropInstancesRequest','Props/Instances/TRK_UNIT')]:
  _,code,rels,va,linked=functions[n]; matches=[]
  for off,i,k in rels:
   if k!=16:continue
   data=obj.read_bytes(); name,_,_=m['object_symbol'](data,i)
   if not name.startswith('??_C@') or m['symbol_literal'](data,i)!=lit:continue
   lows=[o for o,j,t in rels if j==i and t==17 and o>off];assert len(lows)==1
   ptr=m['data_target'](linked,off,lows[0])&0xffffffff;assert load(ptr,len(lit)+1)==lit.encode()+b'\0';matches.append(hex(ptr))
  assert len(matches)==1,(n,lit,matches)
 # Acquire response case 65: switch base 40, slot 25 -> shared case at +0x2a8.
 ac=functions['ProcessInternalAcquireResponse']; assert ac[1][0x98:0xa4]==bytes.fromhex('80bf00283965ffd82b0b001a');assert sym(obj,ac[2],0x124,2)[1]==0x2a8;assert struct.unpack_from('>I',ac[4],0x124)[0]==ac[3]+0x2a8
 edges['acquire_to_post']=edge('ProcessInternalAcquireResponse',0x2c0,'PostGameDataResponse');edges['acquire_slot_recycle']=edge('ProcessInternalAcquireResponse',0x2d8,'PushIndex')
 post=functions['PostGameDataResponse'];assert post[1][0x20:0x2c]==bytes.fromhex('3944ffe57ce63b782b0a0027');assert sym(obj,post[2],0xe0,2)[1]==0x304;assert struct.unpack_from('>I',post[4],0xe0)[0]==post[3]+0x304
 edges['ice_reply_helper']=edge('PostGameDataResponse',0x314,'SendHandleResponse@UGetICEMovieResponse')
 # Generic unload reply posts the staged response id then returns the slot.
 u=functions['ProcessInternalUnloadResponse'];assert u[1][0xbc:0xc4]==bytes.fromhex('2f0b002d409a0034');edges['unload_post']=edge('ProcessInternalUnloadResponse',0x1a4,'AddEvent@BaseEventReceiverQueue');edges['unload_slot_recycle']=edge('ProcessInternalUnloadResponse',0x1bc,'PushIndex')
 return {'state':'pass','head':head,'tree':tree,'parent':parent,'root_pins_sha256':pinsha,'selected_object_sha256':sha(obj),'composed_object_sha256':sha(ref),'donor_proof_sha256':sha(donor),'functions':{n:{'va':hex(x[3]),'bytes':len(x[1])} for n,x in functions.items()},'edges':edges,'scope':'PPC selected COFF and linked PE; case 65 and generic unload static continuation; not execution proof'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--root-pins-sha',required=True);a=p.parse_args();r=prove(a.root_pins_sha);out=Path(__file__).parent/'R165_LINKED_GAMEDATA.json'
 with out.open('x',encoding='utf8') as f:json.dump(r,f,indent=2);f.write('\n')
 print(out,sha(out))
