from pathlib import Path
import sys,json,random,struct,hashlib
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sphere13-line-abi');q=out/'2.obj';l=out/'0.obj';qn='?RunQuery@PolygonSoupListSpatialMap@CgsGeometric@@QAAHABULine@2@@Z';ln='?TestLineStartEndAxisAlignedBox@CgsGeometric@@';b=F.Bundle([F.Coff(q,qn),F.Coff(l,ln)]);b.select(qn);A=0x40000000;rng=random.Random(0x82843E98);bad=[]
def wr32(c,at,x):c.mem.wr(at,x.to_bytes(4,'big'))
def run(comp,lo,hi,v1,v2):
 c=F.cpu(b);c.mem.add(A,0x3000);c.mem.wr(A,b'\x00'*0x80);wr32(c,A+8,A+0x400);wr32(c,A+0x50,A+0x800);wr32(c,A+0x54,A+0x900);wr32(c,A+0x5c,1);wr32(c,A+0x60,8);c.mem.wr(A+0x400,struct.pack('>8f',*lo,*hi));wr32(c,A+0x420,A+0xA00);c.mem.wr(A+0x424,(3).to_bytes(2,'big'));c.mem.wr(A+0xA00,struct.pack('>3H',3,7,11));c.mem.wr(A+0x800,b'\xA5'*32);c.mem.wr(A+0x900,b'\xA5'*32);c.mem.wr(A+0xB00,struct.pack('>8f',*v1,*v2));fires=[]
 def fire(cpu):fires.append(1)
 for n,a in [('BeginAssert',0x82817548),('FireAssert',0x82820810),('EndAssert',0x82817558)]:
  f=fire if n=='FireAssert' else lambda cpu:None;c.hooks[a]=f
  for addr,sym in b.unresolved.items():
   if n in sym:c.hooks[addr]=f
 for addr,sym in b.unresolved.items():
  if sym.startswith('__save') or sym.startswith('__rest'):c.hooks[addr]=lambda cpu:None
 c.setg(3,A);c.setg(4,A+0xB00);c.run(b.entry if comp else 0x82843E98);return c.g(3)&0xffffffff,c.mem.rd(A+0x58,4).hex(),c.mem.rd(A+0x64,4).hex(),c.mem.rd(A+0x800,32).hex(),c.mem.rd(A+0x900,32).hex(),len(fires)
for i in range(128):
 lo=[-3.,-2.,-1.,rng.uniform(-5,5)];hi=[3.,2.,1.,rng.uniform(-5,5)];x=[rng.uniform(-8,8) for k in range(4)];y=[rng.uniform(-8,8) for k in range(4)];a=run(False,lo,hi,x,y);z=run(True,lo,hi,x,y)
 if a!=z:bad.append({'case':i,'original':a,'compiled':z})
r={'cases':128,'mismatches':bad,'scope':'actual original PolygonSoupListSpatialMap::RunQuery(Line)82843E98 with native inlined slab test vs actual compiled caller plus actual compiled MaskScalar provider; one level/one node with three indices, finite randomized endpoint/W values; compares return, output buffer, count, scratch bytes and assert count. VMX reciprocal estimate emulator; no multi-level/full scene/NaN/hardware claim','query_object_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'line_object_sha256':hashlib.sha256(l.read_bytes()).hexdigest()};(out/'line-query-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('cases',128,'mismatches',len(bad),bad[:1]);assert not bad
