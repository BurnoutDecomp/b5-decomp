"""Original and compiled plug-in Process through actual Clear/Configure/VMX kernel.

Production uses actual native VMX log/exp estimates. This software fixture uses
shared mathematical-f32 and quantized estimate surrogates, compares their exact
input vectors separately, and controls CRT pow, XMemSet/memset and FPR save/restore services.
No Xenon estimate-table, floating-exception, VSCR-saturation or hardware-bit parity
claim is made. Cache hints have no modeled hardware cache effects.
"""
from pathlib import Path
import argparse,hashlib,json,math,random,struct,time
from validate_kernel import CPU,Obj,Bundle,F,SELF,CTX,SRC,DST,INPUT,OUTPUT,STACK,make_input,words,f32bits
OUT=Path(__file__).resolve().parent
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/compressor-native-3534')
FORMAT=0x55000000
M32=0xffffffff

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def fstore(data,at,value):struct.pack_into('>I',data,at,f32bits(value))

def make_plugin(kind,index,channels):
 rng=random.Random(0x3534+index+0x1000*(kind=='Limiter1'))
 plugin=bytearray(rng.randbytes(0x200));engine,inp,dst,stride=make_input(index,channels)
 offset=0x40 if kind=='Limiter1'else 0x50
 plugin[offset:offset+0x80]=engine
 plugin[0x20]=channels;plugin[0x21]=channels
 mode=(index//7)%8;rate=(32000.,44100.,48000.)[(index//3)%3]
 threshold=(-12.,-24.,-3.)[index%3];release=(0.,0.125,0.5)[index%3];link=float(index%2)
 if kind=='Limiter1':
  attrs=[threshold,release,link];cache=0x90;state=0xa0
 else:
  attrs=[threshold,(2.,4.,10.)[index%3],(0.,0.025,0.1)[index%3],release,link];cache=0xa0;state=0xb8
 for j,value in enumerate(attrs):fstore(plugin,0x28+j*8,value);fstore(plugin,cache+j*4,value)
 fstore(plugin,cache+len(attrs)*4,rate)
 struct.pack_into('>I',plugin,state,1 if index&1 else 0)
 # Actual bypass, state edge and each cache/reconfiguration path.
 if mode==0:
  fstore(plugin,0x28,30.);struct.pack_into('>I',plugin,state,0)
 elif mode==1:
  fstore(plugin,0x28,30.);struct.pack_into('>I',plugin,state,1)
 elif mode==2:
  # Already configured: finite state from the independently tested kernel input.
  pass
 elif mode==3:
  fstore(plugin,cache,threshold+1.)
 elif mode==4:
  # Reconfigure and execute the real release clamp before the real kernel.
  slot=1 if kind=='Limiter1'else 3
  fstore(plugin,0x28+slot*8,-0.5 if index&1 else 31.)
 elif mode==5:
  fstore(plugin,cache+len(attrs)*4,22050.)
 elif mode==6:
  fstore(plugin,0x28,math.nan) # original unordered bypass
 elif mode==7:
  if kind=='Compressor1':fstore(plugin,0x30,1.) # ratio bypass, no estimates
  else:fstore(plugin,cache+2*4,1.-link) # reconfigure grouping
 return bytes(plugin),inp,dst,stride,rate,mode

def execute(bundle,kind,compiled,index,channels,oracle):
 mem=F.M.V.Mem();mem.add(STACK,0x20000);mem.add(SELF,0x400);mem.add(CTX,0x30100);mem.add(SRC,0x2000);mem.add(INPUT,0x4000);mem.add(OUTPUT,0x4000);mem.add(FORMAT,0x100);mem.add(0x84800000,0x10000)
 if compiled:
  for at,blob in bundle.images.items():mem.add(at,len(blob),blob)
 cpu=CPU(mem);cpu.setg(1,STACK+0x18000);cpu.max_steps=600000;cpu.atomic_trace=[];cpu.fence_trace=[];cpu.msr=0x8000;cpu.reservation=None;cpu.failures=0;cpu.estimates={'log':[],'exp':[]};cpu.oracle=oracle
 initial,inp,dst,stride,rate,mode=make_plugin(kind,index,channels)
 mem.wr(SELF,initial);mem.wr(INPUT,inp);mem.wr(OUTPUT,dst);words(cpu,CTX+0x3000c,[SRC,DST]);words(cpu,CTX+0x30018,[FORMAT]);words(cpu,FORMAT+0xc,[f32bits(rate)]);words(cpu,SRC+4,[INPUT]);words(cpu,DST+4,[OUTPUT]);mem.wr(SRC+0xe,struct.pack('>H',stride));mem.wr(DST+0xe,struct.pack('>H',stride))
 entries={'kernel':bundle.addr('?Process@CompressorLimiter1@')if compiled else 0x82b64db0,'clear':bundle.addr('?ClearBuffer@CompressorLimiter1@')if compiled else 0x82b671f0,'configure_engine':bundle.addr('?Configure@CompressorLimiter1@')if compiled else 0x82b67188,'configure_plugin':bundle.addr('?Configure@'+kind+'@')if compiled else(0x82b97ab0 if kind=='Limiter1'else 0x82b96b28)}
 calls=[];pow_calls=[];memory_calls=[]
 def pow_service(c):
  x,y=c.f(1),c.f(2);pow_calls.append((struct.pack('>dd',x,y).hex()));c.setf(1,math.pow(x,y))
 def memset_service(c):
  dst,value,size=[c.g(i)&M32 for i in(3,4,5)];memory_calls.append([dst,value,size]);mem.wr(dst,bytes([value&255])*size);c.setg(3,dst)
 def observe(pc,word):
  for label,at in entries.items():
   if pc==at:
    if label=='kernel':args=[cpu.g(i)&M32 for i in(3,4,5)]
    elif label=='clear':args=[cpu.g(3)&M32]
    elif label=='configure_engine':args=[cpu.g(3)&M32,*[cpu.g(i)&M32 for i in(7,8,9)],struct.pack('>fff',cpu.f(1),cpu.f(2),cpu.f(3)).hex()]
    else:args=[cpu.g(3)&M32,struct.pack('>f',cpu.f(1)).hex()]
    calls.append([label,args])
 cpu.instruction_observer=observe
 if compiled:
  for at,name in bundle.unresolved.items():
   if name.startswith(('__savefpr','__restfpr')):cpu.hooks[at]=lambda c:None
   elif name=='pow':cpu.hooks[at]=pow_service
   elif name in('memset','XMemSet'):cpu.hooks[at]=memset_service
   else:
    def reject(c,n=name):raise AssertionError(('unexpected unresolved provider',n))
    cpu.hooks[at]=reject
 else:
  cpu.hooks[0x82c09970]=pow_service;cpu.hooks[0x82926fd0]=memset_service
 cpu.setg(3,SELF);cpu.setg(4,CTX);cpu.setg(5,index&1)
 entry=bundle.entry if compiled else(0x82b9e3a0 if kind=='Limiter1'else 0x82b9d988)
 try:cpu.run(entry)
 except Exception as e:raise RuntimeError((kind,compiled,index,channels,oracle,hex(getattr(cpu,'last_pc',0))))from e
 estimates={k:sorted(v)for k,v in cpu.estimates.items()}
 kernel_calls=sum(x[0]=='kernel'for x in calls)
 assert len(estimates['log'])==len(estimates['exp'])==kernel_calls*channels*64
 assert cpu.g(3)&M32==1
 return{'plugin':mem.rd(SELF,0x200).hex(),'input':mem.rd(INPUT,0x4000).hex(),'output':mem.rd(OUTPUT,0x4000).hex(),'context':mem.rd(CTX,0x30100).hex(),'descriptors':mem.rd(SRC,0x2000).hex(),'format':mem.rd(FORMAT,0x100).hex(),'calls':calls,'pow_calls':pow_calls,'memory_calls':memory_calls,'estimate_inputs':estimates,'return':cpu.g(3)&M32}

def main():
 p=argparse.ArgumentParser();p.add_argument('--stage',default='composed-callers-ready');p.add_argument('--cases',type=int,default=112);p.add_argument('--kind',choices=['Compressor1','Limiter1','both'],default='both');a=p.parse_args()
 report=json.loads((OUT/a.stage/'native/compile-results.json').read_text());rows={Path(r['source']).name:r for r in report['results']};assert all(r['passed']for r in rows.values())
 errors=[];count=0;coverage={};start=time.monotonic();objects={};kinds=['Compressor1','Limiter1']if a.kind=='both'else[a.kind]
 for kind in kinds:
  objects[kind]=[Obj(rows[kind+'.cpp']['object']),Obj(rows['CompressorLimiter1.cpp']['object'])];bundle=Bundle(objects[kind]);bundle.select('?Process@'+kind+'@');coverage[kind]={'cases':0,'kernel_calls':0,'clear_calls':0,'configure_calls':0,'channels':set(),'modes':set(),'oracles':set()}
  for case in range(a.cases):
   channels=case%7;index=case;oracle=(case//56)%2
   pair=[execute(bundle,kind,c,index,channels,oracle)for c in(False,True)];count+=1
   cov=coverage[kind];cov['cases']+=1;cov['channels'].add(channels);cov['modes'].add((index//7)%8);cov['oracles'].add(oracle)
   for label,_ in pair[1]['calls']:
    if label=='kernel':cov['kernel_calls']+=1
    elif label=='clear':cov['clear_calls']+=1
    elif label=='configure_engine':cov['configure_calls']+=1
   if pair[0]!=pair[1]:
    error={'kind':kind,'case':case,'channels':channels,'oracle':oracle,'differences':{}}
    for key in pair[0]:
     if pair[0][key]!=pair[1][key]:
      if key in('calls','pow_calls','memory_calls','estimate_inputs','return'):error['differences'][key]=[v[key]for v in pair]
      else:
       x,y=[bytes.fromhex(v[key])for v in pair];error['differences'][key]={'first_byte_differences':[hex(i)for i,(u,v)in enumerate(zip(x,y))if u!=v][:32],'original':x.hex(),'compiled':y.hex()}
    errors.append(error);print('FAIL',kind,case,str(error)[:2500],flush=True);break
   print('PASS',kind,case,'channels',channels,'mode',(index//7)%8,'oracle',oracle,flush=True)
  if errors:break
 for cov in coverage.values():
  for key in('channels','modes','oracles'):cov[key]=sorted(cov[key])
 result={'cases':count,'mismatches':errors,'coverage':coverage,'elapsed_seconds':time.monotonic()-start,'scope':__doc__,'objects':{str(o.path):sha(o.path)for oo in objects.values()for o in oo},'script_sha256':sha(__file__),'kernel_fixture_sha256':sha(OUT/'validate_kernel.py'),'actual_inputs_sha256':sha(OUT/a.stage/'actual-inputs.json')}
 (OUT/('composed-process-'+a.kind+'-comparison.json')).write_text(json.dumps(result,indent=2)+'\n');print('TOTAL',count,'mismatches',len(errors),flush=True);raise SystemExit(bool(errors))
if __name__=='__main__':main()
