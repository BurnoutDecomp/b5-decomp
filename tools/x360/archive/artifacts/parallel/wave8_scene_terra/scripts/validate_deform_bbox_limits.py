from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'12.obj';n='?CalculateDriveTimeLimits@DeformableObject@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;SPEC=A+0x9000;VEH=A+0xb000;ATTR=A+0xd000;rng=random.Random(0x82609dc8);bad=[]
def run(comp,sensors,limits):
 c=F.cpu(b);c.mem.add(A,0x10000);c.mem.wr(A+0x18e0,SPEC.to_bytes(4,'big'));c.mem.wr(A+0x194c,VEH.to_bytes(4,'big'));c.mem.wr(SPEC+0x652,bytes([len(sensors)]));c.mem.wr(VEH+0x720,ATTR.to_bytes(4,'big'));c.mem.wr(ATTR+0x40,limits)
 for i,(pos,radius) in enumerate(sensors):c.mem.wr(SPEC+0x110+i*64,pos);c.mem.wr(SPEC+0x138+i*64,radius)
 fire=[]
 for addr in (0x82817548,0x82817558):c.hooks[addr]=lambda cpu:None
 c.hooks[0x82820810]=lambda cpu:fire.append(1)
 for k,v in b.unresolved.items():
  if 'FireAssert' in v:c.hooks[k]=lambda cpu:fire.append(1)
  elif 'Assert' in v or v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 c.setg(3,A);c.run(b.entry if comp else 0x82609dc8);return c.mem.rd(A+0x66f0,32).hex(),len(fire)
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 sensors=[(rnd(4),rnd(1)) for j in range(i%5)];limits=rnd(4);x=run(False,sensors,limits);y=run(True,sensors,limits)
 if x!=y:bad.append({'i':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'bbox-limits-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole CalculateDriveTimeLimits vs original, 0-4 finite random sensor xyzW/radius and attributes, compares bounds xyzW and controlled assertion count; valid physics/attributes; no nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
