from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/camera-three-c0d-terra');j=json.loads((O/'camera-final-v1/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CameraUtils.cpp')));sym='?ProjectWorldSpacePointToScreen@Utils@Camera@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;OUT=F.M.S.AUX+0x1000;rng=random.Random(0x82222060)
def run(compiled,mat,point,out):
 c=F.cpu(b);c.mem.wr(S,mat);c.mem.wr(OUT,out);c.setg(3,S);c.setg(4,OUT);c.vr[1]=struct.unpack('>4I',point)
 for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
 c.run(b.entry if compiled else 0x82222060);return c.g(3),c.mem.rd(OUT,16)
positive=negative=0
for i in range(256):
 mat=struct.pack('>16f',*[rng.uniform(-5,5) for _ in range(16)]);point=struct.pack('>4f',*[rng.uniform(-4,4) for _ in range(4)]);out=rng.randbytes(16);a=run(False,mat,point,out);z=run(True,mat,point,out)
 if a!=z:print(i,a,z);raise AssertionError(i)
 if a[0]:positive+=1
 else:negative+=1
assert positive and negative
r={'cases':256,'true':positive,'false':negative,'scope':'Original ARTIST 82222060 vs actual compiled ProjectWorldSpacePointToScreen; native VPU3 v1 input, matrix r3, output r4, return and complete16 output bytes including preserved Z/W. Random finite inputs with both visibility outcomes; no mocks.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'projection-comparison-final-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
