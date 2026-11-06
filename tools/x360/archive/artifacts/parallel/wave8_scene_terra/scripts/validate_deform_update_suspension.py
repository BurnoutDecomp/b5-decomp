from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdateIKSuspensionOffsets@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x8000;SA=A+0x9000;SB=A+0x9200;PA=A+0x9600;PB=A+0x9700;VEH=A+0xb000;rng=random.Random(0x826083b0);bad=[]
def run(comp,pos,offa,offb,initial,weights,scra,scrb,pa,pb,wheel,stream,skinned,scratch):
 c=F.cpu(b);c.mem.add(A,0x10000);tag=A+0x3b10;c.mem.wr(A+0xf50,b'\x00\xff\xff\xff');c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(tag,pos);c.mem.wr(tag+0x10,SPEC.to_bytes(4,'big'));c.mem.wr(tag+0x14,SA.to_bytes(4,'big'));c.mem.wr(tag+0x18,SB.to_bytes(4,'big'));c.mem.wr(SPEC,offa+offb+initial);c.mem.wr(SPEC+0x30,weights);c.mem.wr(SPEC+0x41,bytes([skinned]));c.mem.wr(SA+0x19c,PA.to_bytes(4,'big'));c.mem.wr(SB+0x19c,PB.to_bytes(4,'big'));c.mem.wr(SA+0x1a4,scra);c.mem.wr(SB+0x1a4,scrb);c.mem.wr(PA,pa);c.mem.wr(PB,pb);c.mem.wr(VEH+0x1b0,wheel);c.mem.wr(VEH+0x1c0,stream);c.mem.wr(A+0x10e0,scratch)
 for k,v in b.unresolved.items():
  if 'GetWheel@' in v:c.hooks[k]=lambda cpu:cpu.setg(3,VEH+0x130)
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v:c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.run(b.entry if comp else 0x826083b0);return c.mem.rd(tag,16).hex(),c.mem.rd(tag+0x1c,4).hex(),c.mem.rd(A+0x10e0,16).hex()
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 args=(rnd(4),rnd(4),rnd(4),rnd(4),rnd(2),rnd(1),rnd(1),rnd(4),rnd(4),rnd(4),rnd(4),i%2,rnd(4));x=run(False,*args);y=run(True,*args)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-suspension-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole UpdateIKSuspensionOffsets 826083B0 vs actual object, one wheel/tag, finite random xyzW/weights, skinned and unskinned, compares full position/scratch; wheel accessor controlled, no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
