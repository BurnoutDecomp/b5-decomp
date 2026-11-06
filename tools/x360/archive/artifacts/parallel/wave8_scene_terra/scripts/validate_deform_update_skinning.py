from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdateSkinningOffsets@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x8000;VEH=A+0xa000;rng=random.Random(0x825dfa90);bad=[]
def run(comp,tags,scratch):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x82fb9000,0x1000);margin=struct.pack('>4f',.1,0,0,0);c.mem.wr(0x82fb9550,margin);c.mem.wr(F.address(b,'KVF_SKINNING_CLAMP_MARGIN'),margin);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x4b10,len(tags).to_bytes(4,'big'));c.mem.wr(A+0x6678,bytes(4));c.mem.wr(A+0x10e0,scratch)
 for i,(pos,initial,flag,amount) in enumerate(tags):
  tag=A+0x3b10+i*32;spec=SPEC+i*0x80;c.mem.wr(tag,pos);c.mem.wr(tag+0x10,spec.to_bytes(4,'big'));c.mem.wr(tag+0x1c,amount);c.mem.wr(spec+0x20,initial);c.mem.wr(spec+0x41,bytes([flag]))
 c.setg(3,A);c.run(b.entry if comp else 0x825dfa90);return c.mem.rd(A+0x10e0,len(scratch)).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 tags=[(rnd(4),rnd(4),(i>>j)&1,rnd(1)) for j in range(4)];scratch=rnd(16);x=run(False,tags,scratch);y=run(True,tags,scratch)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-skinning-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole UpdateSkinningOffsets 825DFA90, finite four-tag xyzW and all flag combinations, poisoned scratch, zero IK parts; runtime margin initialized, no real IK callbacks/nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
