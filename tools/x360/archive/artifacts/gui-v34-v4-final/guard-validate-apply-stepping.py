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
ORIG=[(2185622544, 2185622616)]
NATIVE=[(1208, 1268)]
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);name='?ApplyZoom@MainMapComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);rng=random.Random(616)
for i in range(96):
 centre=[rng.uniform(-200,200) for _ in range(4)];desired=[rng.uniform(-200,200) for _ in range(4)];scale=rng.uniform(1000,4000);target=scale+(200 if i%2 else -200);view=[-100.,-50.,100.,50.];outs=[];guard_outputs=[]
 for native in (False,True):
  c=F.cpu(b);c.__class__=GuardCPU;c.guard_native=native;c.guards=[];c.mem.add(0x8327f200,0x100)
  for lane in range(4):
   for source in range(4):
    mask=bytearray(range(16));mask[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(mask))
  base=F.M.S.SELF;ret=F.M.S.AUX;c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x650,struct.pack('>4f',*desired));c.mem.wr(base+0x660,struct.pack('>f',scale));c.mem.wr(base+0x664,struct.pack('>f',target));c.mem.wr(ret,b'\xaa'*16);c.setg(3,ret);c.setg(4,base);c.vr[1]=struct.unpack('>4I',struct.pack('>4f',*centre));c.setf(1,1.7777778)
  for a,n in b.unresolved.items():
   if n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
   elif n.startswith('__savevmx_') or n.startswith('__restvmx_'):c.hooks[a]=lambda x:None
  c.run(b.entry if native else 0x8245ee78);guard_outputs.append(c.guards);outs.append((c.mem.rd(ret,16),c.mem.rd(base+0x610,16),c.mem.rd(base+0x660,8),c.mem.rd(base+0x670,1)))
 assert guard_outputs[0]==guard_outputs[1],(i,guard_outputs)
 assert len(guard_outputs[0]) == 2, (i,guard_outputs)
 assert outs[0]==outs[1],(i,[tuple(t.hex() for t in x) for x in outs])
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual ApplyZoom stepping arm versus ARTIST 8245EE78; finite random centres, both zoom directions, compares full result/world rect/scales/zoom flag. Inferred original lane-insert table and native VecFloat constructor controlled; V4 Magnitude assertion provider is unverified. Settled branch, exceptional values, real hardware and full runtime excluded.'};(D/'guard-apply-zoom-stepping-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
