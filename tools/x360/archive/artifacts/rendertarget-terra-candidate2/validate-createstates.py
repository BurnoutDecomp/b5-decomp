from pathlib import Path
import sys,json,struct,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?CreateStates@RenderTarget@postfx@graphics@rw@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);gdepth=next(a for a,n in b.unresolved.items() if '?gOriginalDepthStencilBuffer@renderengine' in n);cases=0
for colour in (0,1,2):
 for depth in (0,1,2,4):
  for sampled in (0,1):
   for hiz in (0,1):
    outs=[]
    for native in (False,True):
     c=F.cpu(b);c.mem.add(0x86000000,0x10000);base=F.M.S.SELF;params=F.M.S.AUX;alloc=0x86000000;vt=alloc+0x100;hook=0x84803f00;F.put(c,alloc,vt);F.put(c,vt+16,hook);F.put(c,base,alloc);F.put(c,base+0x10,colour);F.put(c,base+0x14,depth);F.put(c,base+0x24,0x11111111);F.put(c,base+0x84,0x22222222);F.put(c,base+0x88,0x33333333);F.put(c,base+0x90,0x44444444 if hiz else 0);F.put(c,base+0x8c,0x77777777);F.put(c,base+0x1c,0x88888888);c.mem.wr(params+0x18,bytes([sampled,1]));events=[];counter=[0]
     def descriptor(size):
      def f(x):
       p=x.g(3)&0xffffffff;x.mem.wr(p,b''.join(struct.pack('>II',*z) for z in ([(size,4)]+[(0,1)]*4)));x.setg(3,p)
      return f
     def allocate(x):
      p=x.g(3)&0xffffffff;events.append(('alloc',x.mem.rd(x.g(5)&0xffffffff,40),x.g(6)));res=0x86001000+counter[0]*0x100;counter[0]+=1;x.mem.wr(p,struct.pack('>5I',res,0,0,0,0));x.setg(3,p)
     def initialize(kind):
      def f(x):
       resource=x.mem.rd(x.g(3)&0xffffffff,20);ptr=int.from_bytes(resource[:4],'big');raw=x.mem.rd(x.g(4)&0xffffffff,0x4c if kind=='texturestate' else 0x14)
       if kind=='texturestate':raw=raw[:0x45]+raw[0x48:]
       else:
        words=list(struct.unpack('>5I',raw));words=[0x83271140 if w==gdepth else w for w in words];raw=struct.pack('>5I',*words)
       events.append((kind,resource,raw));x.setg(3,ptr)
      return f
     c.hooks[hook]=allocate
     for a,n in b.unresolved.items():
      if '?GetResourceDescriptor@RenderTargetState' in n:c.hooks[a]=descriptor(0x18)
      elif '?GetResourceDescriptor@TextureState' in n:c.hooks[a]=descriptor(0x20)
      elif '?Initialize@RenderTargetState' in n:c.hooks[a]=initialize('rtstate')
      elif '?Initialize@TextureState' in n:c.hooks[a]=initialize('texturestate')
     for a,f in [(0x82b63678,descriptor(0x18)),(0x82b635c8,descriptor(0x20)),(0x82b62748,initialize('rtstate')),(0x82b62720,initialize('texturestate'))]:c.hooks[a]=f
     c.setg(3,base);c.setg(4,params);c.run(b.entry if native else 0x824037e8);outs.append((events,c.mem.rd(base+0x19,1),c.mem.rd(base+0x1c,4),c.mem.rd(base+0x8c,4)))
    assert outs[0]==outs[1],(colour,depth,sampled,hiz,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual RenderTarget::CreateStates vs ARTIST 824037E8 over colour 0/1/2, depth 0/1/2/4, depth sampling and HiZ choices: compares allocator/resource and state parameter lanes, section count, state pointer and depth sampler +8C. Native device-depth address normalized to original global identity; allocator/descriptor/init providers controlled. Invalid >1 section assertions, real GPU and runtime excluded.'};(D/'createstates-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
