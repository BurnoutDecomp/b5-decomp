from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;r=json.loads((D/'compile-results.json').read_text())['results'][0];obj=Path(r['object']);sym='?CalculateEventZoomFactor@CrashNavMap@BrnGui@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym);statics=[b.bases[(0,s)]+v for n,v,s in o.symbols.values() if 'K_CRASHNAV_' in n and (0,s) in b.bases];base=F.M.S.SELF;cache=0x86000000;world=0x86020000;race=0x86021000;start=0x86022000;recs=0x86023000;rng=random.Random(991)
for i in range(32):
 count=i%4;points=[tuple(rng.uniform(-500,500) for _ in range(4)) for _ in range(4)];outs=[];raw=rng.randbytes(0x7000)
 for native in [False,True]:
  c=F.cpu(b);c.mem.add(cache,0x30000);c.mem.add(0x82fb4000,0x1000);c.mem.wr(base,raw);F.put(c,base+0x48,cache);F.put(c,base+0x609c,42);F.put(c,cache+0x4064,world);F.put(c,world,11);F.put(c,race+0x18,recs);F.put(c,race+0x1c,count);c.mem.wr(start,struct.pack('>4f',*points[0]));rect=struct.pack('>4f',638.0,349.8,0.,0.);c.mem.wr(0x82fb4ac0,rect);c.mem.wr(0x82fb4d10,rect)
  for a in statics:c.mem.wr(a,rect)
  for k in range(count):F.put(c,recs+40*k,k)
  trace=[]
  def getstart(x):x.setg(3,start)
  def getworld(x):x.setg(3,world)
  def getrace(x):x.setg(3,race)
  def getcheck(x):x.setg(3,recs+40*x.g(4))
  def geticon(x):
   idx=x.g(4)&0xffffffff;ptr=x.g(5)&0xffffffff;x.mem.wr(ptr,struct.pack('>4f',*points[idx+1]))
  def zoom(x):trace.append((tuple(x.vr[1]),tuple(x.vr[2]),tuple(x.vr[3]),x.f(1)));x.setf(1,7.0)
  c.hooks.update({0x824f8af0:getstart,0x82501698:getrace,0x825067e0:geticon,0x8244f318:zoom})
  for a,n in b.unresolved.items():
   if 'GetProfileEventDisplayInfo' in n:c.hooks[a]=getstart
   elif 'GetWorldDataController' in n:c.hooks[a]=getworld
   elif 'GetEventInfoFromEventId' in n:c.hooks[a]=getrace
   elif 'GetCheckpointData' in n:c.hooks[a]=getcheck
   elif 'GetLandmarkInfoFromID' in n:c.hooks[a]=geticon
   elif 'CalculateZoomFactor@MapTransform' in n:c.hooks[a]=zoom
  c.setg(3,base);c.run(b.entry if native else 0x824bf4b0);outs.append((c.mem.rd(base+0x6b0,16),trace))
 assert outs[0]==outs[1],(i,count,outs)
report={'passed':True,'cases':32,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual CalculateEventZoomFactor vs ARTIST824BF4B0 over 0-3 checkpoint sets, finite four-lane inputs; complete desired-centre vector and v1/v2/v3/f1 zoom arguments compared; cache/world/zoom services controlled. Reciprocal estimate is emulator model, not Xenon hardware; exceptional floats and full frame excluded.'};(D/'zoom-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
