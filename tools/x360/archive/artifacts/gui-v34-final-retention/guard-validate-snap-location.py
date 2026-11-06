from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
class GuardCPU(F.AtomicCPU):
 def exec_vector(self,pc,w,mn):
  pairs = [(b.entry+x,b.entry+y) for x,y in NATIVE] if self.guard_native else ORIG
  for begin,end in pairs:
   if pc==begin:
    va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6);self.guards.append(('input',tuple(self.vr[va])))
   if pc==end:
    r=super().exec_vector(pc,w,mn);self.guards.append(('result',tuple(self.vr[(w>>21)&31])));return r
  return super().exec_vector(pc,w,mn)
ORIG=[(2185620808, 2185620908)]
NATIVE=[(348, 416)]
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][-1]['object']);name='?SnapToLocation@MainMapComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);rng=random.Random(992)
for i in range(96):
 point=[rng.uniform(-100,100) for _ in range(4)];scale=rng.uniform(100,1000);view=[-100.,-50.,100.,50.];outs=[];guard_outputs=[]
 for native in (False,True):
  c=F.cpu(b);c.__class__=GuardCPU;c.guard_native=native;c.guards=[];c.mem.add(0x8327f200,0x100);c.mem.add(0x82fb3000,0x1000)
  for lane in range(4):
   for source in range(4):
    mask=bytearray(range(16));mask[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(mask))
  base=F.M.S.SELF;c.mem.wr(0x82fb31f0,struct.pack('>4f',-10000.,-10000.,10000.,10000.));c.mem.wr(0x82fb3660,struct.pack('>4f',-1.,-1.,1.,1.));c.mem.wr(base+0x610,struct.pack('>4f',*view));c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x630,struct.pack('>4f',*view));c.mem.wr(base+0x660,struct.pack('>f',scale));c.mem.wr(base+0x678,bytes([(i>>k)&1 for k in range(4)]));F.put(c,base+0x674,0x1234);c.mem.wr(base+0x670,b'\1');c.setg(3,base);c.vr[1]=struct.unpack('>4I',struct.pack('>4f',*point));c.hooks[0x8245a080]=lambda x:None
  for a,n in b.unresolved.items():
   if 'smv4WorldRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-10000.,-10000.,10000.,10000.))
   elif 'smv4NormalizedRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-1.,-1.,1.,1.))
   elif '?Transform@MapTransform' in n:c.hooks[a]=lambda x:None
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
   elif n.startswith('__savevmx_') or n.startswith('__restvmx_'):c.hooks[a]=lambda x:None
  c.run(b.entry if native else 0x8245eba0);guard_outputs.append(c.guards);outs.append((c.mem.rd(base+0x610,16),c.mem.rd(base+0x650,16),c.mem.rd(base+0x670,1)))
 assert guard_outputs[0]==guard_outputs[1],(i,guard_outputs)
 assert len(guard_outputs[0]) == 2, (i,guard_outputs)
 assert outs[0]==outs[1],(i,outs)
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual SnapToLocation vs ARTIST 8245EBA0 with finite random four-lane inputs and all 16 edge masks; compares world rect, desired centre and zoom flag. Rect transforms identity, original insert masks inferred, native VecFloat constructor controlled; V4 Magnitude and assertions, exceptions, physical hardware and runtime excluded.'};(D/'guard-snap-location-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
