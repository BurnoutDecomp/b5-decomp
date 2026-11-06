from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;results=json.loads((D/'compile-results.json').read_text())['results'];obj=Path(next(x['object'] for x in results if x['source'].endswith('BrnMapIconManager.cpp')));name='?GetSatNavIconPositions@MapIconManager@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);rng=random.Random(333)
for i in range(96):
 bits=tuple(rng.getrandbits(32) for _ in range(4));outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x86000000,0x10000);base=F.M.S.SELF;out=F.M.S.AUX;count=out+0x100;cache=0x86000000;F.put(c,base+0xa9f8,cache);F.put(c,base+0xa9f0,5);c.mem.wr(base+0x7080,b'\1');c.mem.wr(cache+0x200,struct.pack('>4I',*bits));c.setg(3,base);c.setg(4,out);c.setg(5,count);trace=[]
  def world(x):trace.append((x.g(3),tuple(x.vr[1])))
  for n,(oi,v,sec) in b.definitions.items():
   if (oi,sec) in b.bases and n.startswith('?GetDriveThroughAndJunkyardCount@MapIconManager'):c.hooks[b.bases[(oi,sec)]+v]=lambda x:x.setg(3,1)
   elif (oi,sec) in b.bases and n.startswith('?GetDriveThroughOrJunkyardAtIndex@MapIconManager'):c.hooks[b.bases[(oi,sec)]+v]=lambda x:x.setg(3,cache+0x200)
  c.hooks[0x82428878]=world;c.hooks[0x824f4888]=lambda x:x.setg(3,1);c.hooks[0x824fac10]=lambda x:x.setg(3,cache+0x200)
  for a,n in b.unresolved.items():
   if '?WorldToDevice@MapTransform' in n:c.hooks[a]=world
   elif '?GetDriveThroughAndJunkyardCount@MapIconManager' in n:c.hooks[a]=lambda x:x.setg(3,1)
   elif '?GetDriveThroughOrJunkyardAtIndex@MapIconManager' in n:c.hooks[a]=lambda x:x.setg(3,cache+0x200)
   elif '?GetWorldCameraPosition@GuiCache' in n:c.hooks[a]=lambda x:x.setg(3,cache+0x4ae0)
  c.run(b.entry if native else 0x8250a708);outs.append((c.mem.rd(out,16),c.mem.rd(count,4),trace))
 assert outs[0]==outs[1],(i,bits,outs)
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Whole actual GetSatNavIconPositions drive-through-only path vs ARTIST 8250A708: 96 arbitrary 128-bit patterns (including possible NaN encodings), compares live WorldToDevice v1/r3, written full output and count. Count/index and WorldToDevice controlled; other icon categories and actual map service excluded.'};(D/'drive-position-call-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
