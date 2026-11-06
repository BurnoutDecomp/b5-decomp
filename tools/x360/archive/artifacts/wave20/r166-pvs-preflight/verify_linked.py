"""R166 PVS Prepare/Release linked PPC witnesses; no execution claim."""
import argparse,hashlib,json,pathlib,runpy,struct,subprocess
P=pathlib.Path; wave=P('D:/Reverse/BP-X360-Experimental-Artifacts/wave20');folder=wave/'r166-link';batch=wave/'r166-batch';root=P('C:/Users/adri1/.codex/worktrees/bp360/native_lion');m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m'];m['fixed_equal'].__globals__['masks'][2]=0xffffffff
if not __debug__:raise RuntimeError('assertions required')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text(encoding='utf8'));tree='77910cc622803ecd685ea93afca520619840d51c';parent='451d95889337e064441dec06a1726179187e48cf';candidate='9a99e86aedecb6b7933fe8f56fa74feda3986031';source='src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.cpp';sourcefile=r'd:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.cpp'
def prove(pinsha):
 pins=folder/'ROOT_LINK_PINS.json';assert sha(pins)==pinsha;pin=read(pins);head=pin['head'];assert pin['tree']==tree and pin['parent']==parent
 for name in ('completion','seal','invocation','result','pe','map','xex','image','diagnostics'):
  x=pin['pins'][name];assert sha(P(x['path']))==x['sha256'],name
 git=lambda *a:subprocess.check_output(['git','-C',str(root),*a]).decode().strip();assert git('rev-parse',head+'^{tree}')==tree and git('rev-parse',head+'^')==parent and git('rev-parse',candidate+'^{tree}')==tree
 comp=read(batch/'COMPOSED_PPC_CHECK.json');binding=read(batch/'TARGETED_OBJECT_BINDING.json');assert comp['head']==candidate and comp['tree']==tree and sha(batch/'TARGETED_OBJECT_BINDING.json')==comp['targeted_binding_sha256'];assert comp['pvs']['artist_assert_lines']==[155,200,257] and comp['pvs']['release_compare_word']=='0x2b0b0002'
 donor=wave/'r166-pvs-preflight/PROOF.json';assert sha(donor)=='f5eccb46b1372a8069a44cc93fdb0627ac1e6fe3782ddc1756248198b93962dc'
 result=read(folder/'result.json');image=read(folder/'image-verification.json');inventory=read(folder/'source-inventory.json');inv=read(folder/'invocation.json');assert inventory['head']==head and result['exit_code']==0 and result['all_post_checks_pass'] and image['head']==head and image['magic']=='XEX2' and image['image_sha256']==pin['pins']['xex']['sha256']
 item,=[x for x in inv['objects'] if x['source']==source];obj=P(item['object']);assert sha(obj)==item['sha256'];b,=[x for x in binding['results'] if x['source']==source];ref=P(b['artifacts']['object']['path']);assert sha(ref)==b['artifacts']['object']['sha256'] and git('rev-parse',head+':'+source)==b['git_blob']
 row,=[x for x in inventory['files'] if x['path'].replace('\\','/').lower().endswith(source.lower())];actual=P(row['path']);assert sha(actual)==row['sha256'] and actual.read_bytes().replace(b'\r\n',b'\n')==subprocess.check_output(['git','-C',str(root),'show',head+':'+source])
 address,load=m['linked_reader'](folder); funcs={}
 for n in ('Prepare','Release'):
  symbol='?'+n+'@PVSModule@BrnWorld@@UAA_NXZ';c,rels=m['collect'](obj,symbol);r,rr=m['collect'](ref,symbol);m['fixed_equal'](r,c,rr);va=address(symbol);linked=load(va,len(c));m['fixed_equal'](c,linked,rels)
  for off,i,k in rels:
   if k==2:
    name,value,sec=m['object_symbol'](obj.read_bytes(),i);assert name.startswith('$LN') and 0<=value<len(c);assert struct.unpack_from('>I',linked,off)[0]==va+value+struct.unpack_from('>I',c,off)[0]
  funcs[n]=(c,rels,va,linked)
 def edge(n,off,needle):
  c,r,va,linked=funcs[n];i,=[i for o,i,k in r if o==off and k==6];name=m['object_symbol'](obj.read_bytes(),i)[0];assert needle in name,(n,off,name);target=m['branch_target'](va+off,struct.unpack_from('>I',linked,off)[0]);assert target==address(name);return hex(target)
 def literal(n,hi,lo,want):
  c,r,va,linked=funcs[n];i,=[i for o,i,k in r if o==hi and k==16];j,=[i for o,i,k in r if o==lo and k==17];assert i==j and m['symbol_literal'](obj.read_bytes(),i)==want;target=m['data_target'](linked,hi,lo)&0xffffffff;assert load(target,len(want.encode())+1)==want.encode()+b'\0';return hex(target)
 a=funcs['Prepare'][1];b=funcs['Release'][1]
 assert funcs['Prepare'][0][0x16c:0x170]==bytes.fromhex('38a0009b') and funcs['Prepare'][0][0x200:0x204]==bytes.fromhex('38a000c8')
 release=funcs['Release'][0];assert release[0x20:0x28]==bytes.fromhex('2b0b000240980020') and release[0x44:0x48]==bytes.fromhex('4099002c') and release[0x50:0x54]==bytes.fromhex('38a00101') and release[0x68:0x70]==bytes.fromhex('3860000048000014')
 literals={'prepare_path':literal('Prepare',0x140,0x144,sourcefile),'invalid_event':literal('Prepare',0x138,0x13c,'Invalid event received\n'),'prepare_stage':literal('Prepare',0x208,0x20c,'Invalid Stage\n'),'release_path':literal('Release',0x4c,0x54,sourcefile),'release_stage':literal('Release',0x58,0x5c,'Invalid Stage\n')};assert literal('Prepare',0x1fc,0x204,sourcefile)==literals['prepare_path']
 edges={}
 for n,off,target in [('Prepare',0xa4,'?Prepare@ModuleSingleBuffered'),('Prepare',0xf0,'?LoadPVS@'),('Prepare',0xf8,'?Clear@BaseEventReceiverQueue'),('Prepare',0x128,'?GetFirstEvent@BaseEventReceiverQueue'),('Prepare',0x160,'?CreateFromHandle@BaseResourcePtr'),('Prepare',0x190,'?GetNextEvent@BaseEventReceiverQueue'),('Prepare',0x1a4,'?Clear@BaseEventReceiverQueue'),('Prepare',0x1ac,'?LockOutputForWrite@ModuleSingleBuffered'),('Prepare',0x1c4,'??C?$ResourcePtr@VZoneList'),('Prepare',0x1d8,'?UnlockOutputForWrite@ModuleSingleBuffered'),('Prepare',0x1e8,'?UnlockOutputForWrite@ModuleSingleBuffered'),('Prepare',0x178,'?FireAssert@Assert'),('Prepare',0x210,'?FireAssert@Assert'),('Release',0x2c,'?Release@ModuleSingleBuffered'),('Release',0x60,'?FireAssert@Assert')]:edges[n+'+'+hex(off)]=edge(n,off,target)
 return {'state':'pass','head':head,'tree':tree,'parent':parent,'root_pins_sha256':pinsha,'selected_object_sha256':sha(obj),'composed_object_sha256':sha(ref),'donor_proof_sha256':sha(donor),'prepare_va':hex(funcs['Prepare'][2]),'release_va':hex(funcs['Release'][2]),'edges':edges,'assertion_literals':literals,'assertion_lines':[155,200,257],'release_unsigned_compare':True,'scope':'selected PPC COFF and linked PE; no runtime proof'}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--root-pins-sha',required=True);a=p.parse_args();r=prove(a.root_pins_sha);out=P(__file__).parent/'R166_PVS_LINKED.json'
 with out.open('x',encoding='utf8') as f:json.dump(r,f,indent=2);f.write('\n')
 print(out,sha(out))
