from pathlib import Path
import sys,json,struct,hashlib
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);sym='?Initialize@RenderTarget@postfx@graphics@rw@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);cases=0
for i in range(32):
 null=i%2;use_default=(i//2)%2;outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x86000000,0x10000);params=F.M.S.AUX;alloc=0x86000000;vt=alloc+0x100;hook=0x84803f00;memory=0x86001000;F.put(c,alloc,vt);F.put(c,vt+16,hook);F.put(c,params,0 if use_default else alloc);F.put(c,params+4,320+i);F.put(c,params+8,240+i);c.mem.wr(params+0x18,b'\0\1');c.mem.wr(memory,b'\xa5'*0x98);events=[]
  def allocate(x):
   p=x.g(3)&0xffffffff;events.append(('alloc',x.g(4),x.mem.rd(x.g(5)&0xffffffff,40),x.g(6)));x.mem.wr(p,struct.pack('>5I',0 if null else memory,0,0,0,0));x.setg(3,p)
  def default(x):events.append(('default',));x.setg(3,alloc)
  def copy(x):
   dst=x.g(3)&0xffffffff;x.mem.wr(dst,x.mem.rd(x.g(4)&0xffffffff,x.g(5)));x.setg(3,dst)
  def desc(x):F.put(x,x.g(3)&0xffffffff,0);F.put(x,(x.g(3)&0xffffffff)+4,1)
  c.hooks[hook]=allocate;c.hooks[0x82bbd1e0]=default;c.hooks[0x82c0a630]=copy
  for a,n in b.unresolved.items():
   if n.startswith('??0BaseResourceDescriptor@'):c.hooks[a]=desc
   elif '?GetDefaultAllocator@ResourceAllocatorRegistry' in n:c.hooks[a]=default
   elif n=='memcpy':c.hooks[a]=copy
  c.setg(3,params);c.run(b.entry if native else 0x82409b60);outs.append((c.g(3),c.mem.rd(memory,0x98),events))
 assert outs[0]==outs[1],(i,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual RenderTarget::Initialize vs ARTIST 82409B60 for explicit/default allocators and allocation success/null; compares full five-lane descriptor, allocator order, returned pointer and complete 0x98 result. Allocator, descriptor constructor and memcpy controlled; no GPU or runtime claim.'};(D/'initialize-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)
