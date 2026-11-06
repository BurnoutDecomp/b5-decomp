from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][-1]['object']);name='?Update@MainMapComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);rng=random.Random(888)
for i in range(64):
 centre=[rng.uniform(-200,200) for _ in range(4)];desired=[rng.uniform(-200,200) for _ in range(4)];scale=rng.uniform(100,1000);target=scale;view=[-100.,-50.,100.,50.];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x8327f200,0x100);c.mem.add(0x82fb3000,0x1000)
  for lane in range(4):
   for source in range(4):
    mask=bytearray(range(16));mask[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(mask))
  base=F.M.S.SELF;ret=F.M.S.AUX;c.mem.wr(0x82fb31f0,struct.pack('>4f',-10000.,-10000.,10000.,10000.));c.mem.wr(0x82fb3660,struct.pack('>4f',-1.,-1.,1.,1.));c.mem.wr(base+0x610,struct.pack('>4f',*view));c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x630,struct.pack('>4f',*view));c.mem.wr(base+0x650,struct.pack('>4f',*desired));c.mem.wr(base+0x660,struct.pack('>f',scale));c.mem.wr(base+0x664,struct.pack('>f',target));c.mem.wr(base+0x678,bytes([(i>>k)&1 for k in range(4)]));F.put(c,base+0x674,0x1234);F.put(c,base+0x668,1);c.mem.wr(base+0x5f0,b'\1');F.put(c,base+0x88,0x12345678);c.mem.wr(base+0x67c,b'\1');c.mem.wr(ret,b'\xaa'*16);c.setg(3,ret);c.setg(4,base);c.vr[1]=struct.unpack('>4I',struct.pack('>4f',*centre));trace=[]
  def worldhook(x):trace.append(('world',tuple(x.vr[1]),tuple(x.vr[2]),tuple(x.vr[3])))
  def viewhook(x):trace.append(('view',tuple(x.vr[1])))
  def tilehook(x):trace.append(('tile',x.g(3)))
  def refreshhook(x):trace.append(('refresh',x.g(3)))
  def eventhook(x):trace.append(('event',x.g(3),x.mem.rd(x.g(4)&0xffffffff,45)))
  c.hooks[0x8244fa80]=tilehook;c.hooks[0x82448540]=refreshhook;c.hooks[0x82465e50]=eventhook;c.hooks[0x8245a080]=lambda x:None;c.hooks[0x8244f768]=lambda x:None;c.hooks[0x824504e8]=worldhook;c.hooks[0x82450608]=viewhook
  for n,(oi,v,sec) in b.definitions.items():
   if 'OutputViewState@UGuiEventRenderMainMap' in n and (oi,sec) in b.bases:c.hooks[b.bases[(oi,sec)]+v]=eventhook
  for a,n in b.unresolved.items():
   if '?CalculateCurrentTileSet@MapManager' in n:c.hooks[a]=tilehook
   elif '?RefreshActiveTextureArray@MapManager' in n:c.hooks[a]=refreshhook
   elif 'smv4WorldRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-10000.,-10000.,10000.,10000.))
   elif 'smv4NormalizedRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-1.,-1.,1.,1.))
   elif '?Transform@MapTransform' in n or '?SetZoomLevel@MapManager' in n:c.hooks[a]=lambda x:None
   elif '?SetZoomedWorldRect@MapTransform' in n:c.hooks[a]=worldhook
   elif '?SetZoomedViewportRect@MapTransform' in n:c.hooks[a]=viewhook
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
   elif n.startswith('__savevmx_') or n.startswith('__restvmx_'):c.hooks[a]=lambda x:None
  c.run(b.entry if native else 0x824696e8);outs.append((c.mem.rd(ret,16),c.mem.rd(base+0x610,16),c.mem.rd(base+0x8c,16),c.mem.rd(base+0x660,8),c.mem.rd(base+0x670,1),trace))
 assert outs[0]==outs[1],(i,outs)
r={'passed':True,'cases':64,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual MainMap Update vs ARTIST 824696E8 with map manager enabled and zoom settled, all edge masks; compares full return/world rect/tile rect/scales/flag and tile/refresh/event and zoomed-world/viewport service traces, including event bytes. Rect Transform identity, zoom services and native VecFloat constructor controlled; original lane-insert table inferred. V4 Magnitude and asserts, exceptional inputs and real runtime/hardware excluded.'};(D/'update-settled-enabled-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)

