from corona_fixture_final import *
import math,random
SELF=F.M.S.SELF;DEST=SELF+0x1000;SRC=SELF+0x2000
b=bundle(['core_probe.cpp']);rng=random.Random(0x821f05b0);errors=[];checks={}
for kind,address,prefix,count in [('RGBA',0x821f05b0,'?RgbaCtor@CoronaAbi@',512),('VPU2',0x821f0db0,'?Vpu2Ctor@CoronaAbi@',512)]:
 select(b,prefix)
 for i in range(count):
  seed=rng.randbytes(64);channels=[rng.randrange(256) for _ in range(4)];values=[-0.,0.,1.,-1.,math.inf,-math.inf,math.nan,1e-38] if i<64 else [rng.uniform(-1e4,1e4) for _ in range(8)];both=[]
  for native in [False,True]:
   c=cpu(b);c.mem.wr(DEST-16,seed);c.setg(3,DEST)
   if kind=='RGBA':
    for reg,v in zip([4,5,6,7],channels):c.setg(reg,v)
   else:c.setf(1,values[i%8]);c.setf(2,values[(i+3)%8])
   c.run(b.entry if native else address);both.append((ptr(c,3),c.mem.rd(DEST-16,64)))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'returns':[x[0] for x in both],'different_bytes':[n for n,(x,y) in enumerate(zip(both[0][1],both[1][1])) if x!=y]})
 checks['complete_native_'+kind+'_ctor']=count
# The real packed Curve constructor is bound to the exact ten-instruction
# original CRT field-copy window. This is explicitly not a standalone retail ctor.
select(b,'?CurveCtor@CoronaAbi@')
for i in range(256):
 source=rng.randbytes(20);seed=rng.randbytes(64);both=[]
 for native in [False,True]:
  c=cpu(b);c.mem.wr(SRC,source);c.mem.wr(DEST-16,seed)
  if native:c.setg(3,DEST);c.setg(4,SRC);c.setg(5,SRC+12);start=b.entry
  else:
   c.setg(9,SRC+20);c.setg(11,DEST-0xd8);start=0x82c4f4e8
   original_step=c.exec_vector
   def stop(pc,word,mnemonic):
    if pc==0x82c4f510:raise F.M.V.Return()
    return original_step(pc,word,mnemonic)
   c.exec_vector=stop
  c.run(start);both.append(c.mem.rd(DEST-16,64))
 if both[0]!=both[1]:errors.append({'kind':'actual_curve_ctor_vs_original_copy_window','case':i,'different_bytes':[n for n,(x,y) in enumerate(zip(*both)) if x!=y]})
checks['curve_ctor_original_field_copy_window']=256
report={'checks':checks,'cases':sum(checks.values()),'errors':errors,'objects':{'actual_header_probe':pin(PATHS['core_probe.cpp'])},'scope':'Actual header-emitted complete RGBA four-channel and VPU2(float,float) constructors versus their complete ARTIST821F05B0/821F0DB0 bodies. Raw destination bytes, guards and returned this pointers compare. The genuine20-byte FPU Curve constructor is separately compared to exact original CRT copy instructions82C4F4E8..50C, with its register-window setup explicit.','limits':'RGBA inputs are valid zero-extended8-bit parameters. Scalar FP/NaN behavior is the shared software model. Curve copy-window evidence is not a standalone original constructor or general by-value FPU ABI proof; no hardware/game claim.'}
p=emit('final-constructor-comparison.json',report);print(json.dumps({'checks':checks,'errors':len(errors),'first':errors[:3]},indent=2));raise SystemExit(bool(errors))
