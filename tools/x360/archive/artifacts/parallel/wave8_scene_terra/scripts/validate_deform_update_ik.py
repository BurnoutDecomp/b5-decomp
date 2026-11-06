from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdateIK@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x8000;SENS=A+0x9000;SP=A+0xb000;rng=random.Random(0x82608858);bad=[]
def run(comp,tags,dt):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A+0x4b10,len(tags).to_bytes(4,'big'));c.mem.wr(A+0x6678,bytes(4))
 for i,(pos,offa,offb,sphera,spherb,weights,scratcha,scratchb) in enumerate(tags):
  tag=A+0x3b10+i*32;spec=SPEC+i*0x80;sa=SENS+i*0x400;sb=sa+0x1b0;pa=SP+i*32;pb=pa+16;c.mem.wr(tag,pos);c.mem.wr(tag+0x10,spec.to_bytes(4,'big'));c.mem.wr(tag+0x14,sa.to_bytes(4,'big'));c.mem.wr(tag+0x18,sb.to_bytes(4,'big'));c.mem.wr(spec,offa+offb);c.mem.wr(spec+0x30,weights);c.mem.wr(sa+0x19c,pa.to_bytes(4,'big'));c.mem.wr(sb+0x19c,pb.to_bytes(4,'big'));c.mem.wr(sa+0x1a4,scratcha);c.mem.wr(sb+0x1a4,scratchb);c.mem.wr(pa,sphera);c.mem.wr(pb,spherb)
 for k,v in b.unresolved.items():
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.vr[1]=struct.unpack('>4I',dt);c.run(b.entry if comp else 0x82608858);return [c.mem.rd(A+0x3b10+i*32,16).hex()+c.mem.rd(A+0x3b10+i*32+0x1c,4).hex() for i in range(len(tags))]
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 tags=[(rnd(4),rnd(4),rnd(4),rnd(4),rnd(4),rnd(2),rnd(1),rnd(1)) for j in range(i%4)];dt=rnd(4);x=run(False,tags,dt);y=run(True,tags,dt)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-ik-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole UpdateIK original vs actual object, 0-3 finite arbitrary tags, weights and distinct time lanes; compares position xyzW and scratch, no IK parts, nonfinite or hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
