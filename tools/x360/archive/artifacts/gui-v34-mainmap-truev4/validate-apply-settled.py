from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);name='?ApplyZoom@MainMapComponent@BrnGui@@';b=F.Bundle([F.Coff(obj,name)]);b.select(name);rng=random.Random(616)
for i in range(96):
 centre=[rng.uniform(-200,200) for _ in range(4)];desired=[rng.uniform(-200,200) for _ in range(4)];scale=rng.uniform(100,1000);target=scale;view=[-100.,-50.,100.,50.];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x8327f200,0x100);c.mem.add(0x82fb3000,0x1000);c.mem.wr(0x82fb31f0,struct.pack('>4f',-10000.,-10000.,10000.,10000.));c.mem.wr(0x82fb3660,struct.pack('>4f',-1.,-1.,1.,1.))
  for lane in range(4):
   for source in range(4):
    mask=bytearray(range(16));mask[4*lane:4*lane+4]=bytes(range(16+4*source,20+4*source));c.mem.wr(0x8327f140+lane*64+source*16,bytes(mask))
  base=F.M.S.SELF;ret=F.M.S.AUX;c.mem.wr(base+0x610,struct.pack('>4f',*view));c.mem.wr(base+0x620,struct.pack('>4f',*view));c.mem.wr(base+0x630,struct.pack('>4f',*view));c.mem.wr(base+0x678,bytes([i%2,(i>>1)%2,(i>>2)%2,(i>>3)%2]));F.put(c,base+0x668,1);c.mem.wr(base+0x650,struct.pack('>4f',*desired));c.mem.wr(base+0x660,struct.pack('>f',scale));c.mem.wr(base+0x664,struct.pack('>f',target));c.mem.wr(ret,b'\xaa'*16);c.setg(3,ret);c.setg(4,base);c.vr[1]=struct.unpack('>4I',struct.pack('>4f',*centre));c.setf(1,1.7777778)
  c.hooks[0x8245a080]=lambda x:None;c.hooks[0x8244f768]=lambda x:None
  for a,n in b.unresolved.items():
   if 'smv4WorldRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-10000.,-10000.,10000.,10000.))
   elif 'smv4NormalizedRect@MapTransform' in n:c.mem.wr(a,struct.pack('>4f',-1.,-1.,1.,1.))
   elif '?Transform@MapTransform' in n or '?SetZoomLevel@MapManager' in n:c.hooks[a]=lambda x:None
   elif n.startswith('??0VecFloat@'):c.hooks[a]=lambda x:x.mem.wr(x.g(3)&0xffffffff,x.mem.rd(x.g(4)&0xffffffff,4)*4)
   elif n.startswith('__savevmx_') or n.startswith('__restvmx_'):c.hooks[a]=lambda x:None
  
  try:c.run(b.entry if native else 0x8245ee78)
  except Exception as e:print('case',i,'native',native,'pc',hex(c.last_pc));raise
  outs.append((c.mem.rd(ret,16),c.mem.rd(base+0x610,16),c.mem.rd(base+0x660,8),c.mem.rd(base+0x670,1)))
 assert outs[0]==outs[1],(i,[tuple(t.hex() for t in x) for x in outs])
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual ApplyZoom settled arm versus ARTIST 8245EE78; finite random centres, all edge masks, compares full result/world rect/scales/zoom flag. Inferred original lane-insert table and native VecFloat constructor controlled; V4 Magnitude assertion provider is unverified. Other branches, exceptional values, real hardware and full runtime excluded.'};(D/'apply-zoom-settled-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)

