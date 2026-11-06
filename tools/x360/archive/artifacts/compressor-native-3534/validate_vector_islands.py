"""Direct original VMX island comparisons for special-value power and uint gain selection.

The same explicit estimate surrogates as the whole-kernel fixture are used. Exact
Xenon estimate tables, exception flags and VSCR saturation state are not modeled.
"""
from pathlib import Path
import itertools,json,random,struct,sys,time
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT));from validate_kernel import CPU,Obj,Bundle,F
STACK=0x40000000;AUX=0x60000000
class Stop(Exception):pass
class ProbeCPU(CPU):
 def exec_vector(self,a,w,mn):
  if a==getattr(self,"boundary",0):raise Stop()
  return super().exec_vector(a,w,mn)

def make(b):
 mem=F.M.V.Mem();mem.add(STACK,0x20000);mem.add(AUX,0x10000);mem.add(0x84800000,0x10000)
 for a,data in b.images.items():mem.add(a,len(data),data)
 c=ProbeCPU(mem);c.setg(1,STACK+0x18000);c.max_steps=50000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0;c.estimates={'log':[],'exp':[]};c.oracle=0
 return c

def power(b,compiled,base,exponent,oracle):
 c=make(b);c.oracle=oracle
 if compiled:
  c.mem.wr(AUX+0x100,struct.pack('>4I',*base));c.mem.wr(AUX+0x200,struct.pack('>4I',*exponent));c.setg(3,AUX);c.setg(4,AUX+0x100);c.setg(5,AUX+0x200);c.run(b.entry);result=struct.unpack('>4I',c.mem.rd(AUX,16))
 else:
  c.vr[0]=(0,)*4;c.vr[6]=(0x80000000,)*4;c.vr[7]=base;c.vr[8]=exponent;c.vr[10]=(0xfffffff7,)*4;c.vr[11]=(0x3f800000,)*4;c.vr[12]=(1,)*4;c.vr[13]=(0xffffffff,)*4;c.setf(10,1.0)
  def observer(pc,word):
   if pc==0x82b65a18:raise Stop()
  c.instruction_observer=observer;c.boundary=0x82b65a18
  try:c.run(0x82b6591c)
  except Stop:pass
  else:raise AssertionError('island missed boundary')
  result=c.vr[4]
 return list(result),c.estimates

def linked(b,compiled,previous,current,first):
 c=make(b);mask=(0xffffffff if first else 0,)*4
 if compiled:
  for off,value in[(0x100,previous),(0x200,current),(0x300,mask)]:c.mem.wr(AUX+off,struct.pack('>4I',*value))
  c.setg(3,AUX);c.setg(4,AUX+0x100);c.setg(5,AUX+0x200);c.setg(6,AUX+0x300);c.run(b.entry);return list(struct.unpack('>4I',c.mem.rd(AUX,16)))
 c.vr[7]=previous;c.vr[14]=previous;c.vr[3]=current;c.vr[9]=mask;c.setg(4,AUX);c.setg(31,AUX+16)
 def observer(pc,word):
  if pc==0x82b66f48:raise Stop()
 c.instruction_observer=observer;c.boundary=0x82b66f48
 try:c.run(0x82b66f24)
 except Stop:pass
 else:raise AssertionError('reduction missed boundary')
 return list(c.vr[7])

def main():
 report=json.loads((OUT/'power-probe-compile.json').read_text());assert report['passed'];obj=Obj(report['object']);b=Bundle([obj]);errors=[];cases=0
 bases=[0,0x80000000,0x3f800000,0xbf800000,0x40000000,0xc0000000,0x3e800000,0xbe800000,0x7f800000,0xff800000,0x7fc12345,0xffc12345,1,0x80000001,0x00800000,0x80800000,0x7f7fffff]
 exponents=[0,0x80000000,0x3f000000,0xbf000000,0x3f800000,0xbf800000,0x40000000,0xc0000000,0x40400000,0xc0400000,0x4f000000,0xcf000000,0x7f800000,0xff800000,0x7fc00000,0xffc00000,1,0x80000001]
 pairs=list(itertools.product(bases,exponents));b.select('NativeGainPowerProbe')
 for oracle in(0,1):
  for i in range(0,len(pairs),4):
   group=(pairs[i:i+4]+pairs[:4])[:4];base=tuple(x[0]for x in group);e=tuple(x[1]for x in group);a,z=[power(b,c,base,e,oracle)for c in(False,True)];cases+=1
   if a!=z:errors.append({'kind':'power','oracle':oracle,'base':base,'exponent':e,'original':a,'compiled':z});break
  if errors:break
 print('power',cases,'vector cases;',len(errors),'mismatches',flush=True)
 power_cases=cases
 if not errors:
  b.select('NativeLinkedGainProbe');rng=random.Random(3534)
  for i in range(512):
   old=tuple(rng.getrandbits(32)for _ in range(4));new=tuple(rng.getrandbits(32)for _ in range(4));first=i%2;a,z=[linked(b,c,old,new,first)for c in(False,True)];cases+=1
   if a!=z:errors.append({'kind':'linked','original':a,'compiled':z});break
  print('linked',cases-power_cases,'vector cases;',len(errors),'mismatches',flush=True)
 result={'cases':cases,'power_vector_cases':power_cases,'linked_vector_cases':cases-power_cases,'mismatches':errors,'scope':__doc__,'special_inputs':{'bases':bases,'exponents':exponents},'original_islands':{'power':[0x82b6591c,0x82b65a18],'linked':[0x82b66f24,0x82b66f48]}};(OUT/'vector-island-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
 if errors:print(str(errors[0])[:4500])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
