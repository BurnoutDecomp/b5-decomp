from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][-1]['object']);rp=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-v34-final-retention/compile-results.json').read_text())['results'];prov=Path(next(x['object'] for x in rp if x['source'].endswith('BrnMapUtils.cpp')));name='?Construct@MainMapComponent@BrnGui@@';trans='?Transform@MapTransform@BrnGui@@SA?AUVector2@vpu@math@rw@@U3456@VMatrix33@456@@Z';b=F.Bundle([F.Coff(obj,name),F.Coff(prov,trans)]);b.select(name);rng=random.Random(818)
def locate(fragment):
 return [b.bases[(oi,s)]+v for n,(oi,v,s) in b.definitions.items() if n.startswith(fragment) and (oi,s) in b.bases]
world=locate('?smv4WorldRect@MapTransform');device=locate('?smm33DeviceSpace@MapTransform');assert len(world)==len(device)==1,(world,device)
zoom=locate('?mfZoomScalFactors@MainMapComponent');sd=locate('?mfStandardDefZoomScalFactors@MainMapComponent');assert len(zoom)==len(sd)==1,(zoom,sd)
for i in range(64):
 view=[rng.uniform(-100,100) for _ in range(4)];pad=[rng.uniform(-100,100) for _ in range(4)];dev=[rng.uniform(-5,5) for _ in range(12)];wr=[-1000.,-1000.,1000.,1000.];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x82fb3000,0x1000);c.mem.add(0x82f25000,0x1000);c.mem.add(0x86000000,0x10000);base=F.M.S.SELF;state=F.M.S.AUX;params=state+0x2000;access=state+0x4000;cache=0x86000000;F.put(c,state+4,access);F.put(c,access+0x10,cache);c.mem.wr(cache+0x4b49,b'\0');c.mem.wr(params,struct.pack('>8fI',*(view+pad+[i%3])));c.mem.wr(0x82fb3050,struct.pack('>12f',*dev));c.mem.wr(device[0],struct.pack('>12f',*dev));c.mem.wr(0x82fb31f0,struct.pack('>4f',*wr));c.mem.wr(world[0],struct.pack('>4f',*wr));c.mem.wr(0x82f259dc,struct.pack('>4f',6500.,3500.,2500.,0.));c.mem.wr(0x82f259ec,struct.pack('>4f',5000.,2750.,1000.,0.));c.mem.wr(zoom[0],struct.pack('>4f',6500.,3500.,2500.,0.));c.mem.wr(sd[0],struct.pack('>4f',5000.,2750.,1000.,0.));c.setg(3,base);c.setg(4,state);c.setg(5,params)
  def construct(x):F.put(x,(x.g(3)&0xffffffff)+0x88,x.g(5))
  c.hooks[0x82848bd0]=construct;c.hooks[0x82458590]=lambda x:None
  for a,n in b.unresolved.items():
   if '?GetAccessPointers@StateInterface' in n:c.hooks[a]=lambda x:x.setg(3,int.from_bytes(x.mem.rd((x.g(3)&0xffffffff)+4,4),'big'))
   elif '?GetGuiCache@GuiAccessPointers' in n:c.hooks[a]=lambda x:x.setg(3,int.from_bytes(x.mem.rd((x.g(3)&0xffffffff)+0x10,4),'big'))
   elif '?Construct@GuiComponent' in n:c.hooks[a]=construct
   elif '?Construct@MapManager' in n:c.hooks[a]=lambda x:None
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
   elif n.startswith('__savevmx_') or n.startswith('__restvmx_'):c.hooks[a]=lambda x:None
  c.run(b.entry if native else 0x8245e228);outs.append((c.mem.rd(base+0x610,0x40),c.mem.rd(base+0x660,0x1d),c.mem.rd(zoom[0] if native else 0x82f259dc,16)))
 assert outs[0]==outs[1],(i,[tuple(a.hex() for a in x) for x in outs])
r={'passed':True,'cases':64,'caller_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'provider_object_sha256':hashlib.sha256(prov.read_bytes()).hexdigest(),'scope':'Actual MainMap Construct and real native one-matrix MapTransform provider versus ARTIST 8245E228 in finite random view/padding/device matrices; compares full four rects and zoom/type/cache/flag span and SD zoom-table stores. GuiComponent and MapManager construction controlled, valid SD cache and native VecFloat constructor controlled. V4 assertion provider, exceptional values, full game/runtime excluded.'};(D/'construct-sd-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
