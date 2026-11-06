"""Original/product full World shader publisher through three actual setter providers.

Environment outputs and DispatchBin allocation are controlled interfaces; original
CRT mask initialization, quadric packer, table dirty bookkeeping, frame setters and
World dispatch-output setters execute. World's known +0x7900 contained-member drift
is explicitly represented for the environment receiver/curve read; this is a
semantic caller/ABI/writes witness, NOT native whole-World layout or strict-link proof.
The missing global-storage CPP provider is separately recorded as a compile failure.
"""
from pathlib import Path
import hashlib,json,random,struct,sys,time
import numpy as np
OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F,Obj,Bundle
from validate_vpu_mask_startup import initialise
STACK=0x40000000;WORLD=0x50000000;VIEW=0x51000020;CAM=0x52000020;FRAME=0x54000020;DISPATCH=0x55000020;BIN=0x56000000;POOL=0x57000000;TABLE=0x830113d0
MASK=0xffffffff
SIZES=[16]*20+[4]+[64]*3+[16]*2

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def put(m,a,v,n=4):m.wr(a,(v&((1<<(n*8))-1)).to_bytes(n,'big'))
def prepare_bundle(rows):
 objects=[Obj(r['object'])for r in rows];b=Bundle(objects);b.select('?SetupShaderConstantsBeforeRendering@WorldModule@')
 name='?mShaderConstantTable@CgsGraphics@@3UShaderConstantTable@@A';old=b.ext(name)
 # Bind this real external global to its original home for this fixture only.
 # Its actual source storage provider remains uncompiled and is not fabricated.
 for key,base in b.bases.items():
  o=objects[key[0]];image=b.images[base]
  for at,ix,typ in o.rels(key[1]):
   if o.symbols[ix][0]!=name:continue
   w=struct.unpack_from('>I',image,at)[0]
   if typ in(0x10,0x11):w=(w&0xffff0000)|((((TABLE+0x8000)>>16)if typ==0x10 else TABLE)&0xffff)
   elif typ==2:w=(w-old+TABLE)&MASK
   else:raise AssertionError((typ,name))
   struct.pack_into('>I',image,at,w)
 del b.unresolved[old]
 return b,objects

