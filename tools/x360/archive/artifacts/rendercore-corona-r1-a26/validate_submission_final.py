from corona_fixture_final import *
import math,random
SELF,AUX=F.M.S.SELF,F.M.S.AUX
IFACE,PARAM,POS,DIR,SIZE,ENUM=[SELF+n for n in [0x1000,0x2000,0x2100,0x2200,0x2300,0x2400]]
DATA_A,DATA_B=AUX+0x1000,AUX+0xb000
METHODS={
 'params':('?AddCorona@BrnSubmissionInterface@BrnCoronaManager@@QAAXVVector3@vpu@math@rw@@0MMABU',0x823fd270,'?AddParams@CoronaAbi@'),
 'enum':('?AddCorona@BrnSubmissionInterface@BrnCoronaManager@@QAAXVVector3@vpu@math@rw@@0MMABW4',0x823fd428,'?AddEnum@CoronaAbi@'),
 'prop':('?AddPropCorona@BrnSubmissionInterface@BrnCoronaManager@@',0x823fd138,'?AddProp@CoronaAbi@')}
NAMES=['BrnCoronaManager.cpp','Curves.cpp','rwgcoronabufferiterator.cpp','core_probe.cpp']
b=bundle(NAMES);rng=random.Random(0x823fd270);errors=[];checks={};windows={}
# The actual typed CRT final bytes are separately diagnosed. This boundary test
# starts after startup; it does not waive its differing pre-CRT image/writes.
phase=json.loads((OUT/'final-table-crt-comparison.json').read_text());assert not phase['final_state_errors'] and phase['pre_CRT_image_matches']
old=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/corona-original-init.json').read_text());table=(OUT/'native-initialized-table.bin').read_bytes()
expected_file='d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Graphics/BrnCoronaManager.cpp'
expr='mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>( KI_MAX_CORONAS )'
def cstring(c,p):
 raw=bytearray()
 while True:
  v=c.mem.rd(p+len(raw),1)
  if v==b'\0':return raw.decode()
  raw+=v
