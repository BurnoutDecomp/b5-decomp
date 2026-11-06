from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'compose-wip3/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('BrnCollisionPolicyAttachedToVehicle.cpp')));vp=Path(next(x['object'] for x in j['results'] if x['source'].endswith('vecfloat.cpp')));utils=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CameraUtils.cpp')));sym='?ProcessSceneQueryResults@FrustrumCollisionResolver@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym),F.Coff(vp,'??0VecFloat@vpu@math@rw@@QAA@ABM'),F.Coff(utils,'?ResolveLineTestNearestUsingDisplacementAndVector@Utils@Camera@BrnDirector@@')]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;CAM=A+0x1000;OUT=A+0x3000;INFO=A+0x4000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822242F8.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
addr=next(target(int(l.split()[0],16)) for l in orig if 'ResolveLineTestNearestUsingDisplacementAndVector' in l and '  bl ' in l)
class CPU(F.AtomicCPU):
 def exec_vector(self,pc,w,mn):
  if w>>26==5 and w&0x3d0==0x250:
   d=((w>>21)&31)|(((w>>2)&3)<<5);a=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);z=((w>>11)&31)|((w&3)<<5);self.vr[d]=tuple(self.vr[a][i] & (~self.vr[z][i]&0xffffffff) for i in range(4));return True
  return super().exec_vector(pc,w,mn)
def run(compiled,box,camera,targetvec):
 c=F.cpu(b);c.__class__=CPU;c.mem.wr(S,box);c.mem.wr(CAM,camera);c.mem.wr(OUT,b'\xa5'*16);c.setg(3,S);c.setg(4,INFO);c.setg(5,CAM);c.setg(6,OUT);c.vr[1]=struct.unpack('>4I',targetvec);trace=[]
 def effect(cc,n):
  if 'ResolveLineTestNearestUsingDisplacementAndVector' in n:trace.append((cc.g(3)-S,cc.g(4)-CAM,tuple(cc.vr[1]),tuple(cc.vr[2]),tuple(cc.vr[3])));cc.setg(3,0)
  elif 'GetNearClipDistance' in n:cc.setf(1,struct.unpack('>f',cc.mem.rd(CAM+0x150,4))[0])
  elif n.startswith('__save') or n.startswith('__rest'):pass
  else:raise AssertionError(('unexpected',n))
 if compiled:
  for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
  c.run(b.entry)
 else:c.run(0x822242f8)
 return c.g(3),c.mem.rd(OUT,16),c.mem.rd(S,0x160),trace
rng=random.Random(0x822242f8)
for i in range(128):
 box=bytearray(rng.randbytes(0x160));box[0x150:0x154]=struct.pack('>f',rng.uniform(.1,5));
 for k in range(4):
  o=k*0x50;box[o:o+4]=struct.pack('>I',2);box[o+0x10:o+0x20]=struct.pack('>4f',0,0,0,0);box[o+0x20:o+0x30]=struct.pack('>4f',1,0,0,0);box[o+0x48]=i%2
 camera=bytearray(rng.randbytes(0x170));camera[:64]=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)]);camera[0x58:0x60]=struct.pack('>2f',rng.uniform(30,90),rng.uniform(.7,2));camera[0x150:0x154]=struct.pack('>f',rng.uniform(.1,2));camera[0x15d]=1;vec=struct.pack('>4f',*[rng.uniform(-3,3) for _ in range(4)]);a=run(False,bytes(box),bytes(camera),vec);z=run(True,bytes(box),bytes(camera),vec)
 if a!=z:
  print('case',i,'ret/normal',a[:2],z[:2]);print('traceA',a[3]);print('traceZ',z[3]);print('boxdiff',[hex(k) for k,(u,v) in enumerate(zip(a[2],z[2])) if u!=v]);raise AssertionError(i)
r={'cases':128,'scope':'ARTIST 822242F8 vs actual compiled FrustrumCollisionResolver caller; actual caller and actual displacement resolver composed on both sides; full resolver object, return and normal output. Finite random camera/target; real GetNearClipDistance represented by controlled offset-equivalent load, valid postboxes, hit/no-hit.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'collision-composed-comparison-wip3.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))



