from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-checkpoint2';r=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnRendererModule.cpp'));p=Path(r['object']);o=F.Coff(p,'?GetVSynchRate@BrnRendererModule@@');rng=random.Random(0x823F5628);values=[0,1,29.97,30,49.49,49.5,49.500004,49.999996,50,50.5,59.49,59.5,59.94,59.999996,60,60.5,119.88,120,144,240,-0.5,-0.50000006,-1.5,16777215.0,16777214.0,2147483520.0,-2147483648.0];errors=[]
values+=[rng.uniform(-1000000,1000000) for _ in range(128-len(values))]
for i,value in enumerate(values):
 mode=bytearray(rng.randbytes(48));struct.pack_into('>f',mode,20,value);both=[]
 for compiled in [False,True]:
  c=F.cpu(o);c.setg(3,rng.getrandbits(32));trace=[]
  def query(x):trace.append('XGetVideoMode');x.mem.wr(x.g(3)&0xffffffff,mode)
  c.hooks[0x82C75B7C]=query
  for at,name in o.unresolved.items():
   if name=='XGetVideoMode':c.hooks[at]=query
  c.run(o.entry if compiled else 0x823F5628);both.append((c.g(3)&0xffffffff,trace))
 if both[0]!=both[1]:errors.append(dict(case=i,input=struct.unpack_from('>f',mode,20)[0],results=both))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='actual native static GetVSynchRate against ARTIST823F5628:real48byteSDKmode,RefreshRate+14,single-precisionadd0.5f,fctiwz;includes59.94 and16,777,215 rounding-order witness',limits='XGetVideoModecontrolled;noDeviceParametersrefreshsubstitutionorhardwaremodeclaim')
(out/'renderer-vsync-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native GetVSynchRate cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
