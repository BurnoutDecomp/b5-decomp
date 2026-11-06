from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);name='?SetZoom@MainMapComponent@BrnGui@@';o=F.Coff(obj,name);b=F.Bundle([o]);b.select(name);zoom=[b.bases[(0,s)]+v for n,v,s in o.symbols.values() if n.startswith('?mfZoomScalFactors@MainMapComponent') and (0,s) in b.bases];assert len(zoom)==1,zoom;rng=random.Random(99)
for i in range(96):
 kind=3;immediate=(i//3)%2;custom=[-100.,0.,100.][i%3];scale=rng.uniform(100,1000);view=[-100.,-50.,100.,50.];table=[6500.,3500.,2500.,0.];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x8327f200,0x100);c.mem.add(0x82f25000,0x1000)
  for lane in range(4):
   for source in range(4):
    mask=bytearray(range(16));mask[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(mask))
  base=F.M.S.SELF;c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x660,struct.pack('>f',scale));F.put(c,base+0x674,0x1234);c.mem.wr(0x82f259dc,struct.pack('>4f',*table));c.mem.wr(0x82f25ad4,struct.pack('>f',1.7777778));c.mem.wr(zoom[0],struct.pack('>4f',*table));c.setg(3,base);c.setg(4,kind);c.setg(6,immediate);c.setf(1,custom);F.put(c,base+0x88,0x12345678);trace=[]
  def zstr(x,a):
   return bytes(x.mem.rd(a+j,1)[0] for j in range(64) if x.mem.rd(a+j,1)[0]!=0)
  def audio(x):
   def readstr(a):
    data=bytearray()
    for j in range(128):
     q=x.mem.rd(a+j,1)[0]
     if not q:break
     data.append(q)
    return bytes(data)
   trace.append(("construct",x.g(4),readstr(x.g(5)&0xffffffff),readstr(x.g(6)&0xffffffff),readstr(x.g(7)&0xffffffff)))
  def output(x):trace.append(("output",x.g(3)))
  c.hooks[0x824f6350]=audio;c.hooks[0x82436890]=output
  for a,n in b.unresolved.items():
   if '?Construct@GuiAudioTriggerEvent' in n:c.hooks[a]=audio
   elif 'OutputGuiEvent@UGuiAudioTriggerEvent' in n:c.hooks[a]=output
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
  c.run(b.entry if native else 0x82469a38);outs.append((c.mem.rd(base+0x610,16),c.mem.rd(base+0x660,12),c.mem.rd(zoom[0] if native else 0x82f259dc,16),trace))
 assert outs[0]==outs[1],(i,outs)
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual SetZoom vs ARTIST 82469A38 for custom mode with less/equal/greater values and immediate/deferred paths; compares world rect, both scale fields, mode, zoom table and audio constructor/output trace. Original lane-insert table inferred, native VecFloat constructor controlled, inputs below V4 assertion threshold. V4 Magnitude behavior, exceptions/hardware/runtime excluded.'};(D/'setzoom-custom-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
