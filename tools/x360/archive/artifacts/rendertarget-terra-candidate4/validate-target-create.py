from pathlib import Path
import sys,json,struct,hashlib,itertools
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);cases=0
for sym,orig,depth in [('?CreateColor@Target@postfx@graphics@rw@@',0x824034d0,False),('?CreateDepth@Target@postfx@graphics@rw@@',0x82403688,True)]:
 b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 for i in range(32):
  trace=[];outs=[]
  for native in (False,True):
   c=F.cpu(b);c.mem.add(0x86000000,0x10000);base=F.M.S.SELF;params=F.M.S.AUX;alloc=0x86000000;vtable=alloc+0x100;hook=0x84803f00;F.put(c,alloc,vtable);F.put(c,vtable+16,hook);F.put(c,params,alloc);F.put(c,params+4,320+i);F.put(c,params+8,240+i);F.put(c,params+0x10,0x18280186);F.put(c,params+0x14,0x18280186);F.put(c,params+0x18,1);F.put(c,params+0x1c,i%3);F.put(c,params+0x20,1+(i%2));c.mem.wr(params+0x24,bytes([1 if depth and i%2 else 0]));F.put(c,params+0x28,i);F.put(c,params+0x2c,i+3);c.mem.wr(params+0x44,bytes([1+i%3]));c.mem.wr(base,b'\xa5'*24);events=[];counter=[0]
   def descriptor(size):
    def f(x):
     p=x.g(3)&0xffffffff;vals=[(size,4)]+[(0,1)]*4;x.mem.wr(p,b''.join(struct.pack('>II',*z) for z in vals));x.setg(3,p)
    return f
   def allocate(x):
    ptr=x.g(3)&0xffffffff;desc=x.mem.rd(x.g(5)&0xffffffff,40);events.append(('alloc',desc,x.g(6)));index=counter[0];counter[0]+=1;res=0x86001000+index*0x100;x.mem.wr(ptr,struct.pack('>5I',res,0,0,0,0));x.setg(3,ptr)
   def initialize(kind):
    def f(x):
     resource=x.mem.rd(x.g(3)&0xffffffff,20);ptr=int.from_bytes(resource[:4],'big');raw=x.mem.rd(x.g(4)&0xffffffff,0x4c if kind=='state' else 0x1c)
     if kind=='state':raw=raw[:0x45]+raw[0x48:0x4c]
     events.append((kind,resource,raw));
     if kind=='texture':F.put(x,ptr+0x20,0xabcde000|(ptr&0xfff))
     x.setg(3,ptr)
    return f
   def setbase(kind):
    def f(x):events.append((kind,x.g(3)&0xffffffff,x.g(4)&0xffffffff))
    return f
   def check(x):events.append(('check',x.g(3)&0xffffffff,x.mem.rd((x.g(3)&0xffffffff)+0x20,4)))
   c.hooks[hook]=allocate
   for a,n in b.unresolved.items():
    if '?GetResourceDescriptor@Texture@renderengine' in n:c.hooks[a]=descriptor(0x34)
    elif '?GetResourceDescriptor@TextureState@renderengine' in n:c.hooks[a]=descriptor(0x20)
    elif '?GetResourceDescriptor@PixelBuffer@renderengine' in n:c.hooks[a]=descriptor(0x34)
    elif '?Initialize@Texture@renderengine' in n:c.hooks[a]=initialize('texture')
    elif '?Initialize@TextureState@renderengine' in n:c.hooks[a]=initialize('state')
    elif '?Initialize@PixelBuffer@renderengine' in n:c.hooks[a]=initialize('buffer')
    elif '?Xbox2SetBaseEDRAM@PixelBuffer' in n:c.hooks[a]=setbase('edram')
    elif '?Xbox2SetBaseHierarchicalZ@PixelBuffer' in n:c.hooks[a]=setbase('hizbase')
    elif '?Xbox2CheckPhysicalMemoryFlags@Texture' in n:c.hooks[a]=check
   for a,f in [(0x823ff848,descriptor(0x34)),(0x82b635c8,descriptor(0x20)),(0x82b629d8,initialize('texture')),(0x82b62720,initialize('state')),(0x82b621a8,initialize('buffer')),(0x82b60870,setbase('edram')),(0x82b60898,setbase('hizbase')),(0x82b60d28,check)]:c.hooks[a]=f
   c.setg(3,base);c.setg(4,params);c.run(b.entry if native else orig);trace.append(events);outs.append(c.mem.rd(base,24))
  assert trace[0]==trace[1],(sym,i,trace);assert outs[0]==outs[1],(sym,i,[x.hex() for x in outs]);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual Target CreateColor/CreateDepth vs ARTIST 824034D0/82403688, 64 combinations of section counts, filters, dimensions and optional HiZ. Compares full target bytes, allocator descriptor/flags, texture/state/pixel-buffer parameter/resource lanes and EDRAM/HiZ calls plus HiZ fetch base. Allocator, descriptor/initialize providers and GPU/physical check controlled; no GPU or runtime claim.'};(D/'target-create-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
