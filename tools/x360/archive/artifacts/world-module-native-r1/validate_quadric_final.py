"""Actual whole-CPP quadric packer versus ARTIST827B0790, finite matrix inputs.

All32 output words, complete input/guard bytes and valid input/output aliases are
compared. Original and compiled PPC execute, without controlled math providers.
The actual ARTIST CRT initializer82C74120 populates its real permutation table
before each side; masks are not seeded. Floating exception/VSCR/physical hardware
behavior is not modeled.
"""
from pathlib import Path
import hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F,Obj,Bundle
from validate_vpu_mask_startup import initialise
STACK=0x40000000;MATS=[0x50000000+i*0x1000 for i in range(5)]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def execute(b,compiled,index):
 m=F.M.V.Mem();m.add(STACK,0x10000);m.add(0x8327f000,0x1000)
 for a in MATS:m.add(a,0x100)
 if compiled:
  for a,data in b.images.items():m.add(a,len(data),data)
 c=F.AtomicCPU(m);c.setg(1,STACK+0x8000);c.max_steps=15000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0
 initialise(c)
 rng=random.Random(0x827b0790+index)
 for a in MATS:
  data=bytearray(rng.randbytes(0x100))
  for i in range(16):struct.pack_into('>f',data,0x20+i*4,(-0.0 if(i+index)%11==0 else(0.0 if(i+index)%7==0 else rng.uniform(-3.0,3.0))))
  m.wr(a,data)
 inputs=[a+0x20 for a in MATS[:3]];outputs=[a+0x20 for a in MATS[3:]]
 mode=index%8
 if mode==1:outputs[0]=inputs[0]
 if mode==2:outputs[1]=inputs[1]
 if mode==3:outputs=inputs[:2]
 if mode==4:outputs[1]=outputs[0]
 if mode==5:outputs=inputs[1:3]
 if mode==6:outputs=[inputs[2],inputs[0]]
 if mode==7:inputs=[inputs[0]]*3
 for r,v in enumerate(inputs+outputs,3):c.setg(r,v)
 assert not b.unresolved
 try:c.run(b.entry if compiled else 0x827b0790)
 except Exception as e:raise RuntimeError((compiled,index,hex(getattr(c,'last_pc',0))))from e
 return[m.rd(a,0x100).hex()for a in MATS]
def main():
 report=json.loads((OUT/'native-final/native/compile-results.json').read_text());row=report['results'][0];o=Obj(row['object']);b=Bundle([o]);b.select('?GenerateShaderConstantsForQuadricIrradiance@');errors=[]
 for i in range(256):
  pair=[execute(b,c,i)for c in(False,True)]
  if pair[0]!=pair[1]:
   error={'case':i,'mode':i%8,'differences':[]}
   for j,(a,z)in enumerate(zip(*pair)):
    x,y=bytes.fromhex(a),bytes.fromhex(z)
    if x!=y:error['differences'].append({'region':j,'byte_offsets':[hex(k)for k,(v,w)in enumerate(zip(x,y))if v!=w],'original':x.hex(),'compiled':y.hex()})
   errors.append(error);break
 result={'cases':i+1,'mismatches':errors,'scope':__doc__,'object':row['object'],'object_sha256':sha(row['object']),'script_sha256':sha(__file__),'actual_inputs_sha256':sha(OUT/'native-final/actual-inputs.json')};(OUT/'quadric-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('Quadric',i+1,'cases,',len(errors),'mismatches',flush=True)
 if errors:print(str(errors[0])[:6000])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
