"""Full original/compiled DSP flow under explicit shared estimate surrogates.

Production executes native vlogefp/vexptefp. This fixture does not know Xenon's
estimate tables: mathematical-round and quantized surrogates are used identically
on both sides, and estimate operand bits are checked separately. No hardware bit
parity or floating exception/VSCR-state proof is claimed.
"""
from pathlib import Path
import argparse,collections,hashlib,json,math,random,struct,sys,time
import numpy as np
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import Obj,Bundle,F
SELF=0x50000000;CTX=0x51000000;SRC=0x52000000;DST=0x52001000;INPUT=0x53000000;OUTPUT=0x54000000;STACK=0x40000000
M32=0xffffffff

def f32bits(v):
 with np.errstate(all='ignore'):return struct.unpack('>I',struct.pack('>f',float(np.float32(v))))[0]
def floatbits(v):return struct.unpack('>f',struct.pack('>I',v&M32))[0]
class CPU(F.AtomicCPU):
 def step(self,pc,w,nxt,depth):
  if w>>26==31 and (w>>1)&1023==278:return nxt # dcbt is a non-faulting cache hint; hardware cache effects excluded
  return super().step(pc,w,nxt,depth)
 def exec_vector(self,a,w,mn):
  op=w>>26;vd=(w>>21)&31;va=(w>>16)&31;vb=(w>>11)&31;x=w&0x7ff
  if mn.startswith('vupkd3d128'):
   vd|=((w>>2)&3)<<5
   self.vr[vd]=(0,0,0,0x3f800000);return True # only word3 is live, zero NORM2 input
  if op==4 and x in(0x1ca,0x18a):
   values=self.vf(vb);self.estimates['log'if x==0x1ca else'exp'].append(tuple(self.vr[vb]));result=[]
   for value in values:
    f=float(value)
    if x==0x1ca:
     y=math.nan if math.isnan(f)or f<0 else(-math.inf if f==0 else math.log2(f))
    else:
     if math.isnan(f):y=math.nan
     elif f>128:y=math.inf
     elif f< -150:y=0.0
     else:y=2.0**f
    word=f32bits(y)
    if self.oracle and word&0x7f800000 not in(0,0x7f800000):word&=0xffffff80
    result.append(word)
   self.vr[vd]=tuple(result);return True
  if op==4 and x==0x3ca: # vctsxs
   vals=[]
   for v in self.vf(vb):
    f=float(v)*(2**va)
    if math.isnan(f):n=0 # the only consumed parity bit is0 for either NaN conversion convention
    elif f>=2147483647:n=2147483647
    elif f<=-2147483648:n=-2147483648
    else:n=math.trunc(f)
    vals.append(n&M32)
   self.vr[vd]=tuple(vals);return True
  if op==4 and x==0x24a: # vrfiz
   vals=[]
   for word,v in zip(self.vr[vb],self.vf(vb)):
    f=float(v);vals.append(word|0x00400000 if math.isnan(f)else(word if not math.isfinite(f)or f==0 else f32bits(math.trunc(f))))
   self.vr[vd]=tuple(vals);return True
  if op==4 and x==0x284: # vsrw
   self.vr[vd]=tuple((self.vr[va][i]&M32)>>(self.vr[vb][i]&31)for i in range(4));return True
  if op==4 and (w&0x3ff)==0x286: # vcmpgtuw
   self.vr[vd]=tuple(M32 if self.vr[va][i]>self.vr[vb][i]else 0 for i in range(4));return True
  return super().exec_vector(a,w,mn)

