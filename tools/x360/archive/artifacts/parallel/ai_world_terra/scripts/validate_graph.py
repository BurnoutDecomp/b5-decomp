from pathlib import Path
import hashlib,json,random,struct,sys
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/ai_world_terra/race-balancing-final');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
row=next(r for r in json.loads((out/'compile-results.json').read_text())['results'] if r['source'].endswith('BrnRaceBalancingGraph.cpp'));o=F.Coff(Path(row['object']),'?ComputeSpeedRatio@RaceBalancingGraph@BrnAI@@');bundle=F.Bundle([o]);bundle.select('?ComputeSpeedRatio@RaceBalancingGraph@BrnAI@@');SELF=0x40000000;STACK=F.M.S.STACK

def run(compiled,data,frac,graph):
 mem=F.M.V.Mem();mem.add(SELF,0x1000);mem.add(STACK,0x20000);mem.add(0x84800000,0x10000)
 for a,d in bundle.images.items():mem.add(a,len(d),d)
 c=F.AtomicCPU(mem);c.setg(1,STACK+0x10000);c.msr=0x8000;c.reservation=None;c.failures=0;c.atomic_trace=[];c.fence_trace=[];c.max_steps=50000;c.mem.wr(SELF,data);c.setg(3,SELF);c.setg(4,graph);c.setf(1,frac)
 for at,name in bundle.unresolved.items():
  def bad(cpu,name=name):raise AssertionError(('unexpected service',name))
  c.hooks[at]=bad
 c.run(bundle.entry if compiled else 0x8277b748);return struct.pack('>f',c.f(1)).hex()
rng=random.Random(0x360b748);fail=[]
for i in range(256):
 data=struct.pack('>16f',*[rng.uniform(-3,3) for j in range(16)]);frac=rng.uniform(-1,2);graph=i%2;a=run(False,data,frac,graph);b=run(True,data,frac,graph)
 if a!=b:fail.append({'case':i,'frac':frac,'original':a,'native':b})
result={'cases':256,'mismatches':fail,'scope':'whole ComputeSpeedRatio valid graph indices, randomized finite values, no assertion paths; original and actual native PPC emulated','object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'graph-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',256,'mismatches',len(fail));print(fail[:1]);assert not fail
