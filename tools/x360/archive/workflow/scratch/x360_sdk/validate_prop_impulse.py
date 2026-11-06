import copy,json,math,random,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import validate_native_math as M
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-vehicle-validation';OUT.mkdir(exist_ok=True)
r=json.loads((ROOT/'build/x360/native-vehicle-layout/compile-results.json').read_text())
obj=Path(next(x['object'] for x in r['results'] if x['source'].endswith('/RaceCarPhysics.cpp')));data=obj.read_bytes()
machine,nsec,stamp,symoff,nsyms,optsize,flags=struct.unpack_from('<HHIIIHH',data);assert machine==0x1f2
strings=data[symoff+nsyms*18:]
def name(raw):
 if raw[:4]==b'\0'*4:
  at=struct.unpack_from('<I',raw,4)[0];return strings[at:strings.index(b'\0',at)].decode()
 return raw.rstrip(b'\0').decode()
sections={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}
symbols={};i=0
while i<nsyms:
 raw,val,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18);symbols[i]=(name(raw),val,sec);i+=1+aux
fn=next(v for v in symbols.values() if v[0].startswith('?ApplyPropCollisionImpulseSum@') and v[2]>0)
TRAP=0x95000000;ADD=0x825be8f8
used=set();pending=[fn[2]]
def relocations(sec):
 s=sections[sec]
 return [struct.unpack_from('<IIH',data,s[5]+j*10) for j in range(s[7])]
while pending:
 sec=pending.pop()
 if sec in used:continue
 used.add(sec)
 for at,index,kind in relocations(sec):
  if kind==0x12:assert index==0;continue
  symbol,value,target=symbols[index]
  if target>0:pending.append(target)
  else:assert symbol.startswith('?AddWorldSpaceImpulse@ExternalPhysicsBody@'),symbol
bases={sec:0x94000000+i*0x1000 for i,sec in enumerate(sorted(used))};images={}
for sec in used:
 s=sections[sec];assert s[3]<=0x1000
 body=bytearray(data[s[4]:s[4]+s[3]])
 for at,index,kind in relocations(sec):
  if kind==0x12:continue
  symbol,value,target=symbols[index];address=bases[target]+value if target>0 else TRAP
  word=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:
   assert word&0xffff==0
   immediate=((address+0x8000)>>16) if kind==0x10 else address
   word=(word&0xffff0000)|(immediate&0xffff)
  elif kind==6:
   delta=address-(bases[sec]+at);assert -(1<<25)<=delta<(1<<25)
   word=(word&0xfc000003)|(delta&0x03fffffc)
  else:raise AssertionError(('Unsupported relocation',kind))
  struct.pack_into('>I',body,at,word)
 images[bases[sec]]=body
class CPU(M.CPU):
 def run(self,pc,depth=0):
  if pc in [TRAP,ADD]:
   if pc==TRAP:
    a=self.g(4)&0xffffffffffffffff;b=self.g(5)&0xffffffffffffffff
    vector=(a>>32,a&0xffffffff,b>>32,b&0xffffffff)
   else:vector=self.vr[1]
   self.impulse_observations.append((self.g(3)&0xffffffff,tuple(vector)));return
  return super().run(pc,depth)
def cpu():
 c=M.new_cpu();c.__class__=CPU;c.impulse_observations=[];c.mem.add(0x82fb8000,0x2000)
 for at,body in images.items():c.mem.add(at,len(body),body)
 for init in [0x82c5d0c0,0x82c5d0e8,0x82c5d110,0x82c5d140,0x82c5d168]:c.run(init)
 for at,value in [(0x82fb91c0,.05),(0x82fb8430,.2),(0x82fb8b80,55),(0x82fb9c00,30)]:
  assert struct.unpack('>4I',c.mem.rd(at,16))==(M.bits(value),)*4
 return c
base={'impulse':[100.,50.,-20.,3.],'up':[0.,1.,0.,0.],'velocity':[4.,2.,1.,0.],'mass':[1500.]*4,'speed':[35.]*4,'traction':True}
fields={'impulse':0x13f0,'up':0x20,'velocity':0x50,'mass':0xe0,'speed':0x6c0}
cases=[]
special=[0.,-0.,1.,-1.,.5,M.value(1),M.value(0x80000001),M.value(0x00800000),math.inf,-math.inf,math.nan]
for field in fields:
 for value in special:
  for traction in [False,True]:
   case=copy.deepcopy(base);case[field]=[value]*4;case['traction']=traction;cases.append(case)
random.seed(360)
for i in range(400):
 case={k:[M.value(M.bits(random.uniform(-100,100))) for _ in range(4)] for k in fields}
 case['mass']=[M.value(M.bits(random.uniform(1,2500))) for _ in range(4)];case['traction']=bool(i%2);cases.append(case)
errors=[]
for index,case in enumerate(cases):
 outputs=[]
 for compiled in [False,True]:
  c=cpu();c.mem.wr(M.S.SELF,b'\0'*0x1460)
  for field,offset in fields.items():c.mem.wr(M.S.SELF+offset,struct.pack('>4f',*case[field]))
  c.mem.wr(M.S.SELF+0x135b,bytes([case['traction']]))
  c.setg(3,M.S.SELF);c.run(bases[fn[2]]+fn[1] if compiled else 0x82600780)
  assert len(c.impulse_observations)==1,c.impulse_observations
  outputs.append((c.impulse_observations[0],c.mem.rd(M.S.SELF+0x13f0,16)))
 (pa,va),za=outputs[0];(pb,vb),zb=outputs[1]
 same=pa==pb and za==zb and all(M.same(M.value(a),M.value(b)) for a,b in zip(va,vb))
 if not same:errors.append({'case':index,'input':case,'original':[hex(v) for v in va],'compiled':[hex(v) for v in vb],'this':[hex(pa),hex(pb)],'clear':[za.hex(),zb.hex()]})
 if index%100==0:print('Prop cases',index+1,'checked',flush=True)
report={'cases':len(cases),'mismatches':errors,'object':str(obj),'selected_function':fn[0],'sections':len(used),'target':'actual compiled PPC COFF vs original ARTIST body','call_boundary':'AddWorldSpaceImpulse call observed, not executed; native C++ GPR aggregate marshalled to original VMX value','hardware_estimates':'shared interpreter models','nan_payloads_compared':False,'game_booted':False}
(OUT/'prop-impulse-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('Prop impulse',len(cases),'cases, mismatches',len(errors))
if errors:print(json.dumps(errors[:4],indent=2))
raise SystemExit(bool(errors))