def words(c,a,v):c.mem.wr(a,struct.pack('>'+'I'*len(v),*(x&M32 for x in v)))
def make_input(index,channels):
 rng=random.Random(3534+index);state=bytearray(rng.randbytes(0x80));kind=index%6
 for ch in range(6):struct.pack_into('>ff',state,ch*8,0.01*(ch+1)if kind else 0,-0.01*ch)
 on,off,target,stepOn,stepOff=(0.2,0.02,-0.75,-0.015625,-0.0078125)
 if kind==2:on,off=0.1,0.3 # overlapping hysteresis
 if kind==3:on,off=-0.2,-0.3 # negative bases exercise native edge masks
 if kind==4:on,off=0.0,0.0
 if kind==5:target,stepOn,stepOff=-1.0,-0.25,-0.125
 struct.pack_into('>fffii ff',state,0x30,on,off,target,37,93,stepOn,stepOff)
 state[0x4c]=(0,1,255)[(index//6)%3]
 stride=(256,272)[(index//18)%2];inp=bytearray(rng.randbytes(0x4000));dst=bytearray(rng.randbytes(0x4000))
 for ch in range(max(channels,1)):
  for n in range(256):
   value=0.0 if kind==0 else(((-1.0)**(n&1))*(0.03+0.91*(n%37)/36)if kind in(1,2)else rng.uniform(-1.3,1.3))
   struct.pack_into('>f',inp,(stride*ch+n)*4,value)
 return bytes(state),bytes(inp),bytes(dst),stride

def execute(bundle,compiled,index,channels,oracle):
 mem=F.M.V.Mem();mem.add(STACK,0x20000);mem.add(SELF,0x100);mem.add(CTX,0x30100);mem.add(SRC,0x2000);mem.add(INPUT,0x4000);mem.add(OUTPUT,0x4000);mem.add(0x84800000,0x10000)
 if compiled:
  for a,b in bundle.images.items():mem.add(a,len(b),b)
 c=CPU(mem);c.setg(1,STACK+0x18000);c.max_steps=500000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0;c.estimates={'log':[],'exp':[]};c.oracle=oracle
 state,inp,dst,stride=make_input(index,channels);mem.wr(SELF,state);mem.wr(INPUT,inp);mem.wr(OUTPUT,dst);words(c,CTX+0x3000c,[SRC,DST]);words(c,SRC+4,[INPUT]);words(c,DST+4,[OUTPUT]);mem.wr(SRC+0xe,struct.pack('>H',stride));mem.wr(DST+0xe,struct.pack('>H',stride))
 if compiled:
  for a,name in bundle.unresolved.items():
   if name.startswith(('__savefpr','__restfpr')):c.hooks[a]=lambda cpu:None
   else:
    def reject(cpu,n=name):raise AssertionError(('unexpected provider',n))
    c.hooks[a]=reject
 c.setg(3,SELF);c.setg(4,CTX);c.setg(5,channels)
 try:c.run(bundle.entry if compiled else 0x82b64db0)
 except Exception as e:raise RuntimeError((compiled,index,channels,oracle,hex(getattr(c,'last_pc',0))))from e
 estimates={k:sorted(v)for k,v in c.estimates.items()}
 assert len(estimates['log'])==len(estimates['exp'])==channels*64
 return{'state':mem.rd(SELF,0x80).hex(),'input':mem.rd(INPUT,0x4000).hex(),'output':mem.rd(OUTPUT,0x4000).hex(),'slots':mem.rd(CTX+0x3000c,8).hex(),'descriptors':mem.rd(SRC,0x2000).hex(),'estimate_inputs':estimates}

def main():
 p=argparse.ArgumentParser();p.add_argument('--cases',type=int,default=14);p.add_argument('--stage',default='provider-first');a=p.parse_args();report=json.loads((OUT/a.stage/'native/compile-results.json').read_text());row=report['results'][0];o=Obj(row['object']);b=Bundle([o]);b.select('?Process@CompressorLimiter1@');errs=[];start=time.monotonic();count=0
 for i in range(a.cases):
  channels=i%7;index=i//7+(i%7)*6;oracle=(i//7)%2
  pair=[execute(b,c,index,channels,oracle)for c in(False,True)];count+=1
  if pair[0]!=pair[1]:
   e={'case':i,'index':index,'channels':channels,'oracle':oracle}
   for k in pair[0]:
    if pair[0][k]!=pair[1][k]:
     if k=='estimate_inputs':e[k]=[v[k]for v in pair]
     else:
      x,y=[bytes.fromhex(v[k])for v in pair];e[k]={'first_byte_differences':[hex(j)for j,(xx,yy)in enumerate(zip(x,y))if xx!=yy][:32],'original':x.hex(),'compiled':y.hex()}
   errs.append(e);break
  print('PASS',i,'channels',channels,'oracle',oracle,flush=True)
 result={'cases':count,'mismatches':errs,'elapsed_seconds':time.monotonic()-start,'scope':__doc__,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(OUT/(a.stage+'-kernel-comparison.json')).write_text(json.dumps(result,indent=2)+'\n');print('TOTAL',count,'mismatches',len(errs),flush=True)
 if errs:print(str(errs[0])[:5000])
 raise SystemExit(bool(errs))
if __name__=='__main__':main()
