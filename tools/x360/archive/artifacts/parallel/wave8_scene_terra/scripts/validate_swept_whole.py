from pathlib import Path
import sys,json,struct,random,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
class SweptCPU(F.AtomicCPU):
 def exec_vector(self,a,w,mn):
  if w>>26==6 and (w&0x7f0)==0x670:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);vb=((w>>11)&31)|((w&3)<<5);x,y=self.vr[30],self.vr[31];self.vr[31]=self.vr[vb];super().exec_vector(a,(4<<26)|(30<<21)|(31<<11)|330,'vrsqrtefp');z=self.vr[30];self.vr[30],self.vr[31]=x,y;self.vr[vd]=z;return True
  if w>>26==5 and (w&0x3d0)==0x290:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);vb=((w>>11)&31)|((w&3)<<5);self.vr[vd]=tuple(~(self.vr[va][i]|self.vr[vb][i])&0xffffffff for i in range(4));return True
  if w>>26==5 and (w&0x3d0)==0x250:
   vd=((w>>21)&31)|(((w>>2)&3)<<5);va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);vb=((w>>11)&31)|((w&3)<<5);self.vr[vd]=tuple(self.vr[va][i]&(~self.vr[vb][i]&0xffffffff) for i in range(4));return True
  return super().exec_vector(a,w,mn)
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');obj=p/'triangle.obj';prov=p/'sweptsphere-provider.obj';names=['?IntersectTriangle4SweptSphere@','?Intersect2DCircleWithTriangleSOA@','?GetPositionAndRadius@SweptSphere@','?GetDirectionAndLength@SweptSphere@'];b=F.Bundle([F.Coff(obj,names[0]),F.Coff(obj,names[1]),F.Coff(prov,names[2]),F.Coff(prov,names[3])]);b.select(names[0]);A=0x40000000;TRI=A+0x1000;OUT=A+0x2000;rng=random.Random(0x8283ef50);bad=[]
def run(comp,sphere,tri):
 c=F.cpu(b);c.__class__=SweptCPU;c.mem.add(A,0x5000);c.mem.wr(A,sphere);c.mem.wr(TRI,tri)
 for k,v in b.unresolved.items():
  if v.startswith('__save') or v.startswith('__rest'):c.hooks[k]=lambda cpu:None
 for i in range(16):
  addr=OUT+i*0x30+0x10;c.mem.wr(addr-16,b'\xA5'*48)
  if i<6:c.setg(5+i,addr)
  else:c.mem.wr((c.g(1)+0x54+8*(i-6))&0xffffffff,addr.to_bytes(4,'big'))
 c.setg(3,A);c.setg(4,TRI);c.run(b.entry if comp else 0x8283ef50);return tuple(c.vr[1]),[c.mem.rd(OUT+i*0x30,48).hex() for i in range(16)]
for i in range(128):
 rnd=lambda n:struct.pack('>'+str(n)+'f',*[rng.uniform(-5,5) for _ in range(n)])
 sphere=rnd(3)+struct.pack('>f',rng.uniform(.05,4))+rnd(3)+struct.pack('>f',rng.uniform(.05,4));tri=bytearray(rnd(56));tri[0x90:0xa0]=struct.pack('>4I',*[rng.choice([0,0xffffffff]) for _ in range(4)]);x=run(False,sphere,tri);y=run(True,sphere,tri)
 if x!=y:bad.append({'case':i,'original':x,'compiled':y});break
print('mismatches',len(bad),bad[:1]);(p/'swept-whole-comparison.json').write_text(json.dumps({'cases':128,'mismatches':bad,'scope':'whole IntersectTriangle4SweptSphere 8283EF50 including real original/compiled 2D-circle callee, real linked SweptSphere getters; finite distinct triangle lanes, arbitrary valid masks; compares all 16 output rows plus guards and v1 mask; no whole owning TU/nonfinite/hardware claim','object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'provider_sha256':hashlib.sha256(prov.read_bytes()).hexdigest()},indent=2)+'\n');assert not bad
