"""R155 traffic layout and light behavior bound to a sealed PPC link."""
import argparse, hashlib, json, runpy, subprocess, struct
from pathlib import Path
W=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20'); OUT=Path(__file__).resolve().parent
HEAD_CANDIDATE='68208920c4ac83d0759649576cb2d0516cb57bf8'; TREE='6bdcd7954fa913359b809b7506984f0dc05d90ee'
BINDING=W/'r155-batch/TARGETED_OBJECT_BINDING.json'; BINDING_SHA='a52fee9e789efa7ae7fe18c7c6e4e83bd227c6f3346fe8ef0632c87972f745db'
READER=W/'r148-boot-presentation-linked/verify_linked.py'; FRONT=W/'r149-frontend-linked/verify_linked.py'
READER_SHA='434c49d6c2f58252bd634051f259fd267184fce7d8568e01b55cafc1a53e0fba'
FRONT_SHA='bacec188cc23a9472e70f886b7629d6d53e7adff6f5589082c91f1182e9d69f9'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
p=argparse.ArgumentParser();p.add_argument('link',type=Path);p.add_argument('--head',required=True);p.add_argument('--seal-sha',required=True);p.add_argument('--xex-sha',required=True);p.add_argument('--pe-sha',required=True);p.add_argument('--map-sha',required=True);p.add_argument('--report',type=Path,required=True);args=p.parse_args(); link=args.link.resolve()
assert sha(BINDING)==BINDING_SHA and sha(READER)==READER_SHA and sha(FRONT)==FRONT_SHA
binding=load(BINDING);assert (binding['head'],binding['tree'])==(HEAD_CANDIDATE,TREE)
assert sha(binding['compile_report']['path'])==binding['compile_report']['sha256']
for name,digest in [('seal.json',args.seal_sha),('native-game.exe',args.xex_sha),('native-game.pe',args.pe_sha),('native-game.map',args.map_sha)]: assert sha(link/name)==digest.lower()
seal=load(link/'seal.json');inv=load(link/'invocation.json');res=load(link/'result.json');image=load(link/'image-verification.json');release=load(link/'ROOT_RELEASE.json')
assert seal['head']==image['head']==args.head and res['exit_code']==0 and res['all_post_checks_pass'] and image['magic']=='XEX2'
assert inv['sealed_sha256'].lower()==args.seal_sha.lower() and sha(link/'ROOT_RELEASE.json')==inv['release_sha256'].lower() and release['sealed_sha256'].lower()==args.seal_sha.lower()
assert inv['objects']==seal['selected'] and inv['sources']==seal['sources']
root=Path(seal['source_root']); tree=subprocess.check_output(['git','rev-parse',args.head+'^{tree}'],cwd=root,text=True).strip();assert tree==TREE
assert subprocess.check_output(['git','rev-parse',HEAD_CANDIDATE+'^{tree}'],cwd=root,text=True).strip()==TREE
h=runpy.run_path(str(READER));front=runpy.run_path(str(FRONT));h['load_image'].__globals__['LINK']=link
read,entries,image_pins=h['load_image'](res,image); rows={x['source']:x for x in inv['objects']}; bound={x['source']:x for x in binding['results']}
base='src/GameSource/World/EntityModules/TrafficEntityModule/'
groups={
 base+'BrnTrafficEntityModule.cpp':['?Construct@TrafficEntityModule@BrnTraffic@@','?UpdateTimers@TrafficEntityModule@BrnTraffic@@','?ManageTriggers@TrafficEntityModule@BrnTraffic@@','?PrePhysicsUpdate@TrafficEntityModule@BrnTraffic@@','?RecalculateActiveHulls@TrafficEntityModule@BrnTraffic@@','?AddVehicleToPhysics@TrafficEntityModule@BrnTraffic@@','?LoadData@TrafficEntityModule@BrnTraffic@@'],
 base+'BrnTrafficDebugComponent.cpp':['?OnActivate@DebugComponent@BrnTraffic@@'],
 base+'BrnTrafficLightManager.cpp':['?GetLightState@TrafficLightManager@BrnTraffic@@QA','?TrafficLightGotSmashed@TrafficLightManager@','?TrafficLightGotRestored@TrafficLightManager@','?Construct@TrafficLightManager@','?SetCountdownValue@TrafficLightManager@','?ChangeLightState@TrafficLightManager@','?Update@TrafficLightManager@','?RenderLightsForHull@TrafficLightManager@','?RenderAllLightsToBeInStateForHull@TrafficLightManager@'],
 base+'BrnTrafficLightRuntimeState.cpp':['?Update@TrafficLightRuntimeState@','?UpdateHull@TrafficLightManager@']}
