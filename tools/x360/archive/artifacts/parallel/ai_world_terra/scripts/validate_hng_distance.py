from pathlib import Path
import hashlib,json,random,struct,sys
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/ai_world_terra/hng');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
row=json.loads((out/'compile-results.json').read_text())['results'][0];o=F.Coff(Path(row['object']),'?Distance2D@');bundle=F.Bundle([o]);bundle.select('?Distance2D@');SELF=0x40000000;STACK=F.M.S.STACK
class Stop(Exception):pass
def run(compiled,a,b):
 mem=F.M.V.Mem();mem.add(SELF,0x1000);mem.add(STACK,0x20000);mem.add(0x84800000,0x10000)
 for q,d in bundle.images.items():mem.add(q,len(d),d)
 c=F.AtomicCPU(mem);c.setg(1,STACK+0x10000);c.msr=0x8000;c.reservation=None;c.failures=0;c.atomic_trace=[];c.fence_trace=[];c.max_steps=10000
 if compiled:
  x=struct.pack('>4f',*a);y=struct.pack('>4f',*b)
  for i,v in enumerate([x[:8],x[8:],y[:8],y[8:]]):c.setg(3+i,int.from_bytes(v,'big'))
  c.run(bundle.entry);return struct.pack('>f',c.f(1)).hex()
 c.setvf(11,[a[i]-b[i] for i in range(4)]);c.setvf(0,[1]*4);c.setvf(13,[.5]*4);c.setvf(10,[0]*4);c.setvf(5,[0]*4);c.setg(11,0x40000);c.setg(10,0x820c3b70)
 def stop(pc,word):
  if pc==0x82777cc4:raise Stop()
 c.instruction_observer=stop
 try:c.run(0x82777c4c)
 except Stop:pass
 return struct.pack('>I',c.vr[12][0]).hex()
rng=random.Random(0x3607c4c);fail=[]
for i in range(128):
 a=[rng.uniform(-5000,5000) for _ in range(4)];b=[rng.uniform(-5000,5000) for _ in range(4)];a=list(struct.unpack('>4f',struct.pack('>4f',*a)));b=list(struct.unpack('>4f',struct.pack('>4f',*b)));x=run(False,a,b);y=run(True,a,b)
 if x!=y:fail.append({'case':i,'original':x,'native':y})
res={'cases':128,'mismatches':fail,'scope':'2D distance actual native helper against ARTIST 82777C4C..CB0 VMX window, common emulated estimates, finite vectors','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'distance-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(fail));print(fail[:1]);assert not fail
