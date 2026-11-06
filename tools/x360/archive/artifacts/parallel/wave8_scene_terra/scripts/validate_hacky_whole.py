from pathlib import Path
import sys,json,struct,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
class HackyCPU(F.AtomicCPU):
 def exec_vector(self,a,w,mn):
  if w>>26==6 and (w&0x7f0)==0x670:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);vb=((w>>11)&31)|((w&3)<<5)
   saved30,saved31=self.vr[30],self.vr[31];self.vr[31]=self.vr[vb]
   super().exec_vector(a,(4<<26)|(30<<21)|(31<<11)|330,'vrsqrtefp');result=self.vr[30];self.vr[30],self.vr[31]=saved30,saved31;self.vr[vd]=result;return True
  return super().exec_vector(a,w,mn)
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'triangle.obj';n='?IntersectTriangle4Sphere_HackyBurnoutVersion@';o=F.Coff(obj,n);b=F.Bundle([o]);b.select(n);A=0x40000000;TRI=A+0x1000;OUT=A+0x2000;rng=random.Random(0x8283d2e0);bad=[]
def run(comp,sphere,tri,padding):
 c=F.cpu(b);c.__class__=HackyCPU;c.mem.add(A,0x5000);c.mem.add(0x83039000,0x1000);c.mem.wr(A,sphere);c.mem.wr(TRI,tri);eps=struct.pack('>4f',*[.001]*4);c.mem.wr(0x83039220,eps);c.mem.wr(F.address(b,'KF_MIN_PLANE_DIST@CgsGeometric'),eps)
 for i in range(16):
  addr=OUT+i*0x30+0x10;c.mem.wr(addr-16,b'\xA5'*48)
  if i<6:c.setg(5+i,addr)
  else:c.mem.wr((c.g(1)+0x64+8*(i-6))&0xffffffff,addr.to_bytes(4,'big'))
 c.setg(3,A);c.setg(4,TRI);c.vr[1]=struct.unpack('>4I',padding);c.run(b.entry if comp else 0x8283d2e0);return tuple(c.vr[1]),[c.mem.rd(OUT+i*0x30,48).hex() for i in range(16)]
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 sphere=rnd(3)+struct.pack('>f',rng.uniform(.05,4));tri=bytearray(rnd(56));tri[0x90:0xa0]=struct.pack('>4I',*[rng.choice([0,0xffffffff]) for _ in range(4)]);padding=rnd(4);x=run(False,sphere,tri,padding);y=run(True,sphere,tri,padding)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatch',len(bad),bad[:1]);(p/'hacky-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole ARTIST 8283D2E0 vs temporary exact generated native method with actual types; finite random distinct triangle lanes/padding, arbitrary validity masks, full 16 outputs and surrounding guards plus result mask; no full owning TU, nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
