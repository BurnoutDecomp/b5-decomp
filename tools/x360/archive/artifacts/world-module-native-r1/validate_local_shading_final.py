"""Actual whole-CPP LOD methods and VMX helpers versus original instruction islands.

The Construct/Update islands are genuine inlined methods. Clamp and Scale compare
actual original VMX operations over all128-bit rows, with finite scalar inputs and
arbitrary row bits. These are bounded instruction/state witnesses, not complete
GenerateDispatchLists or hardware floating-exception/VSCR proofs.
"""
from pathlib import Path
import hashlib,json,random,struct,sys
import numpy as np
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F,Obj,Bundle
STACK=0x40000000;SELF=0x50000000
class Stop(Exception):pass
def cpu(b=None):
 m=F.M.V.Mem();m.add(STACK,0x10000);m.add(SELF,0x200)
 if b:
  for at,data in b.images.items():m.add(at,len(data),data)
 c=F.AtomicCPU(m);c.setg(1,STACK+0x8000);c.max_steps=10000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0;return c
def island(c,start,end):
 def observe(pc,w):
  if pc==end:raise Stop()
 c.instruction_observer=observe
 try:c.run(start)
 except Stop:pass
 else:raise AssertionError('island did not stop')
 del c.instruction_observer
def bits(f):return struct.unpack('>I',struct.pack('>f',f))[0]
def execute(b,compiled,kind,index):
 c=cpu(b if compiled else None);rng=random.Random(0x2700+index);data=bytearray(rng.randbytes(0x200));ptr=SELF+0x20
 scalar=[-3.,-0.0,0.,0.125,1.,20.,1000.][index%7]
 if kind=='update':
  word=bits(scalar)if index%4 else rng.getrandbits(32);struct.pack_into('>I',data,0x40,word)
 c.mem.wr(SELF,data)
 if compiled:
  assert not b.unresolved;b_entry=b.entry;c.setg(3,ptr)
  if kind in('clamp','scale'):c.setf(1,scalar)
  c.run(b_entry)
 elif kind=='construct':
  c.setg(11,ptr);c.setg(26,16);c.setg(30,0);c.setg(23,0xffffffff);c.setf(30,20.);c.vr[0]=(bits(1000.),)*4;c.vr[13]=(bits(20.),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32));island(c,0x827cfdac,0x827cfdc8)
 elif kind=='update':
  c.setg(10,ptr);c.setg(11,STACK+0x80e4);c.setg(31,0);c.setg(8,0x83010000);island(c,0x827d1d8c,0x827d1dc0)
 elif kind=='clamp':
  c.vr[126]=struct.unpack('>4I',c.mem.rd(ptr,16));c.vr[127]=(0,)*4;c.mem.wr(STACK+0x8100,struct.pack('>4I',bits(scalar),bits(scalar),bits(scalar),0));island(c,0x827d15e0,0x827d15f8);c.mem.wr(ptr,struct.pack('>4I',*c.vr[1]))
 elif kind=='scale':
  for row in range(4):
   c.vr[125]=struct.unpack('>4I',c.mem.rd(ptr+16*row,16));c.vr[13]=(bits(scalar),)*4;c.vr[0]=(0,)*4;island(c,0x827d2634,0x827d263c);c.mem.wr(ptr+16*row,struct.pack('>4I',*c.vr[13]))
 return c.mem.rd(SELF,0x200)
def main():
 r=json.loads((OUT/'native-final/native/compile-results.json').read_text());row=r['results'][0];o=Obj(row['object']);errors=[];counts={}
 with np.errstate(all='ignore'):
  for kind,prefix in [('construct','?Construct@ShaderLodInfo@'),('update','?Update@ShaderLodInfo@'),('clamp','?ClampColourToWhiteLevel@'),('scale','?ScaleIrradiance@')]:
   b=Bundle([o]);b.select(prefix)
   for i in range(256):
    a,z=[execute(b,c,kind,i)for c in(False,True)]
    if a!=z:errors.append({'kind':kind,'case':i,'first_differences':[hex(j)for j,(u,v)in enumerate(zip(a,z))if u!=v][:32],'original':a.hex(),'compiled':z.hex()});break
   counts[kind]=i+1;print(kind,i+1,'cases;',len(errors),'mismatches',flush=True)
   if errors:break
 result={'cases':sum(counts.values()),'counts':counts,'mismatches':errors,'scope':__doc__,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()};(OUT/'local-shading-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
 if errors:print(str(errors[0])[:4000])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
