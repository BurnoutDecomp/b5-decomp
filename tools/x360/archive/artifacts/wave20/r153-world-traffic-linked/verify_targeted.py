import hashlib,importlib.util,json,struct
from pathlib import Path
W=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); OUT=W/'r153-world-traffic-linked'; B=W/'r153-batch'; sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(B/'MANIFEST.json')=='d61db6067e9aacb72504ea9e6c432d7068f392a0676d87704aa5b1fb6c1f7d35'; assert sha(B/'TARGETED_OBJECT_BINDING.json')=='98eba2b3071f325fb77196423945272e9889eb3143d20803545262eecfe6169f'
b=json.loads((B/'TARGETED_OBJECT_BINDING.json').read_text()); assert b['head']=='e0e4443bf30e7aaa66481201f4f6b00186d39bb0' and b['tree']=='610a39be8529a22eeb2d463669c53ede9e46a2fe';inv=json.loads((W/'r152-link/invocation.json').read_text());spec=importlib.util.spec_from_file_location('coff',W/'r143-cih-proof/verify.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rows={Path(x['source']).name:x for x in b['results']}
def selected(n):
 x=rows[n]['artifacts']['object'];assert sha(x['path'])==x['sha256'];return Path(x['path'])
def inherited(n):
 row,=[x for x in inv['objects'] if x['source'].endswith('/'+n)];assert sha(row['object'])==row['sha256'];return Path(row['object'])
items={'world':(selected('BrnWorldModule.cpp'),'?Update@WorldModule@BrnWorld@@'),'tow':(selected('BrnTrafficVehicle.cpp'),'?CalcTowBarPos@Vehicle@BrnTraffic@@'),'light':(selected('BrnTrafficLightRuntimeState.cpp'),'?Update@TrafficLightRuntimeState@BrnTraffic@@'),'hull':(selected('BrnTrafficLightRuntimeState.cpp'),'?UpdateHull@TrafficLightManager@BrnTraffic@@'),'in_get':(inherited('BrnWorldModuleIO.cpp'),'?GetPlayerVehicleControls@UpdateInputBuffer@'),'out_get':(inherited('BrnWorldModuleIO_UpdateOutputBuffer.cpp'),'?GetPlayerVehicleControls@UpdateOutputBuffer@'),'out_set':(inherited('BrnWorldModuleIO_UpdateOutputBuffer.cpp'),'?SetPlayerVehicleControls@UpdateOutputBuffer@'),'network':(inherited('BrnGameModule_wN1_01.cpp'),'?BridgeWorldToNetwork@BrnGameModule@')}
functions={};
for label,(path,symbol) in items.items():
 code,refs=m.coff(path,symbol);functions[label]=(code,refs)
def checkrefs(label,expect):
 code,refs=functions[label]
 for off,needle in expect.items():assert needle in refs[off//4],(label,hex(off),refs.get(off//4))
def checkwords(label,expect):
 code,_=functions[label]
 for off,word in expect.items():assert code[off//4]==word,(label,hex(off),hex(code[off//4]),hex(word))
checkrefs('world',{0x314:'?LockForRead@',0x3b0:'?LockForWrite@',0x3b8:'?GetPlayerVehicleControls@UpdateInputBuffer@',0x3c4:'?SetPlayerVehicleControls@UpdateOutputBuffer@',0x3cc:'?UnlockForWrite@',0x404:'?LockForWrite@',0x414:'?BridgeInputToPhysicsModule@'}); assert not any('UnlockForRead@' in v for i,v in functions['world'][1].items() if 0x314<i*4<0x3cc)
checkwords('world',{0x3ac:0x7f43d378,0x3b4:0x7e238b78,0x3bc:0x7c641b78,0x3c0:0x7f43d378,0x3c8:0x7f43d378})
checkwords('in_get',{0x44:0x3c7f0005,0x48:0x3863d750});checkwords('out_get',{0x44:0x3c7f0002,0x48:0x38634cf4});checkwords('out_set',{0x4c:0x3c7f0002,0x50:0x38a0003c,0x54:0x38634cf4,0x58:0x7fc4f378});checkrefs('out_set',{0x5c:'memcpy'});checkrefs('network',{0x18:'?GetPlayerVehicleControls@PostSimulationInputBuffer@',0x24:'?GetPlayerVehicleControls@UpdateOutputBuffer@',0x34:'memcpy'});checkwords('network',{0x2c:0x38a0003c})
checkrefs('tow',{0x44:'?FireAssert@',0x48:'?EndAssert@',0x68:'?FireAssert@',0x6c:'?EndAssert@',0x74:'?IsValid@',0x98:'?FireAssert@',0x9c:'?EndAssert@'});checkwords('tow',{0x2c:0x409a0020,0x50:0x409a0020,0x80:0x409a0020,0xa0:0x39600020,0xa4:0x39400030,0xac:0x101c58c3,0xb0:0x1000028c,0xb4:0x11be50c3,0xb8:0x119e58c3,0xbc:0x100c682e,0xc0:0x1000e9c3})
checkwords('light',{0x64:0xc01f0000,0x68:0xec00f028,0x6c:0xd01f0000,0x70:0xff00f800,0x74:0x41990048,0x84:0xd3ff0000,0x8c:0x993f0004,0x90:0x997f0005});checkrefs('hull',{0x8c:'?GetLightState@TrafficLightManager@',0x94:'?Update@TrafficLightRuntimeState@'})
r={'head':b['head'],'tree':b['tree'],'manifest_sha256':sha(B/'MANIFEST.json'),'targeted_binding_sha256':sha(B/'TARGETED_OBJECT_BINDING.json'),'stage':'targeted COFF plus inherited R152 COFF; no linked R153 claim','objects':{k:{'path':str(path),'sha256':sha(path),'symbol_prefix':sym,'function_bytes':len(functions[k][0])*4} for k,(path,sym) in items.items()},'checks':['world input read lock remains held across output lock, getter, setter, unlock before physics','output setter and network bridge copy 60 bytes','tow uses scalar splat and four-lane fused VMX at+position with non-gating assert fallthrough','light countdown fcmpu/bgt leaves unordered values on expiration path','UpdateHull calls actual state update'],'limitations':['inherited objects and targeted COFF require official R153 selection/link verification','no Xenia or runtime']}
(OUT/'targeted-coff.json').write_text(json.dumps(r,indent=2)+'\n');print('TARGETED COFF PASS',sha(OUT/'targeted-coff.json'))

