from pathlib import Path
import json,re,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'rig-wip/compile-results.json').read_text());obj=Path(j['results'][0]['object']);src=Path('src/GameSource/Director/Camera/Utils/BrnCameraRigParams.cpp').read_text(encoding='utf8');rows=re.findall(r'// (Params\w+) @0x([0-9A-Fa-f]+) \(vectors: CRT thunk 0x([0-9A-Fa-f]+)',src);print('rows',len(rows));assert len(rows)==20
for name,address,thunk in rows:
 address=int(address,16);thunk=int(thunk,16);prefix='??__E?'+name+'@CameraRig@';b=F.Bundle([F.Coff(obj,prefix)]);b.select(prefix);data=[(oi,v,sec) for n,(oi,v,sec) in b.definitions.items() if n.startswith('?'+name+'@CameraRig@')];assert len(data)==1,(name,data);oi,v,sec=data[0];actual=b.bases[(oi,sec)]+v
 x=F.cpu(b);x.run(b.entry);got=x.mem.rd(actual,64)
 z=F.cpu(b);base=address&~0xfff;z.mem.add(base,0x2000,F.M.V.x360rd.rd(base,0x2000));z.run(thunk);expected=z.mem.rd(address,64)
 if got!=expected:print(name,'diff',[hex(i) for i,(a,c) in enumerate(zip(got,expected)) if a!=c],got.hex(),expected.hex());raise AssertionError(name)
r={'presets':20,'bytes_per_preset':64,'scope':'Actual compiled C++ static data and dynamic initializer for each CameraRig preset vs original X360 image data and original CRT thunk, all 64 bytes including both vector W lanes, scalar values, flag and padding.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'rig-presets-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