for via in [False,True]:
 for kind,(prefix,original,caller) in METHODS.items():
  select(b,caller if via else prefix,True);images={k:bytes(v) for k,v in b.images.items()};callee=entry(b,prefix);count=128 if via or kind!='params' else 256
  for i in range(count):
   iface=bytearray(rng.randbytes(0x70));index=[0,1,511,512,513][i%5];struct.pack_into('>III',iface,4,DATA_A,index,512)
   camera=[rng.uniform(-500,500) for _ in range(3)];struct.pack_into('>3fI',iface,0x50,*camera,rng.getrandbits(32))
   distance=[0.,10.,20.,100.,219.,220.,221.,249.,250.,251.,400.,1e5][i%12]
   delta=[distance,0.,0.];position=struct.pack('>3fI',camera[0]+distance,camera[1],camera[2],rng.getrandbits(32));direction=struct.pack('>3fI',[-1.,1.,0.][i%3],0.,0.,rng.getrandbits(32))
   if i%31==0:position=struct.pack('>I',0x7fc01234)+position[4:]
   params=bytearray(rng.randbytes(0x30));struct.pack_into('>4fif5f',params,0,.25,.5,1.,-1.,i%12,.5,400.,1400.,22500.,.75,2.25)
   type_value=i%25;size=struct.pack('>2f',.75,2.25);scale=[0.,.5,1.,-1.,4.][i%5];opacity=[0.,1.,.5,-1.,2.][i%5];colour=rng.getrandbits(32);guard=(rng.getrandbits(32)&~1)|(i&1);seed_a=rng.randbytes(0x9000);seed_b=rng.randbytes(0x9000);both=[]
   for native in [False,True]:
    b.images={k:bytearray(v) for k,v in images.items()}
    if via and not native:original_branch(b,NAMES.index('core_probe.cpp'),prefix,original)
    c=cpu(b);c.mem.wr(IFACE,iface);c.mem.wr(PARAM,params);c.mem.wr(POS,position);c.mem.wr(DIR,direction);c.mem.wr(SIZE,size);put(c,ENUM,type_value);c.mem.wr(DATA_A,seed_a);c.mem.wr(DATA_B,seed_b);c.mem.wr(0x82f24310,table);put(c,0x82fb0040,colour);put(c,0x82fb0044,guard);put(c,0x82f242d0,colour)
    sp=ptr(c,1);c.mem.wr(sp-0x4000,bytes([i&255])*0x4000)
    for reg in range(3,11):c.setg(reg,0xa500000000000000|(i<<8)|reg)
    c.setg(3,IFACE);c.setf(1,scale if kind!='prop' else opacity);c.setf(2,opacity)
    if via:
     c.setg(4,POS);c.setg(5,DIR)
     if kind=='prop':c.setg(6,SIZE);c.setg(8,i%12)
     else:c.setg(8,ENUM if kind=='enum' else PARAM)
    else:
     c.vr[1]=struct.unpack('>4I',position);c.vr[2]=struct.unpack('>4I',direction)
     if kind=='prop':c.setg(4,SIZE);c.setg(6,i%12)
     else:c.setg(6,ENUM if kind=='enum' else PARAM)
    trace=[]
    def begin(q):trace.append(('begin',))
    def fire(q):trace.append(('fire',cstring(q,ptr(q,3)),cstring(q,ptr(q,4)),ptr(q,5)))
    def end(q):
     trace.append(('end',))
     if i%2:
      put(q,IFACE+4,DATA_B);put(q,IFACE+8,3)
      # Parameter loads after assertion must observe these changes.
      put(q,PARAM+16,11);q.mem.wr(PARAM+20,struct.pack('>f',-.25));q.mem.wr(PARAM,struct.pack('>4f',2.,3.,4.,5.))
    for address,fn in [(0x82817548,begin),(0x82820810,fire),(0x82817558,end)]:c.hooks[address]=fn
    for address,name in b.unresolved.items():
     if name.startswith('?BeginAssert@'):c.hooks[address]=begin
     elif name.startswith('?FireAssert@'):c.hooks[address]=fire
     elif name.startswith('?EndAssert@'):c.hooks[address]=end
    seen=[];execute=c.exec_vector
    def observe(pc,word,mnemonic):
     if pc==(callee if native else original):
      vecs=[struct.pack('>4I',*c.vr[n]).hex() for n in [1,2]];assert vecs==[position.hex(),direction.hex()] and ptr(c,3)==IFACE
      expected=SIZE if kind=='prop' else (ENUM if kind=='enum' else PARAM)
      assert ptr(c,4 if kind=='prop' else 6)==expected
      assert c.f(1)==(opacity if kind=='prop' else scale)
      if kind!='prop':assert c.f(2)==opacity
      seen.append({'r3':ptr(c,3),'v1':vecs[0],'v2':vecs[1],'f1':c.f(1),'f2':c.f(2),'r4':ptr(c,4),'r6':ptr(c,6)})
     return execute(pc,word,mnemonic)
    c.exec_vector=observe;c.run(b.entry if via or native else original)
    if via:assert len(seen)==1
    expected_line=0x215 if kind=='prop' else 0x251
    for event in trace:
     if event[0]=='fire':assert event[1:]==(expr,expected_file,expected_line),event
    both.append((c.mem.rd(IFACE,0x70),c.mem.rd(PARAM,0x30),c.mem.rd(DATA_A,0x9000),c.mem.rd(DATA_B,0x9000),c.mem.rd(0x82fb0040,8),c.mem.rd(0x82f242d0,4),trace))
    if via and native and i<2:windows.setdefault(kind,[]).extend(seen)
   if both[0]!=both[1]:
    errors.append({'kind':kind,'via_typed_caller':via,'case':i,'different_fields':[n for n,(a,z) in enumerate(zip(both[0],both[1])) if a!=z],'trace':[x[-1] for x in both]})
   if i%64==0:print('progress',via,kind,i,flush=True)
  checks[('typed_caller_' if via else 'complete_direct_')+kind]=count
report={'checks':checks,'cases':sum(checks.values()),'errors':errors,'windows':windows,'objects':{n:pin(PATHS[n]) for n in NAMES},'scope':'Complete native by-value AddCorona parameter and enum overloads/AddPropCorona versus ARTIST, plus same real emitted typed caller -> native provider versus original callee. Retail v1/v2/f1/f2 and r6 or packed-FPU r4 are independently asserted; no argument adaptation. Real Curves, Iterator, RW colour ctor, lookup providers execute. All interface/parameter/full target arrays/lazy colour state/guard and assert metadata/continuation compare, including callback-mutated pointer/index/type lanes.','limits':['Post-startup table bytes come from the independently matched actual native CRT; startup system ordering beyond the two original local initializers is not a game-launch claim.','Software VMX/FPU/NaN/integer-conversion model and controlled assertion services; no hardware, physical GPU or complete game claim.','Typed pointer-source probe is not the whole RaceCar Render or World producer; actual owner caller-object closure is separate.']}
p=emit('final-submission-providers-comparison.json',report);print(json.dumps({'checks':checks,'cases':report['cases'],'errors':len(errors),'first':errors[:4],'report':str(p)},indent=2));raise SystemExit(bool(errors))