checks={ '?RenderLightsForHull@TrafficLightManager@':{0x140:0x897e0005},'?RenderAllLightsToBeInStateForHull@TrafficLightManager@':{0x104:0x897e0005},'?OnActivate@DebugComponent@BrnTraffic@@':{0xe10:0x3c8a0007,0xe18:0x388429f0},'?Construct@TrafficEntityModule@BrnTraffic@@':{0x3f4:0x38632b58,0x7e4:0x612829f0,0x8c8:0x3b3929fc},'?ManageTriggers@TrafficEntityModule@BrnTraffic@@':{0x144:0x3b3929f0,0x304:0x3b5a29f0},'?PrePhysicsUpdate@TrafficEntityModule@BrnTraffic@@':{0x1c:0x3b7b29fc},'?AddVehicleToPhysics@TrafficEntityModule@BrnTraffic@@':{0x200:0x38845b58},'?LoadData@TrafficEntityModule@BrnTraffic@@':{0x19c:0x38632b58,0x31c:0x615b5b58},'?Construct@TrafficLightManager@':{0x004:0x39630004,0x008:0x39400258,0x038:0xd00312c8,0x03c:0x992312c0,0x040:0x916312c4},'?TrafficLightGotSmashed@TrafficLightManager@':{0x08c:0x897f0005,0x090:0x616b0080,0x094:0x997f0005},'?TrafficLightGotRestored@TrafficLightManager@':{0x08c:0x897f0005,0x094:0x997f0005},'?Update@TrafficLightManager@':{0x000:0x896312c0,0x00c:0x816312c4,0x018:0xc00312c8,0x024:0xd00312c8},'?Update@TrafficLightRuntimeState@':{0x058:0x897f0004,0x078:0x897f0005,0x08c:0x993f0004,0x090:0x997f0005}}
proofs={};owner_pins={};helpers={}
for source,prefixes in groups.items():
 row=rows[source]; frozen=bound[source]; obj=Path(row['object']); assert sha(obj)==row['sha256']
 for artifact in frozen['artifacts'].values():assert sha(artifact['path'])==artifact['sha256']
 blob=subprocess.check_output(['git','rev-parse',args.head+':'+source],cwd=root,text=True).strip();assert blob==frozen['git_blob']
 src=[x for x in seal['sources'] if x['path'].replace('\\','/').lower().endswith('/'+source.lower())];assert len(src)==1 and sha(src[0]['path'])==src[0]['sha256']==frozen['source_sha256']
 current=h['Coff'](obj.read_bytes());old=h['Coff'](Path(frozen['artifacts']['object']['path']).read_bytes()); owner_pins[source]={'blob':blob,'selected':row['sha256'],'frozen':frozen['artifacts']['object']['sha256']}
 for prefix in prefixes:
  symbol,code,refs=current.function(prefix);osym,ocode,orefs=old.function(prefix);assert symbol.name==osym.name and code==ocode,prefix
  actual=h['ref_signature'](symbol,refs); expected=h['ref_signature'](osym,orefs); assert actual.keys()==expected.keys()
  differences={k for k in actual if actual[k]!=expected[k]}
  path_sites={
   '?ManageTriggers@TrafficEntityModule@BrnTraffic@@':{0x5c:'GameShared/GameClasses/Module/CgsBaseEventQueue.h',0x64:'GameShared/GameClasses/Module/CgsBaseEventQueue.h',0x80:'GameShared/GameClasses/Containers/CgsArray.h',0x84:'GameShared/GameClasses/Containers/CgsArray.h'},
   '?RecalculateActiveHulls@TrafficEntityModule@BrnTraffic@@':{0x1c0:'GameShared/GameClasses/Containers/CgsArray.h',0x1c4:'GameShared/GameClasses/Containers/CgsArray.h',0x2e4:'GameShared/GameClasses/Containers/CgsSet.h',0x2e8:'GameShared/GameClasses/Containers/CgsSet.h',0x860:'SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h',0x868:'SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h'},
   '?AddVehicleToPhysics@TrafficEntityModule@BrnTraffic@@':{0x158:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h',0x15c:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h'},
   '?LoadData@TrafficEntityModule@BrnTraffic@@':{0x308:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h',0x30c:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h'},
   '?OnActivate@DebugComponent@BrnTraffic@@':{0x8d0:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h',0x8d4:'GameShared/GameClasses/System/Resource/CgsResourcePtr.h'},
   '?RenderLightsForHull@TrafficLightManager@':{0xf0:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h',0xf4:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h',0x1ac:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h',0x1b0:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h'},
   '?RenderAllLightsToBeInStateForHull@TrafficLightManager@':{0xb4:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h',0xb8:'GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h'}}.get(prefix,{})
  assert differences==set(path_sites),(prefix,differences,path_sites)
  for off,suffix in path_sites.items():
   for coff,ref,checkout in ((current,refs[off],'wave4-intake'),(old,orefs[off],'debug_format_luna')):
    kind,target=ref;assert kind in (0x10,0x11) and kind==refs[off][0]==orefs[off][0]
    _,data,_,_=coff.section(target.section); literal=data[target.value:data.index(b"\0",target.value)+1].replace(b"\\",b"/")
    assert literal.endswith(('/'+checkout+'/src/'+suffix).encode()+b"\0"),(prefix,off,literal)

  address=h['map_addr'](entries,symbol.name,obj.name)
  method=prefix; front['body_proof'].__globals__['PRIVATE_DATA'][method]=('__real@','KF_STEERING_SCALE_THRESHOLD','KF_COUNTDOWN_RED_TIME','KF_AMBER_TIME','KA_AIR_RAM_TYPE_OPTIONS','?kauDEBUGRunningWorstCaseHulls@')
  result=front['body_proof'](read,entries,current,method,symbol,code,refs,address,helpers,h)
  for off,word in checks.get(prefix,{}).items():assert h['u32'](read(address+off,4,executable=True),0)==word,(prefix,hex(off))
  result['fixed_checks']={hex(k):hex(v) for k,v in checks.get(prefix,{}).items()};result['assert_path_relocations']={hex(k):v for k,v in path_sites.items()};proofs[prefix]=result
# Q7 diagnostics and removed host tracing must not appear in selected light owners.
for source in (base+'BrnTrafficLightManager.cpp',base+'BrnTrafficLightRuntimeState.cpp'):
 data=Path(rows[source]['object']).read_bytes()
 for marker in (b'Q7Diag_',b'[tl-corona]',b'getenv',b'ReportTrafficLightCorona'):assert marker not in data,(source,marker)
for prefix,offset,target in [
 ('?UpdateHull@TrafficLightManager@','0x8c','?GetLightState@TrafficLightManager@'),
 ('?UpdateHull@TrafficLightManager@','0x94','?Update@TrafficLightRuntimeState@'),
 ('?ManageTriggers@TrafficEntityModule@BrnTraffic@@','0x1a4','?GetTriggerManagementInputInterface@'),
 ('?ManageTriggers@TrafficEntityModule@BrnTraffic@@','0x380','?GetTransform@LightTrigger@'),
 ('?ManageTriggers@TrafficEntityModule@BrnTraffic@@','0x400','?GetTriggerManagementInputInterface@'),
 ('?UpdateTimers@TrafficEntityModule@BrnTraffic@@','0x3c','?GetTimerStatusInterface@'),
 ('?UpdateTimers@TrafficEntityModule@BrnTraffic@@','0xc0','?IsSimTimerFrequency50Hz@'),
 ('?Construct@TrafficEntityModule@BrnTraffic@@','0x3f8','?Construct@TrafficCarStreamer@')]:
 assert proofs[prefix]['bindings'][offset]['symbol'].startswith(target),(prefix,offset)
assert len(proofs)==19
result={'head':args.head,'tree':tree,'targeted_binding_sha256':BINDING_SHA,'seal_sha256':args.seal_sha,'outputs':image_pins,'owners':owner_pins,'functions':proofs,'private_helpers':list(helpers.values()),'scope_note':'Early TrafficLightManager extent and physics-list offsets are separate R156 work; no runtime validation.'}
assert not args.report.exists();args.report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(sha(args.report))
