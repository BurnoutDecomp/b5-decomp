from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?MoveCursor@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;cache=0x86000000;manager=0x86100000;rng=random.Random(147)
for i in range(64):
 cursor=[rng.uniform(-300,300) for _ in range(4)];delta=[0.,50.,49.999,50.001][i%4];point=[cursor[0]+delta,cursor[1],rng.uniform(-300,300),rng.uniform(-300,300)];raw=rng.randbytes(0x6200);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(cache,0x20000);c.mem.add(manager,0x1000);c.mem.wr(base,raw);F.put(c,base+0x48,cache);F.put(c,base+0x4c,manager);F.put(c,base+0x5f50,4);c.mem.wr(base+0x6150,struct.pack('>f',0.));c.mem.wr(base+0x5f00,struct.pack('>4f',*cursor));F.put(c,aux,0);c.mem.wr(aux+4,struct.pack('>2f',0.,0.));trace=[]
  def tm(x):x.setf(1,1.0)
  def icons(x):x.mem.wr(x.g(4)&0xffffffff,struct.pack('>4f',*point));F.put(x,x.g(5)&0xffffffff,1);trace.append(('icons',))
  def nearest(x):trace.append(('nearest',x.g(5)&0xffffffff));x.setg(3,0)
  c.hooks.update({0x8240f0b0:tm,0x8250a708:icons,0x824168b8:nearest})
  for a,n in b.unresolved.items():
   if 'GetTime@GuiCache' in n:c.hooks[a]=tm
   elif 'GetSatNavIconPositions@MapIconManager' in n:c.hooks[a]=icons
   elif 'FindClosestSnapIndex@GuiCursor' in n:c.hooks[a]=nearest
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x824bf100);outs.append((c.mem.rd(base,0x6200),trace))
 assert outs[0]==outs[1],(i,delta,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':64,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual MoveCursor idle panning/snap threshold vs ARTIST824BF3CC..4A0; exact50 threshold and adjacent values plus varied z/w, finite inputs; timer, icon-list and nearest-index services controlled. Other modes, NaN/denormals and full runtime excluded.'};(D/'snap-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
