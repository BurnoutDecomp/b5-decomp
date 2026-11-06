from pathlib import Path
import sys,json,struct,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final2/compile-results.json').read_text());paths={Path(x['source']).name:Path(x['object']) for x in j['results'] if x['passed']};obj=paths['BrnTrafficLightTrigger.cpp'];provider=paths['BrnTrafficLightCollection.cpp'];sym='?GetTransform@LightTrigger@BrnTraffic@@';b=F.Bundle([F.Coff(obj,sym),F.Coff(provider,'?ExpandPosPlusYRotToTransform@BrnTraffic@@')]);b.select(sym);A=F.M.S.AUX;TRIGGER=A+0x1000;OUT=A+0x2000
class Stop(Exception):pass
def run(compiled,data):
 c=F.cpu(b);c.mem.wr(TRIGGER+0x10,data)
 if compiled:
  c.mem.wr(OUT,b'\xa5'*64);c.setg(3,OUT);c.setg(4,TRIGGER)
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry);return c.mem.rd(OUT,64)
 c.setg(31,TRIGGER);c.setg(23,16);c.setg(28,0);c.setg(29,0);out=c.g(1)+0xd0;c.mem.wr(out,b'\xa5'*64)
 def observe(pc,w):
  if pc==0x82747804:raise Stop()
 c.instruction_observer=observe
 try:c.run(0x827477f0)
 except Stop:pass
 return c.mem.rd(out,64)
rng=random.Random(0x827477f0)
for i in range(128):
 v=[rng.uniform(-1000,1000) for _ in range(4)];data=struct.pack('>4f',*v);a=run(False,data);z=run(True,data);assert a==z,(i,a.hex(),z.hex())
r={'cases':128,'scope':'Composed original ManageTriggers inline caller -> original traffic transform versus compiled actual LightTrigger::GetTransform -> compiled actual provider; original and compiled use unchanged v1 input and their own sret r3. Full 64-byte returned matrix, finite inputs.','caller_object':str(obj),'caller_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'provider_object':str(provider),'provider_sha256':hashlib.sha256(provider.read_bytes()).hexdigest()};(O/'trigger-composed-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
