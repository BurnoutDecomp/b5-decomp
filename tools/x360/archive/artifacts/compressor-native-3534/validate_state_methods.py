"""Compare actual member Configure/ClearBuffer against original typed state writes."""
from pathlib import Path
import json,random,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT));from validate_kernel import CPU,Obj,Bundle,F
SELF=0x50000000;STACK=0x40000000

def run(b,compiled,index,kind):
 m=F.M.V.Mem();m.add(STACK,0x20000);m.add(SELF,0x100);m.add(0x84800000,0x10000)
 if compiled:
  for a,data in b.images.items():m.add(a,len(data),data)
 c=CPU(m);c.setg(1,STACK+0x18000);c.max_steps=50000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0;c.oracle=0;c.estimates={'log':[],'exp':[]}
 image=random.Random(3534000+index).randbytes(0x100);m.wr(SELF,image)
 def clear(cpu):
  p,v,n=[cpu.g(i)&0xffffffff for i in(3,4,5)];m.wr(p,bytes([v&255])*n);cpu.setg(3,p)
 if compiled:
  for a,n in b.unresolved.items():
   if n in('memset','XMemSet'):c.hooks[a]=clear
   elif n.startswith(('__savefpr','__restfpr')):c.hooks[a]=lambda cpu:None
   else:
    def reject(cpu,name=n):raise AssertionError(name)
    c.hooks[a]=reject
 else:c.hooks[0x82926fd0]=clear
 c.setg(3,SELF)
 if kind=='configure':
  rng=random.Random(index);on,off,exp=(rng.uniform(-1,1),rng.uniform(-1,1),[-1.0,-0.5,0.0,0.75,1e-20][index%5]);values=[0,1,-1,2,16777217,2147483647,-2147483648]
  c.setf(1,on);c.setf(2,off);c.setf(3,exp);c.setg(7,values[index%7]&0xffffffff);c.setg(8,values[(index//7)%7]&0xffffffff);c.setg(9,index%2)
 c.run(b.entry if compiled else(0x82b67188 if kind=='configure'else 0x82b671f0));return m.rd(SELF,0x100).hex()
def main():
 r=json.loads((OUT/'provider-first/native/compile-results.json').read_text());o=Obj(r['results'][0]['object']);b=Bundle([o]);errors=[];count=0
 for kind,prefix in [('clear','?ClearBuffer@CompressorLimiter1@'),('configure','?Configure@CompressorLimiter1@')]:
  b.select(prefix)
  for i in range(256):
   a,z=[run(b,c,i,kind)for c in(False,True)];count+=1
   if a!=z:errors.append({'function':kind,'case':i,'original':a,'compiled':z});break
  print(kind,i+1,'cases;',len(errors),'mismatches',flush=True)
  if errors:break
 (OUT/'state-method-comparison.json').write_text(json.dumps({'cases':count,'mismatches':errors,'scope':__doc__,'bool_arguments':'Only valid bool0/1; native raw invalid-bool ABI residues are outside the C++ API.'},indent=2)+'\n')
 if errors:print(str(errors[0])[:4500])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
