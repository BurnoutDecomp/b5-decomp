from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?UpdateWheels@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;rng=random.Random(0x826254c0);bad=[]
def run(comp,tags,wheels):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.add(0x82fb9000,0x1000);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(VEH+0x710,b'\0');calls=[]
 for i in range(4):c.mem.wr(SPEC+0x70+i*48,i.to_bytes(4,'big'));c.mem.wr(A+0x3b10+i*32,tags[i]);c.mem.wr(VEH+0x1b0+i*0xe0,wheels[i]);c.mem.wr(VEH+0x130+i*0xe0+0xd7,b'\0')
 def spec(cpu):cpu.setg(3,SPEC+0x50+48*(cpu.g(4)&0xffffffff))
 def setpos(cpu):calls.append((cpu.g(3)&0xffffffff,tuple(cpu.vr[1])));cpu.mem.wr((cpu.g(3)+0x80)&0xffffffff,struct.pack('>4I',*cpu.vr[1]))
 c.hooks[0x825bda10]=setpos
 for k,v in b.unresolved.items():
  if 'GetWheelSpec' in v:c.hooks[k]=spec
  elif 'SetPosition@Wheel' in v:c.hooks[k]=setpos
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v:c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0x8000);c.setg(5,A+0x8100);c.setg(7,A+0x8200);c.setf(1,.016);c.run(b.entry if comp else 0x826254c0);return calls,[c.mem.rd(VEH+0x1b0+i*0xe0,16).hex() for i in range(4)]
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 tags=[rnd(4) for _ in range(4)];wheels=[rnd(4) for _ in range(4)];x=run(False,tags,wheels);y=run(True,tags,wheels)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-wheels-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole UpdateWheels original vs actual object, four attached wheels, noncrashing branch, finite random positions including W; Wheel SetPosition and wheel-spec access controlled, compares calls and wheel vector bytes. Twisting/detach branches, nonfinite and hardware untested','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