def inputs(index):
 rng=random.Random(0x827d1410+index);outputs=[]
 for i,n in enumerate(SIZES):
  words=[rng.uniform(-2.0,3.0)for _ in range(n//4)]
  if n==4:words=[(0.,0.125,0.5,1.,1.5,3.0)[index%6]]
  outputs.append(struct.pack('>'+'f'*len(words),*words))
 return outputs

def execute(b,compiled,index,layout):
 rng=random.Random(0x51410+index);m=F.M.V.Mem();m.add(STACK,0x10000);m.add(0x8327f000,0x1000);m.add(0x83011000,0x2000);m.add(0x84800000,0x10000)
 for a,n in[(VIEW-0x20,0x100),(CAM-0x20,0x100),(FRAME-0x20,0x400),(DISPATCH-0x20,0x200),(BIN,0x100),(POOL,0x4000)]:m.add(a,n,rng.randbytes(n))
 curve=(0.125,1.0,2.25,-0.5)[index%4]
 actual_curve=layout['environment_offset']+layout['environment_curve_offset']
 for off in[0x1e7650,actual_curve]:
  page=(WORLD+off)&~0xfff;m.add(page,0x1000);m.wr(WORLD+off,struct.pack('>f',curve))
 if compiled:
  for a,data in b.images.items():m.add(a,len(data),data)
 c=F.AtomicCPU(m);c.setg(1,STACK+0x8000);c.max_steps=100000;c.atomic_trace=[];c.fence_trace=[];c.msr=0x8000;c.reservation=None;c.failures=0
 initialise(c)
 for a in[VIEW,CAM]:
  m.wr(a,struct.pack('>16f',*[rng.uniform(-3,3)for _ in range(16)]))
 m.wr(FRAME+0x31c,b'\1');put(m,DISPATCH,9,1)
 # Valid runtime table configuration for exactly the constant IDs this caller uses.
 table=bytearray(rng.randbytes(0x680))
 for i in range(50):
  size=64 if i<5 or i in(18,19)else 16
  struct.pack_into('>HBB',table,i*12,size//16,size,1)
 table[0x578]=35;struct.pack_into('>I',table,0x57c,BIN);table[0x580]=index%8;m.wr(TABLE,table)
 supplied=inputs(index);env_trace=[];alloc_trace=[];cursor=[POOL+0x20];quadric_calls=[0]
 def environment(cpu):
  sp=cpu.g(1)&MASK;args=[cpu.g(r)&MASK for r in range(4,11)]+[int.from_bytes(m.rd(sp+0x54+i*8,4),'big')for i in range(19)]
  expected=WORLD+(layout['environment_offset']if compiled else 0x1e6f60)
  assert cpu.g(3)&MASK==expected,(hex(cpu.g(3)),hex(expected))
  assert len(args)==26 and len(set(args))==26
  for ptr,n,data in zip(args,SIZES,supplied):
   assert STACK<=ptr<STACK+0x10000 and ptr%(4 if n==4 else 16)==0,(hex(ptr),n)
   m.wr(ptr,data)
  env_trace.append(['actual_named_environment_receiver',len(args),SIZES])
 def allocate(cpu):
  receiver=cpu.g(3)&MASK;size=cpu.g(4)&MASK;assert receiver==BIN
  result=cursor[0];cursor[0]+=size*16;assert cursor[0]<POOL+0x4000;alloc_trace.append([receiver,size,result]);cpu.setg(3,result)
 def unexpected(cpu):raise AssertionError(('unexpected original/provider assert',hex(getattr(cpu,'last_pc',0)),cpu.g(3)&MASK))
 if compiled:
  for at,name in b.unresolved.items():
   if name.startswith(('__savevmx','__restvmx')):c.hooks[at]=lambda cpu:None
   elif name.startswith('?GenerateShaderConstants@EnvironmentManager@'):c.hooks[at]=environment
   elif name.startswith('?AllocateMemoryFast@DispatchBin@'):c.hooks[at]=allocate
   else:c.hooks[at]=unexpected
  quadric=b.addr('?GenerateShaderConstantsForQuadricIrradiance@')
 else:
  c.hooks[0x827d0098]=environment;c.hooks[0x822a0620]=allocate
  for at in[0x82817548,0x82820810,0x82817558]:c.hooks[at]=unexpected
  quadric=0x827b0790
 def observe(pc,w):
  if pc==quadric:quadric_calls[0]+=1
 c.instruction_observer=observe
 c.setg(3,WORLD);c.setg(4,VIEW);c.setg(5,CAM);c.setg(8,FRAME);c.setg(9,DISPATCH);c.setf(1,[0.,0.016,1234.5,-17.25][index%4]);c.setf(2,[90000.,-3.,0.,0.033][(index//3)%4])
 try:c.run(b.entry if compiled else 0x827d1410)
 except Exception as e:raise RuntimeError((compiled,index,hex(getattr(c,'last_pc',0))))from e
 assert quadric_calls==[1]
 return{'view':m.rd(VIEW-0x20,0x100).hex(),'camera':m.rd(CAM-0x20,0x100).hex(),'frame':m.rd(FRAME-0x20,0x400).hex(),'dispatch_output':m.rd(DISPATCH-0x20,0x200).hex(),'table':m.rd(TABLE,0x680).hex(),'allocated_data':m.rd(POOL,0x4000).hex(),'environment_interface':env_trace,'allocation_interface':alloc_trace,'actual_quadric_calls':quadric_calls}

def main():
 rows=[json.loads((OUT/'shading-candidate-1/native/compile-results.json').read_text())['results'][0]]
 rows +=[r for r in json.loads((OUT/'real-shading-providers/native/compile-results.json').read_text())['results']if r['passed']]
 b,objects=prepare_bundle(rows);layout=json.loads((OUT/'actual-layout.json').read_text())['values'];errors=[];start=time.monotonic()
 with np.errstate(all='ignore'):
  for i in range(128):
   pair=[execute(b,c,i,layout)for c in(False,True)]
   if pair[0]!=pair[1]:
    error={'case':i,'differences':{}}
    for key in pair[0]:
     if pair[0][key]!=pair[1][key]:
      if key.endswith('interface')or key=='actual_quadric_calls':error['differences'][key]=[x[key]for x in pair]
      else:
       a,z=[bytes.fromhex(x[key])for x in pair];error['differences'][key]={'byte_offsets':[hex(j)for j,(x,y)in enumerate(zip(a,z))if x!=y][:40],'original':a.hex(),'compiled':z.hex()}
    errors.append(error);break
   print('PASS shader publisher',i,flush=True)
 result={'cases':i+1,'mismatches':errors,'elapsed_seconds':time.monotonic()-start,'scope':__doc__,'actual_provider_objects':{str(o.path):sha(o.path)for o in objects},'named_layout_bindings':{'original_environment':'0x1E6F60','compiled_environment':hex(layout['environment_offset']),'curve_member':hex(layout['environment_curve_offset']),'whole_World_layout_pass':False,'negative_layout_receipt':'actual-layout.json'},'script_sha256':sha(__file__)};(OUT/'shader-publisher-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('TOTAL',i+1,'mismatches',len(errors),flush=True)
 if errors:print(str(errors[0])[:6000])
 raise SystemExit(bool(errors))
if __name__=='__main__':main()
