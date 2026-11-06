from pathlib import Path
import hashlib,json,random,struct,sys
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/ai_world_terra/fan-target');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
row=json.loads((out/'compile-results.json').read_text())['results'][0];o=F.Coff(Path(row['object']),'?AccumulateWeightings@SteeringFan@BrnAI@@');bundle=F.Bundle([o]);bundle.select('?AccumulateWeightings@SteeringFan@BrnAI@@');_,val,sec=bundle.definitions[next(n for n in bundle.definitions if 'kfBias' in n)];bias=bundle.bases[(0,sec)]+val;SELF=0x40000000;STACK=F.M.S.STACK

def run(compiled,image,biases):
 mem=F.M.V.Mem();mem.add(SELF,0x1000);mem.add(STACK,0x20000);mem.add(0x84800000,0x10000);mem.add(0x82f30400,0x400)
 for a,d in bundle.images.items():mem.add(a,len(d),d)
 c=F.AtomicCPU(mem);c.setg(1,STACK+0x10000);c.msr=0x8000;c.reservation=None;c.failures=0;c.atomic_trace=[];c.fence_trace=[];c.max_steps=100000;c.mem.wr(SELF,image);c.mem.wr(bias if compiled else 0x82f30468,biases);c.setg(3,SELF)
 for at,name in bundle.unresolved.items():
  def bad(cpu,name=name):raise AssertionError(('unexpected',name))
  c.hooks[at]=bad
 c.run(bundle.entry if compiled else 0x82779088);return c.mem.rd(SELF,len(image)),c.g(3)&0xffffffff
rng=random.Random(0x36079088);fail=[]
for i in range(128):
 image=bytearray(rng.randbytes(0x900));struct.pack_into('>I',image,0x3b0,0)
 for j in range(14*17):struct.pack_into('>f',image,0x3b4+4*j,rng.uniform(-2,2))
 for j in range(17):struct.pack_into('>f',image,0x76c+4*j,rng.uniform(-2,2))
 biases=struct.pack('>14f',*[rng.choice([0.0,rng.uniform(-2,2)]) for _ in range(14)]);a=run(False,image,biases);b=run(True,image,biases)
 if a!=b:fail.append({'case':i,'differing_offsets':[hex(j) for j,(x,y) in enumerate(zip(a[0],b[0])) if x!=y][:20]})
res={'cases':128,'mismatches':fail,'scope':'whole AccumulateWeightings with finite random rows, biases and cumulative state; whole object bytes and return compared; emulated PPC','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'accumulate-comparison.json').write_text(json.dumps(res,indent=2)+'\n');print('cases',128,'mismatches',len(fail));print(fail[:1]);assert not fail
