import sys,random,json,struct,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re');import x360rd
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline');obj=out/'triangle-dispatch-probe.obj';b=F.Bundle([F.Coff(obj,'?TriangleLineSegIntersect@')]);b.select('?TriangleLineSegIntersect@');rng=random.Random(0x82bbb7b8);O=0x50000000;bad=[];thinHits=0;fatHits=0
for i in range(192):
 def bits(v):return tuple(struct.unpack('>I',struct.pack('>f',x))[0] for x in v)
 w=lambda:rng.uniform(-2,2)
 if i%3==0:vectors=[bits((rng.uniform(-.2,1.2),rng.uniform(-.2,1.2),.5,w())),bits((0,0,-1,w())),bits((0,0,0,w())),bits((1,0,0,w())),bits((0,1,0,w()))]
 else:vectors=[bits([rng.uniform(-2,2) for _ in range(4)]) for k in range(5)]
 fat=0.0 if i%2==0 else .25;initial=rng.randbytes(0x100);output=struct.pack('>4f',*(rng.uniform(-5,5) for _ in range(4)));r=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(O,0x100,initial)
  if not compiled:
   for a in [0x82001000,0x8200d000,0x82180000]:c.mem.add(a,0x1000,x360rd.rd(a,0x1000))
  trace=[]
  def fatHook(cpu):
   ptr=cpu.g(3)&0xffffffff;trace.append((ptr,tuple(cpu.vr[k] for k in range(1,6)),cpu.f(1),cpu.mem.rd(ptr+0x20,16).hex()));
   if i%4==1:cpu.mem.wr(ptr+0x10,output)
   cpu.setg(3,1 if i%4==1 else 0)
  if compiled:loc=next(a for a,n in b.unresolved.items() if 'FatTriangleLineSegIntersect' in n);c.hooks[loc]=fatHook
  else:c.hooks[0x82bbad98]=fatHook
  c.setg(3,O);c.setf(1,fat)
  for k,v in enumerate(vectors):c.vr[k+1]=v
  c.run(b.entry if compiled else 0x82bbb7b8);r.append((c.mem.rd(O,0x100),c.g(3)&0xffffffff,trace))
 if fat==0 and r[0][1]:thinHits+=1
 if fat!=0 and r[0][1]:fatHits+=1
 if r[0]!=r[1]:bad.append([i,r[0][0][:0x50].hex(),r[1][0][:0x50].hex(),r[0][2],r[1][2]])
print('cases 192 thinHits',thinHits,'fatHits',fatHits,'mismatches',len(bad),bad[:1]);assert not bad and thinHits and fatHits
(out/'triangle-dispatch-probe-comparison.json').write_text(json.dumps({'cases':192,'thin_hits':thinHits,'fat_hits':fatHits,'mismatches':bad,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'temporary extracted dispatcher plus real compiled thin helper; original 82BBB7B8, controlled fat helper captures input lanes/normal and simulates hit/miss, full result/guards; finite inputs, estimate emulator; NOT whole provider'},indent=2))
