from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][-1]['object']);name='?CalculatePositionedWorldRect@MainMapComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);world=[(a,n) for a,n in b.unresolved.items() if 'smv4WorldRect@MapTransform' in n];assert len(world)==1,world;rng=random.Random(813)
for i in range(96):
 rect=[-100.,-100.,100.,100.];view=[-40.,-30.,40.,30.];pad=[-35.,-25.,35.,25.];centre=[rng.uniform(-120,120),rng.uniform(-120,120),rng.uniform(-10,10),rng.uniform(-10,10)];mask=i%16;outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x82fb3000,0x1000);c.mem.add(0x8327f200,0x100);
  for lane in range(4):
   for source in range(4):
    maskbytes=bytearray(range(16));maskbytes[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(maskbytes))
  base=F.M.S.SELF;ret=F.M.S.AUX;c.mem.wr(base+0x610,struct.pack('>4f',*view));c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x630,struct.pack('>4f',*pad));c.mem.wr(0x82fb31f0,struct.pack('>4f',*rect));c.mem.wr(world[0][0],struct.pack('>4f',*rect));c.mem.wr(base+0x678,bytes((mask>>k)&1 for k in range(4)));c.mem.wr(ret,b'\xaa'*16);c.setg(3,ret);c.setg(4,base);c.vr[1]=struct.unpack('>4I',struct.pack('>4f',*centre))
  c.hooks[0x8245a080]=lambda x: None
  for a,n in b.unresolved.items():
   if '?Transform@MapTransform' in n:c.hooks[a]=lambda x:None
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
  c.run(b.entry if native else 0x8245e5f0);outs.append((c.mem.rd(base+0x610,16),c.mem.rd(ret,16)))
 assert outs[0]==outs[1],(i,mask,centre,[tuple(t.hex() for t in x) for x in outs])
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Whole actual CalculatePositionedWorldRect vs ARTIST 8245E5F0; all 16 edge-flag combinations and random finite centres, compares full world rectangle and result. Rect-pair Transform controlled as identity; V4 Magnitude remains unverified and assertions not exercised. No runtime or hardware claim.'};(D/'positioned-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
