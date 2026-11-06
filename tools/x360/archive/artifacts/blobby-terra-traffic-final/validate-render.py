from pathlib import Path
import json,random,sys,hashlib,struct
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][1]['object']);sym='?Render@BrnBlobbyShadowManager@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym);names={}
for n,v,s in o.symbols.values():
 if (0,s) in b.bases:
  if n.startswith('?lfMaxOpacity@'):names['opacity']=b.bases[(0,s)]+v
  elif n.startswith('?$S1@') and 'Render@BrnBlobbyShadowManager' in n:names['flag']=b.bases[(0,s)]+v
  elif n in ('kfAmbientShadowFadeDistance','kfAmbientShadowScaleFactor'):names[n]=b.bases[(0,s)]+v
assert len(names)==4,names;rng=random.Random(18)
for i in range(96):
 records=[[rng.uniform(-10,10) for _ in range(4)] for j in range(4)];records[2][3]=[-0.5,0.,0.625,1.25,1.5,rng.uniform(0.,1.25)][i%6];alpha=[0.0,.3,.7,1.0][i%4];white=[0.0,.2,.9,1.0][(i//4)%4];outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x82fae000,0x3000);c.mem.add(0x83010000,0x1000);base=F.M.S.SELF;matrix=F.M.S.AUX;im3d=matrix+0x1000;texture=0x56781234;F.put(c,base,1);c.mem.wr(base+0x10,struct.pack('>16f',*(records[0]+records[1]+records[2]+records[3])));c.mem.wr(base+0x2020,b'\0');c.mem.wr(matrix,bytes(64));fade=struct.pack('>4f',*[1.25]*4);factor=struct.pack('>4f',*[.4]*4);c.mem.wr(0x82faf0e0,fade);c.mem.wr(0x82fafbf0,factor);c.mem.wr(names['kfAmbientShadowFadeDistance'],fade);c.mem.wr(names['kfAmbientShadowScaleFactor'],factor);F.put(c,0x82fb0060,0);F.put(c,names['flag'],0)
  F.put(c,0x83010f9c,im3d+4);F.put(c,0x83010914,0x11111111);F.put(c,0x83010a40,0x22222222);F.put(c,0x83010f74,0x33333333);trace=[]
  def begin(x):trace.append(('begin',x.g(3)))
  def transform(x):trace.append(('transform',x.g(3),x.mem.rd(x.g(4)&0xffffffff,64)))
  def tex(x):trace.append(('tex',x.g(3),x.g(4)))
  def state(x):trace.append(('state',x.g(3),x.g(4)))
  def render(x):
   ptr=x.g(5)&0xffffffff;data=x.mem.rd(ptr,256);trace.append(('render',x.g(3),x.g(4),x.g(6),b''.join(data[j*32:j*32+28] for j in range(8))))
  def end(x):pass
  for a,f in [(0x8227b730,begin),(0x8227b8a0,transform),(0x82276ee8,tex),(0x82276da8,state),(0x82276e48,state),(0x82276d08,state),(0x824041b8,render)]:c.hooks[a]=f
  # Resolve original EndRendering from X360 disassembly as needed.
  for a,n in b.unresolved.items():
   if n.startswith('__savefpr') or n.startswith('__restfpr'):c.hooks[a]=lambda x:None
   elif '?BeginRendering@' in n:c.hooks[a]=begin
   elif '?SetTransform@' in n:c.hooks[a]=transform
   elif '?SetTexture@' in n:c.hooks[a]=tex
   elif '?SetState@' in n:c.hooks[a]=state
   elif '?Render@?$ImRenderer' in n:c.hooks[a]=render
   elif '?EndRendering@' in n:c.hooks[a]=end
   elif '?saDepthStencilStates' in n:F.put(c,a+8,0x11111111)
   elif '?saRasterizerStates' in n:F.put(c,a+8,0x22222222)
   elif '?saBlendStates' in n:F.put(c,a+4,0x33333333)
  c.setg(3,base);c.setg(4,matrix);c.setg(5,im3d);c.setg(6,texture);c.setf(1,alpha);c.setf(2,white);
  try:c.run(b.entry if native else 0x824071b0)
  except Exception:print('case',i,'native',native,'pc',hex(c.last_pc));raise
  outs.append(trace)
 assert outs[0]==outs[1],(i,outs)
out={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual whole Render versus ARTIST 824071B0, one finite shadow record, fade clamp boundaries and multiple alpha/white levels; compares all 8 vertex position/colour/UV bytes and controlled Begin/transform/texture/state/render call traces. Im3d/GPU methods controlled; native EndRendering handled as no-op versus original inline shadow cleanup, no global state comparison, physical GPU or full runtime claim.'};(D/'render-controlled-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
