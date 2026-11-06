from pathlib import Path
import sys,json,struct,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='??0RenderTarget@postfx@graphics@rw@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);gdepth=next(a for a,n in b.unresolved.items() if '?gOriginalDepthStencilBuffer@renderengine' in n);gcolour=next(a for a,n in b.unresolved.items() if '?gOriginalColorBuffer@renderengine' in n);cases=0
for colour in (0,1,2):
 for depth in (0,1,2,4):
  for sampled in (0,1):
   for use_default in (0,1):
    outs=[]
    for native in (False,True):
     c=F.cpu(b);c.mem.add(0x86000000,0x10000);base=F.M.S.SELF;params=F.M.S.AUX;alloc=0x86000000;vt=alloc+0x100;hook=0x84803f00;provided=0x86000900;F.put(c,alloc,vt);F.put(c,vt+16,hook);F.put(c,params,0 if use_default else alloc);F.put(c,params+4,320);F.put(c,params+8,240);F.put(c,params+0x10,colour);F.put(c,params+0x14,depth);c.mem.wr(params+0x18,bytes([sampled,1]));
     for off in (0x1c,0x13c):
      F.put(c,params+off+0x10,0x18280186);F.put(c,params+off+0x14,0x18280186);F.put(c,params+off+0x18,1);F.put(c,params+off+0x1c,2);F.put(c,params+off+0x20,1);F.put(c,params+off+0x28,9);F.put(c,params+off+0x2c,7)
     c.mem.wr(provided,struct.pack('>6I',0x11111111,0x22222222,0x33333333,0x44444444,0x55555555,0x66666666));F.put(c,params+0x194,provided);c.mem.wr(base,b'\xa5'*0x98);events=[];counter=[0]
     def descriptor(size):
      def f(x):
       p=x.g(3)&0xffffffff;x.mem.wr(p,b''.join(struct.pack('>II',*z) for z in ([(size,4)]+[(0,1)]*4)));x.setg(3,p)
      return f
     def allocate(x):
      p=x.g(3)&0xffffffff;events.append(('alloc',x.mem.rd(x.g(5)&0xffffffff,40),x.g(6)));res=0x86001000+counter[0]*0x100;counter[0]+=1;x.mem.wr(p,struct.pack('>5I',res,0,0,0,0));x.setg(3,p)
     def initialize(kind):
      def f(x):
       resource=x.mem.rd(x.g(3)&0xffffffff,20);ptr=int.from_bytes(resource[:4],'big');raw=x.mem.rd(x.g(4)&0xffffffff,0x4c if kind=='texturestate' else (0x14 if kind=='rtstate' else 0x1c))
       if kind=='texturestate':raw=raw[:0x45]+raw[0x48:]
       elif kind=='rtstate':
        words=list(struct.unpack('>5I',raw));words=[0x83271140 if w==gdepth else (0x83271100 if w==gcolour else w) for w in words];raw=struct.pack('>5I',*words)
       events.append((kind,resource,raw));
       if kind=='texture':F.put(x,ptr+0x20,0xabcde000|(ptr&0xfff))
       x.setg(3,ptr)
      return f
     def setbase(kind):
      def f(x):events.append((kind,x.g(3)&0xffffffff,x.g(4)&0xffffffff))
      return f
     def check(x):events.append(('check',x.g(3)&0xffffffff,x.mem.rd((x.g(3)&0xffffffff)+0x20,4)))
     def copy(x):
      dst=x.g(3)&0xffffffff;src=x.g(4)&0xffffffff;count=x.g(5)&0xffffffff;x.mem.wr(dst,x.mem.rd(src,count));x.setg(3,dst)
     c.hooks[hook]=allocate;c.hooks[0x82bbd1e0]=lambda x:x.setg(3,alloc);c.hooks[0x82c0a630]=copy
     for a,n in b.unresolved.items():
      if '?GetResourceDescriptor@Texture@renderengine' in n:c.hooks[a]=descriptor(0x34)
      elif '?GetResourceDescriptor@TextureState' in n:c.hooks[a]=descriptor(0x20)
      elif '?GetResourceDescriptor@PixelBuffer' in n:c.hooks[a]=descriptor(0x34)
      elif '?GetResourceDescriptor@RenderTargetState' in n:c.hooks[a]=descriptor(0x18)
      elif '?Initialize@Texture@renderengine' in n:c.hooks[a]=initialize('texture')
      elif '?Initialize@TextureState' in n:c.hooks[a]=initialize('texturestate')
      elif '?Initialize@PixelBuffer' in n:c.hooks[a]=initialize('buffer')
      elif '?Initialize@RenderTargetState' in n:c.hooks[a]=initialize('rtstate')
      elif '?Xbox2SetBaseEDRAM' in n:c.hooks[a]=setbase('edram')
      elif '?Xbox2SetBaseHierarchicalZ' in n:c.hooks[a]=setbase('hizbase')
      elif '?Xbox2CheckPhysicalMemoryFlags' in n:c.hooks[a]=check
      elif '?GetDefaultAllocator@ResourceAllocatorRegistry' in n:c.hooks[a]=lambda x:x.setg(3,alloc)
      elif n=='memcpy':c.hooks[a]=copy
     for a,f in [(0x823ff848,descriptor(0x34)),(0x82b635c8,descriptor(0x20)),(0x82b63678,descriptor(0x18)),(0x82b629d8,initialize('texture')),(0x82b62720,initialize('texturestate')),(0x82b62748,initialize('rtstate')),(0x82b621a8,initialize('buffer')),(0x82b60870,setbase('edram')),(0x82b60898,setbase('hizbase')),(0x82b60d28,check)]:c.hooks[a]=f
     c.setg(3,base);c.setg(4,params);c.run(b.entry if native else 0x824088b0);data=c.mem.rd(base,0x98)
     if native:data=data.replace(struct.pack('>I',gcolour),struct.pack('>I',0x83271100)).replace(struct.pack('>I',gdepth),struct.pack('>I',0x83271140))
     outs.append((data,events))
    assert outs[0]==outs[1],(colour,depth,sampled,use_default,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual RenderTarget constructor vs ARTIST 824088B0 over colour 0/1/2, depth 0/1/2/4, depth sampling and explicit/default allocator; compares complete 0x98 object bytes, allocator descriptors/resources, typed state/texture/buffer parameters and EDRAM/HiZ calls. Native device surface symbols normalized to original global identity; allocator/descriptor/init and physical services controlled. Invalid sections, hardware and full runtime excluded.'};(D/'constructor-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
