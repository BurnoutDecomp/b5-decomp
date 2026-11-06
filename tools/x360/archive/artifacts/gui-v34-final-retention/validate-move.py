from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=next(x for x in json.loads((D/'compile-results.json').read_text())['results'] if x['source'].endswith('BrnCrashNavMap.cpp'));obj=Path(r['object']);sym='?MoveCursor@CrashNavMap@BrnGui@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);base=F.M.S.SELF;aux=F.M.S.AUX;cache=0x86000000;rng=random.Random(702)
def bits(v):return struct.unpack('>4I',struct.pack('>4f',*v))
for i in range(32):
 axisx=0.0 if i%4==0 else rng.uniform(-1,1);axisy=0.0 if i%4==0 else rng.uniform(-1,1);centre=[rng.uniform(-100,100) for _ in range(4)];cursor=[rng.uniform(-100,100) for _ in range(4)];device=bits([rng.uniform(-100,100) for _ in range(4)]);world=bits([rng.uniform(-250,250),rng.uniform(-20,20),rng.uniform(-250,250),rng.uniform(-20,20)]);raw=rng.randbytes(0x6200);outs=[]
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(cache,0x20000);c.mem.add(0x82fb3000,0x1000);rect=struct.pack('>4f',-100,-200,100,200);c.mem.wr(0x82fb31f0,rect);c.mem.wr(base,raw);F.put(c,base+0x48,cache);F.put(c,base+0x5f50,4);c.mem.wr(base+0x60f0,struct.pack('>4f',*centre));c.mem.wr(base+0x5f00,struct.pack('>4f',*cursor));F.put(c,aux,1);c.mem.wr(aux+4,struct.pack('>2f',axisx,axisy));trace=[]
  def tm(x):x.setf(1,100.0)
  def buttons(x):trace.append(('buttons',))
  def unflat(x):v=x.vr[1];x.vr[1]=(v[0],0,v[1],0)
  def wtd(x):trace.append(('wtd',tuple(x.vr[1]),x.g(3)&0xffffffff));x.vr[1]=device
  def dtw(x):trace.append(('dtw',tuple(x.vr[1])));x.vr[1]=world
  def pos(x):trace.append(('pos',x.g(3)&0xffffffff,x.g(4)&0xffffffff,tuple(x.vr[1])))
  c.hooks.update({0x8240f0b0:tm,0x824b68b8:buttons,0x82428878:wtd,0x824bab78:dtw,0x82428ad8:pos})
  try:c.hooks[F.address(b,'?UpdateButtonPrompts@CrashNavMap@BrnGui@@')]=buttons
  except AssertionError:pass
  for a,n in b.unresolved.items():
   if 'GetTime@GuiCache' in n:c.hooks[a]=tm
   elif 'Unflatten@MapTransform' in n:c.hooks[a]=unflat
   elif 'WorldToDevice@MapTransform' in n:c.hooks[a]=wtd
   elif 'DeviceToWorld@MapTransform' in n:c.hooks[a]=dtw
   elif 'SetPosition@GuiCursor' in n:c.hooks[a]=pos
   elif 'smv4WorldRect@MapTransform' in n:c.mem.wr(a,rect)
  c.setg(3,base);c.setg(4,aux);c.run(b.entry if native else 0x824bf100);outs.append((c.mem.rd(base,0x6200),trace))
 assert outs[0]==outs[1],(i,outs[0][1],outs[1][1],[(hex(k),a,z) for k,(a,z) in enumerate(zip(outs[0][0],outs[1][0])) if a!=z][:10])
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual MoveCursor right-stick panning vs ARTIST824BF100; finite inputs with all four centre/cursor/device/world lanes varied. Transform, time, button, cursor services controlled; full object and service-register traces compared. Other input modes, hardware estimates, exceptional float and full GUI runtime excluded.'};(D/'move-cursor-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
