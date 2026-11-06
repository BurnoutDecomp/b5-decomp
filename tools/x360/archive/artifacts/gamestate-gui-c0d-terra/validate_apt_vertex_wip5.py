from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gamestate-gui-c0d-terra');j=json.loads((O/'apt-wip5/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('CgsAptCallbackRender.cpp')));sym='?SetVertexMatrix@AptCallbackRender@CgsGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;AUX=A+0x1000;MAT=A+0x4000
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8284B838.json'))['assembly'].splitlines()
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def run(compiled,initial,mat):
 c=F.cpu(b);c.mem.wr(AUX,initial);c.mem.wr(MAT,mat);c.setg(3,MAT);c.mem.add(0x8305a000,0x1000);c.mem.wr(0x8305a6c8,struct.pack('>I',AUX))
 if compiled:
  for at,n in b.unresolved.items():
   if 'mpAptAuxInst' in n:c.mem.wr(at,struct.pack('>I',AUX))
   else:c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(b.entry)
 else:
  for l in orig:
   if '  bl ' not in l:continue
   p=l.split();n=p[2]
   if n.startswith('__save') or n.startswith('__rest'):continue
   c.hooks[target(int(p[0],16))]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(0x8284b838)
 return c.mem.rd(AUX+0x420,0x80)
rng=random.Random(0x8284b838)
for i in range(256):
 init=bytearray(rng.randbytes(0x500));init[0x480:0x490]=struct.pack('>4f',*[rng.uniform(-10,10) for _ in range(4)]);init[0x490:0x4a0]=struct.pack('>4f',*[rng.uniform(-10,10) for _ in range(4)]);mat=struct.pack('>6f',*[rng.uniform(-100,100) for _ in range(6)]);a=run(False,bytes(init),mat);z=run(True,bytes(init),mat)
 if a!=z:print('diff',[hex(k) for k,(x,y) in enumerate(zip(a,z)) if x!=y]);print(a.hex(),z.hex());raise AssertionError(i)
r={'cases':256,'scope':'ARTIST 8284B838 vs actual compiled SetVertexMatrix; same singleton and real handler offsets, compare full 0x80-byte handler prefix across finite arbitrary platform/origin vectors and input matrices. Valid non-null inputs; original assertion paths not compared.','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'apt-vertex-comparison-wip5.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))

