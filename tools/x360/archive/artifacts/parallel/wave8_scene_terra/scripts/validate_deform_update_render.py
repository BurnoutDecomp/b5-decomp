from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'14.obj';n='?RenderSensors@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;SS=A+0xd000;SP=A+0xe000;R=A+0xf000;rng=random.Random(0x825e08c0);bad=[]
def run(comp,tr,local,worlds):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(SPEC+0x652,b'\x01');c.mem.wr(VEH+0x10,tr);c.mem.wr(A+0x1950+4,SS.to_bytes(4,'big'));c.mem.wr(A+0x1950+0x19c,SP.to_bytes(4,'big'));c.mem.wr(A+0x1950+0x1a0,(A+0x10).to_bytes(4,'big'));c.mem.wr(SP,local);c.mem.wr(A+0x10,worlds);c.mem.wr(A+0x66e0,bytes(4));calls=[]
 def box(cpu):calls.append(('box',tuple(cpu.vr[1]),tuple(cpu.vr[2]),cpu.mem.rd(cpu.g(4)&0xffffffff,64).hex()))
 def line(cpu):calls.append(('line',tuple(cpu.vr[1]),tuple(cpu.vr[2])))
 def color(cpu):cpu.mem.wr(cpu.g(3)&0xffffffff,bytes(cpu.g(i)&255 for i in (4,5,6,7)))
 c.hooks[0x8281d1f0]=line;c.hooks[0x8281dee0]=box
 for k,v in b.unresolved.items():
  if 'GetTransform@DeformableObject' in v:c.hooks[k]=lambda cpu:cpu.mem.wr(cpu.g(4)&0xffffffff,tr)
  elif 'DrawBox@' in v:c.hooks[k]=box
  elif 'DrawLine@' in v:c.hooks[k]=line
  elif '??0RGBA' in v:c.hooks[k]=color
  elif v.startswith('__save') or v.startswith('__rest') or 'Assert' in v:c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.setg(4,R);c.setg(5,0);c.run(b.entry if comp else 0x825e08c0);return calls
for i in range(64):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-4,4) for _ in range(n)])
 tr=rnd(16);local=rnd(4);worlds=rnd(16);x=run(False,tr,local,worlds);y=run(True,tr,local,worlds)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'update-render-comparison.json').write_text(json.dumps({'cases':64,'mismatches':bad,'scope':'whole RenderSensors 825E08C0, selected one sensor no links and four wheels; finite arbitrary matrices/sphere xyzW, controlled transform and DrawBox/DrawLine/RGBA, compares vector and matrix payloads/call order; no unselected color ramp, real renderer/nonfinite/hardware','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
