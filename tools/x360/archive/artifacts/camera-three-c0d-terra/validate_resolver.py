from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'shadow-wip1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CameraUtils.cpp')));sym='?ResolveLineTestNearestUsingDisplacementAndVector@Utils@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;POS=A+0x1000;rng=random.Random(0x8220ceb0)
class CPU(F.AtomicCPU):
 def exec_vector(self,pc,w,mn):
  if w>>26==5 and w&0x3d0==0x250:
   d=((w>>21)&31)|(((w>>2)&3)<<5);a=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);z=((w>>11)&31)|((w&3)<<5);self.vr[d]=tuple(self.vr[a][i] & (~self.vr[z][i]&0xffffffff) for i in range(4));return True
  return super().exec_vector(pc,w,mn)
def run(compiled,box,pos,test,vec,mind):
 c=F.cpu(b);c.__class__=CPU;c.mem.wr(S,box);c.mem.wr(POS,pos);c.setg(3,S);c.setg(4,POS);c.vr[1]=struct.unpack('>4I',test);c.vr[2]=struct.unpack('>4I',vec);c.vr[3]=struct.unpack('>4I',mind)
 for at,n in b.unresolved.items():
  if n.startswith('__save') or n.startswith('__rest'):c.hooks[at]=lambda cc:None
  else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
 c.run(b.entry if compiled else 0x8220ceb0);return c.g(3),c.mem.rd(POS,16)
for i in range(256):
 box=bytearray(0x50);box[:4]=struct.pack('>I',2);box[0x10:0x20]=struct.pack('>4f',0,0,0,0);box[0x20:0x30]=struct.pack('>4f',1,0,0,0);box[0x48]=0 if i%4==0 else 1
 test=struct.pack('>4f',float((i%7)-3),float(rng.uniform(-5,5)),float(rng.uniform(-5,5)),0);pos=struct.pack('>4f',*[rng.uniform(-5,5) for _ in range(4)]);vec=struct.pack('>4f',*[rng.uniform(-5,5) for _ in range(4)]);mind=struct.pack('>4f',*[rng.uniform(.1,5) for _ in range(4)]);a=run(False,bytes(box),pos,test,vec,mind);z=run(True,bytes(box),pos,test,vec,mind)
 if a!=z:print('case',i,a,z);raise AssertionError(i)
r={'cases':256,'scope':'Original ARTIST 8220CEB0 vs actual compiled displacement resolver. Genuine SDK VecFloat v3, test/vector v1/v2, postbox r3 and position r4; finite randomized lanes, valid postbox, both hit/no-hit. Compare return and full position vector; hardware reciprocal estimate uses emulator model.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'resolver-comparison-wip.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))


