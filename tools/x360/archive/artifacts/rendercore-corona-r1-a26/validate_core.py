from corona_fixture import *
import itertools,math,random,warnings
warnings.filterwarnings('ignore')
SELF,AUX=F.M.S.SELF,F.M.S.AUX
PARAM,SCALE,DEST,RESOURCE,ITER,DATA,POS,DIR,SIZE=[SELF+n for n in [0x1000,0x1100,0x1200,0x1300,0x1400,0x2000,0x1500,0x1600,0x1700]]
rng=random.Random(0x823f3350);errors=[];checks={}
# Complete packed scalar curve provider, with exact FMA result bytes.
b=bundle(['Curves.cpp']);select(b,'?Evaluate@SmoothStep@Curves@BrnEffects@@SAM')
cases=[]
for i in range(1024):
 a=rng.uniform(-1e4,1e4);m=a+rng.uniform(.01,100);z=m+rng.uniform(.01,100)
 cases.append(((a,m,z),(rng.uniform(-20,20),rng.uniform(-20,20)),rng.choice([a,m,z,rng.uniform(a-100,z+100)])))
cases+=list(itertools.product([(0.,0.,0.),(0.,0.,1.),(0.,1.,1.),(-1.,0.,1.),(-math.inf,0.,math.inf)],[(0.,1.),(-0.,0.),(-5.,7.),(math.inf,1.),(math.nan,1.)],[-math.inf,-1.,-0.,0.,.5,1.,math.inf,math.nan]))
for i,(params,scale,value) in enumerate(cases):
 both=[]
 for native in [False,True]:
  c=cpu(b);c.mem.wr(PARAM,struct.pack('>3f',*params));c.mem.wr(SCALE,struct.pack('>2f',*scale));c.setg(3,PARAM);c.setg(4,SCALE);c.setf(1,value);c.run(b.entry if native else 0x8227d2f0);both.append((struct.pack('>f',c.f(1)),c.mem.rd(PARAM,12),c.mem.rd(SCALE,8)))
 if both[0]!=both[1]:errors.append({'kind':'curve','case':i,'returns':[x[0].hex() for x in both]})
checks['complete_curve']=len(cases)
# Actual five-lane descriptor and Initialize providers, with real constructor linked.
b=bundle(['rwgcoronabuffer.cpp','BaseResourceDescriptor.cpp'])
for method,address in [('GetResourceDescriptor',0x8228d6c0),('Initialize',0x822850d8)]:
 select(b,'?'+method+'@CoronaBuffer@')
 for i in range(256):
  count=[0,1,2,511,512,0xffffffff,0x40000000,0x80000000][i%8] if i<64 else rng.getrandbits(32);seed=rng.randbytes(0x100);resources=bytearray(rng.randbytes(20));base=DATA+(i%4)*4;struct.pack_into('>I',resources,0,base);both=[]
  for native in [False,True]:
   c=cpu(b);c.mem.wr(DEST,seed);c.mem.wr(DATA-16,seed);c.mem.wr(RESOURCE,resources);put(c,PARAM,count);c.setg(3,DEST if method=='GetResourceDescriptor' else RESOURCE);c.setg(4,PARAM);c.run(b.entry if native else address);both.append((ptr(c,3),c.mem.rd(DEST,0x100),c.mem.rd(DATA-16,0x100),c.mem.rd(RESOURCE,20)))
  if both[0]!=both[1]:errors.append({'kind':method,'case':i})
 checks['complete_buffer_'+method]=256
# Native iterator input has full raw v1/v2/v3 payloads, f1, ABGR r5 and texture r6.
b=bundle(['rwgcoronabufferiterator.cpp']);select(b,'?Write@Iterator@CoronaBuffer@')
for i in range(512):
 index=i%16;data=rng.randbytes(0x1000);vectors=[rng.randbytes(16) for _ in range(3)];distance=rng.uniform(-100,100);colour=rng.getrandbits(32);texture=rng.getrandbits(32);both=[]
 for native in [False,True]:
  c=cpu(b);c.mem.wr(DATA,data);c.mem.wr(ITER,struct.pack('>III',DATA,index,512));c.setg(3,ITER);c.setg(5,colour);c.setg(6,texture);c.setf(1,distance)
  for reg,raw in zip([1,2,3],vectors):c.vr[reg]=struct.unpack('>4I',raw)
  c.run(b.entry if native else 0x823f3350);both.append((c.mem.rd(ITER,12),c.mem.rd(DATA,0x1000)))
 if both[0]!=both[1]:errors.append({'kind':'iterator','case':i})
checks['complete_iterator']=512
# Real emitted pointer-source caller -> complete actual Iterator or original target.
b=bundle(['core_probe.cpp','rwgcoronabufferiterator.cpp']);select(b,'?Iterator@CoronaAbi@');images={k:bytes(v) for k,v in b.images.items()};callee=entry(b,'?Write@Iterator@CoronaBuffer@')
for i in range(128):
 data=rng.randbytes(0x1000);vectors=[rng.randbytes(16) for _ in range(3)];distance=rng.uniform(-100,100);colour=rng.getrandbits(32);texture=rng.getrandbits(32);both=[]
 for native in [False,True]:
  b.images={k:bytearray(v) for k,v in images.items()}
  if not native:original_branch(b,0,'?Write@Iterator@CoronaBuffer@',0x823f3350)
  c=cpu(b);c.mem.wr(DATA,data);c.mem.wr(ITER,struct.pack('>III',DATA,i%16,512));c.mem.wr(POS,vectors[0]);c.mem.wr(DIR,vectors[1]);c.mem.wr(SIZE,vectors[2]);c.setg(3,ITER);c.setg(4,POS);c.setg(5,DIR);c.setg(6,SIZE);c.setf(1,distance);c.setg(8,colour);c.setg(9,texture)
  seen=[];execute=c.exec_vector
  def observe(pc,word,mnemonic):
   if pc==(callee if native else 0x823f3350):
    raw=[struct.pack('>4I',*c.vr[n]) for n in [1,2,3]];assert raw==vectors and ptr(c,3)==ITER and ptr(c,5)==colour and ptr(c,6)==texture;seen.append(1)
   return execute(pc,word,mnemonic)
  c.exec_vector=observe;c.run(b.entry);assert len(seen)==1;both.append((c.mem.rd(ITER,12),c.mem.rd(DATA,0x1000)))
 if both[0]!=both[1]:errors.append({'kind':'iterator_emitted_caller','case':i})
checks['emitted_iterator_caller']=128
report={'checks':checks,'cases':sum(checks.values()),'errors':errors,'objects':{k:pin(v) for k,v in PATHS.items() if k in ['Curves.cpp','rwgcoronabuffer.cpp','rwgcoronabufferiterator.cpp','BaseResourceDescriptor.cpp','core_probe.cpp']},'scope':'Complete real packed FPU curve, five-lane buffer descriptor/Initialize and ABGR-to-RGBA8 iterator providers versus ARTIST; actual emitted typed caller asserts native v1/v2/v3/f1/r5/r6 before the complete writer. All defined output bytes and guards compare, with no argument adaptation.','limits':'Shared PPC/VMX software arithmetic model, no hardware/GPU/game link. Curve special values use modeled IEEE behavior; FPU constructors/accessor general surface and global table/CRT are not certified by this report.'}
p=emit('core-providers-comparison.json',report);print(json.dumps({'checks':checks,'errors':len(errors),'first':errors[:3],'report':str(p)},indent=2));raise SystemExit(bool(errors))
