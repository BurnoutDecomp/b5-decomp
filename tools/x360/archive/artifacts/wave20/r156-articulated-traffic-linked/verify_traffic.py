import argparse, hashlib, json, runpy, struct, subprocess
from pathlib import Path
W=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20')
H=runpy.run_path(str(W/'r148-boot-presentation-linked/verify_linked.py'))
P=W/'r156-composed-preflight/compile-results.json'
HEAD='9708f3b6910954ea62a0a118405b4f9b23c163c6'; TREE='f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a'
B='src/GameSource/World/EntityModules/TrafficEntityModule/'
S='src/GameSource/Physics/VehicleManager/'
groups={'src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.cpp':['?GetLocatorTy@LocatorPointSpecList@','?GetLocatorXf@LocatorPointSpecList@'],B+'BrnTrafficEntityModule.cpp':['?TryAllocateTrailerId@','?UpdateVehicles_CreateNewVehicles@','?UpdateDecisionFrame@','?UpdateNonDecisionFrame@','?UpdateTrailers@','?UpdateTrailers_UpdatePurgatory@','?UpdateTrailers_MoveToCab@','?AddVehicleToPhysics@','?HandleExternalResponses@','?SendEmergencyCrashEvents@'],B+'BrnTrafficVehicle.cpp':['?InitialiseAsTrailer@'],B+'BrnTrafficVehicleTypeRuntime_Prepare.cpp':['?Prepare@VehicleTypeRuntime@'],B+'BrnTrafficEntityModule_KillDyingVehicleEntities.cpp':['?KillDyingVehicleEntity@'],S+'SharedIO/BrnVehicleInputInterface.cpp':['?CreateArticulatedTraffic@']}
checks={'?TryAllocateTrailerId@':{0x6c:'?Peek@?',0x78:'?GetVehicleSpecies@',0x13c:'?Pop@?'},'?UpdateDecisionFrame@':{0x198:'?UpdateTrailers@'},'?UpdateNonDecisionFrame@':{0xe0:'?UpdateTrailers@'},'?UpdateTrailers@':{0x10:'?UpdateTrailers_UpdatePurgatory@',0x8c:'?UpdateTrailers_MoveToCab@'},'?UpdateVehicles_CreateNewVehicles@':{0x5c8:'?TryAllocateTrailerId@',0x838:'?InitialiseAsTrailer@'},'?HandleExternalResponses@':{0x3dc:'?Append@?'},'?Prepare@VehicleTypeRuntime@':{0x21c:'?GetLocatorTy@',0x23c:'?GetLocatorXf@'},'?KillDyingVehicleEntity@':{0x16c:'?SetOrphan@',0x228:'?DetachArticulation@',0x238:'?DetachArticulation@',0x2c0:'?StopVehicleBeingPhysical@'},'?AddVehicleToPhysics@':{0x180:'?CalculateInitialPhysicalState@',0x1c0:'?CalculateInitialPhysicalState@',0x394:'?CreateArticulatedTraffic@'}}
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
p=argparse.ArgumentParser();p.add_argument('link',type=Path);p.add_argument('--head',required=True);p.add_argument('--seal-sha',required=True);p.add_argument('--pe-sha',required=True);p.add_argument('--map-sha',required=True);p.add_argument('--xex-sha',required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args(); L=a.link
for f,d in [('seal.json',a.seal_sha),('native-game.pe',a.pe_sha),('native-game.map',a.map_sha),('native-game.exe',a.xex_sha)]:assert sha(L/f)==d.lower(),f
seal=load(L/'seal.json'); inv=load(L/'invocation.json');res=load(L/'result.json'); image=load(L/'image-verification.json')
assert seal['head']==image['head']==a.head and res['exit_code']==0 and res['all_post_checks_pass'] and image['magic']=='XEX2'
assert inv['sealed_sha256'].lower()==a.seal_sha.lower() and inv['objects']==seal['selected'] and inv['sources']==seal['sources']
root=Path(seal['source_root']); assert subprocess.check_output(['git','-C',str(root),'rev-parse',a.head+'^{tree}'],text=True).strip()==TREE
assert subprocess.check_output(['git','-C',str(root),'rev-parse',HEAD+'^{tree}'],text=True).strip()==TREE
H['load_image'].__globals__['LINK']=L
read,entries,pins=H['load_image'](res,image)
rows={x['source']:x for x in inv['objects']}; pre={x['source']:x for x in load(P)['results']}; pre.update({x['source']:x for x in load(W/'r156-native-hitch-pivots/compile-results.json')['results'] if 'BrnStreamedDeformationSpec.cpp' in x['source']}); pre.update({x['source']:x for x in load(W/'r156-allocator-v2-preflight/compile-results.json')['results']}); out={}
for source,prefixes in groups.items():
 row=rows[source]; prior=pre[source]; obj=Path(row['object']); assert sha(obj)==row['sha256']; assert prior['passed'] and prior['machine']==498
 selected=H['Coff'](obj.read_bytes()); baseline=H['Coff'](Path(prior['object']).read_bytes())
 blob=subprocess.check_output(['git','-C',str(root),'rev-parse',a.head+':'+source],text=True).strip()
 source_pins=[x for x in seal['sources'] if x['path'].replace('\\','/').lower().endswith('/'+source.lower())]; assert len(source_pins)==1 and sha(source_pins[0]['path'])==source_pins[0]['sha256']
 assert subprocess.check_output(['git','-C',str(root),'hash-object',source_pins[0]['path']],text=True).strip()==blob
 for prefix in prefixes:
  symbol,code,refs=selected.function(prefix);old,previous,orefs=baseline.function(prefix);assert symbol.name==old.name and code==previous
  for off,target in checks.get(prefix,{}).items(): assert refs[off][0]==6 and refs[off][1].name.startswith(target),(prefix,hex(off))
  if prefix=='?TryAllocateTrailerId@':
   for off,word in {0x50:0x2f0b0000,0x58:0x3c600000,0x5c:0x6063ffff,0x70:0xa3c30000,0x80:0x2f030002,0x10c:0x896b2a85,0x110:0x556b07fe}.items(): assert H['u32'](code,off)==word,(prefix,hex(off))
  address=H['map_addr'](entries,symbol.name,obj.name); actual=read(address,len(code),executable=True); masks={6:0x03fffffc,0x10:0xffff,0x11:0xffff,2:0xffffffff}
  for off in range(0,len(code),4):
   x=H['u32'](code,off);y=H['u32'](actual,off); kind=refs[off][0] if off in refs else None
   assert not ((x^y)&~masks.get(kind,0)),(prefix,hex(off))
  targets={}
  for off in checks.get(prefix,{}):
   target=refs[off][1].name; dest=H['map_addr'](entries,target); op=H['u32'](actual,off); assert off+address+H['sign'](op&0x03fffffc,26)==dest
   targets[hex(off)]={'symbol':target,'address':hex(dest)}
  out[prefix]={'symbol':symbol.name,'address':hex(address),'length':len(code),'linked_sha256':hashlib.sha256(actual).hexdigest(),'source':source,'selected_object_sha256':row['sha256'],'preflight_object_sha256':sha(prior['object']),'checked_call_targets':targets}
import sys; sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
for pc,word in {0x82723510:0xa3830000,0x82723520:0x2f030002,0x827235e8:0x3c600000,0x827235ec:0x6063ffff}.items(): assert x360rd.u32(pc)==word,(hex(pc),hex(x360rd.u32(pc)))
artist_calls={0x82723518:0x821f4648,0x827235a4:0x82707b58,0x827235d8:0x8271ae88,0x8274e644:0x8273b168,0x8274c248:0x8273b168,0x8273abd4:0x827234b0,0x8273ae08:0x8275fb10,0x82742b34:0x8271c9c0}
for pc,dest in artist_calls.items():
 op=x360rd.u32(pc); assert (op & 0xfc000003)==0x48000001 and pc+H['sign'](op&0x03fffffc,26)==dest
result={'artist_call_targets':{hex(k):hex(v) for k,v in artist_calls.items()},'head':a.head,'tree':TREE,'candidate':HEAD,'preflight_sha256':sha(P),'allocator_v2_preflight_sha256':sha(W/'r156-allocator-v2-preflight/compile-results.json'),'hitch_preflight_sha256':sha(W/'r156-native-hitch-pivots/compile-results.json'),'seal_sha256':a.seal_sha,'image':pins,'functions':out,'scope':'Static code and selected call bindings only. Other relocations masked; no runtime proof.'}
assert not a.report.exists();a.report.write_text(json.dumps(result,indent=2)+'\n');print(sha(a.report))












