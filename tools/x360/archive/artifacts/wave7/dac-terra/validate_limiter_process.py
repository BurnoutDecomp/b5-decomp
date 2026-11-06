import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'limiter-build/Limiter1.obj';errors=[];rng=random.Random(0xe3a0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
b=F.Bundle([F.Coff(p,'?Process@Limiter1@')]);b.select('?Process@Limiter1@')
for i in range(128):
 initial=bytearray(rng.randbytes(0x200));initial[0x21]=1+i%6
 for at,val in [(0x28,[10.,30.][i%2]),(0x30,0.1),(0x38,1.),(0x90,[10.,11.][(i//2)%2]),(0x94,0.1),(0x98,1.),(0x9c,48000.)]:struct.pack_into('>f',initial,at,val)
 struct.pack_into('>I',initial,0xa0,(i//4)%2);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);F.put(c,mix+0x30018,aux+0x5000);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);c.setg(3,selfaddr);c.setg(4,mix);trace=[]
  def clear(x):
   dst=x.g(3)&0xffffffff;trace.append(('clear',dst));x.mem.wr(dst,bytes(48));x.setg(3,dst)
  def configure(x):trace.append(('configure',x.g(3)&0xffffffff,x.f(1)))
  def kernel(x):trace.append(('kernel',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.g(5)&0xffffffff))
  c.hooks[0x82b671f0]=clear;c.hooks[0x82b97ab0]=configure;c.hooks[0x82b64db0]=kernel
  for at,n in b.unresolved.items():
   if n.startswith('?ClearBuffer@CompressorLimiter1@'):c.hooks[at]=clear
   if n.startswith('?Process@CompressorLimiter1@'):c.hooks[at]=kernel
  c.hooks[F.address(b,'?Configure@Limiter1@')]=configure
  c.run(b.entry if compiled else 0x82b9e3a0);both.append((c.mem.rd(selfaddr,0x200),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20], 'trace':[both[0][1],both[1][1]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual Limiter1 Process state/branch and call arguments; Configure, ClearBuffer, VMX compressor kernel controlled. No actual compressor math/output claim.'};(out/'limiter-process-boundary-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
