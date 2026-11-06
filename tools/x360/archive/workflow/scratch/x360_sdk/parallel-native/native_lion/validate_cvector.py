"""Compare actual native COFF/listing dataflow with ARTIST inline cVector streams.
No execution on hardware; scalar/vector expression kinds and order stay distinct.
"""
from pathlib import Path
import sys,json,re,struct,hashlib
HERE=Path(__file__).parent
PARENT=HERE.parents[3]
sys.path.insert(0,str(PARENT/'tools/re'))
import vmx128,x360rd
OUT=PARENT/'b5-decomp/build/x360/parallel/native_lion/cvector-final'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def coff(data):
 machine,nsec,_,symptr,nsyms,opt,_=struct.unpack_from('<HHLLLHH',data);assert machine==0x1f2
 sections=[]; rels=[]
 for i in range(nsec):
  off=20+opt+40*i;size,ptr=struct.unpack_from('<LL',data,off+16);rp=struct.unpack_from('<L',data,off+24)[0];nr=struct.unpack_from('<H',data,off+32)[0]
  sections.append(data[ptr:ptr+size]);rels.append([struct.unpack_from('<LLH',data,rp+10*j) for j in range(nr)])
 strings=symptr+18*nsyms;syms={};names={};loc={};i=0
 while i<nsyms:
  off=symptr+18*i;raw=data[off:off+8]
  if raw[:4]==b'\0'*4:
   b=strings+struct.unpack_from('<L',raw,4)[0];name=data[b:data.index(b'\0',b)].decode()
  else:name=raw.rstrip(b'\0').decode()
  val,sec,_,_,aux=struct.unpack_from('<LhHBB',data,off+8);names[i]=name
  if sec>0:syms[name]=sections[sec-1][val:];loc[name]=(sec-1,val)
  i+=1+aux
 return syms,{n:{o-v:(names[k],t) for o,k,t in rels[s]} for n,(s,v) in loc.items()}
asm=next(OUT.glob('cvector_probe-*.asm'));obj=next(OUT.glob('cvector_probe-*.obj'))
symbols,relocs=coff(obj.read_bytes());listing=asm.read_text(errors='replace');functions={}
for m in re.finditer(r'^(\S+)\s+PROC\b(.*?)^\1\s+ENDP',listing,re.M|re.S):
 ins=[]
 for line in m[2].splitlines():
  q=re.match(r'\s+([0-9a-f]{5})\s+([0-9a-f]{8})\s+(\S+)',line)
  if q:
   off,w=int(q[1],16),int(q[2],16);actual=int.from_bytes(symbols[m[1]][off:off+4],'big')
   if actual!=w:
    assert off in relocs[m[1]] and q[3] in ('b','bl'),(m[1],off)
    assert actual==(w|((-off)&0x3fffffc)),(m[1],off,hex(w),hex(actual))
   ins.append((off,w,q[3]))
 functions[m[1]]=ins

def expr(kind,a,b):
 if kind in ('scalar_add','vector_add','vector_mul'):a,b=sorted((a,b),key=repr)
 return (kind,a,b)
class VM:
 def __init__(self):self.r={0:0,1:0x10000,3:0x20000,4:0x30000,5:0x40000,6:0x50000};self.f={1:'t'};self.v={};self.mem={}
 def store(self,a,lanes):
  for i,x in enumerate(lanes):self.mem[a+4*i]=x
 def load(self,a,n=4):return tuple(self.mem[a+4*i] for i in range(n))
 def step(self,w,name):
  d,a,b=(w>>21)&31,(w>>16)&31,(w>>11)&31;imm=w&65535;imm=imm-65536 if imm>=32768 else imm
  if name.endswith('128'):
   v=vmx128.decode(w);vd,va,vb=v['vD'],v['vA'],v['vB']
  else:vd,va,vb=d,a,b
  name=name.removesuffix('128')
  if name=='addi':self.r[d]=(self.r[a] if a else 0)+imm
  elif name=='mr':self.r[a]=self.r[d]
  elif name=='ld':self.r[d]=self.load(self.r[a]+((w&65532)-65536 if (w&32768) else w&65532),2)
  elif name=='std':
   assert isinstance(self.r[d],tuple),(d,self.r[d]);self.store(self.r[a]+((w&65532)-65536 if (w&32768) else w&65532),self.r[d])
  elif name=='lfs':self.f[d]=self.mem[self.r[a]+imm]
  elif name=='stfs':self.mem[self.r[a]+imm]=self.f[d]
  elif name in ('fsubs','fadds'):self.f[d]=expr('scalar_sub' if name=='fsubs' else 'scalar_add',self.f[a],self.f[b])
  elif name in ('lvx','stvx'):
   addr=((self.r[va&31] if va&31 else 0)+self.r[vb&31])&~15
   if name=='lvx':self.v[vd]=self.load(addr)
   else:self.store(addr,self.v[vd])
  elif name in ('vmulfp','vsubfp','vaddfp'):
   kind={'vmulfp':'vector_mul','vsubfp':'vector_sub','vaddfp':'vector_add'}[name];self.v[vd]=tuple(expr(kind,x,y) for x,y in zip(self.v[va],self.v[vb]))
  elif name=='clrlwi':self.r[a]=self.r.get(d,0)&((1<<(32-b))-1)
  elif name in ('mflr','blr'):pass
  else:raise ValueError(name)
 def run(self,name):
  for off,w,mn in functions[name]:
   if mn in ('b','bl'):
    target=relocs[name][off][0]
    if target.startswith(('__savegprlr_','__restgprlr_')):continue
    assert mn=='b' and target in functions,target
    self.run(target);return
   self.step(w,mn)

def expected(kind,a,b,t,d):
 if kind=='Add':return tuple(expr('scalar_add',a[i],b[i]) if i<3 else d[3] for i in range(4))
 if kind in ('Mul','Mul4','Add4','Sub4'):
  op={'Mul':'vector_mul','Mul4':'vector_mul','Add4':'vector_add','Sub4':'vector_sub'}[kind];return tuple(expr(op,x,y) for x,y in zip(a,b))
 if kind=='Lerp':return tuple(expr('scalar_add',a[i],expr('vector_mul',expr('scalar_sub',b[i],a[i]),t[i])) if i<3 else expr('vector_mul',d[3],t[3]) for i in range(4))
 if kind=='Lerp4':return tuple(expr('vector_add',expr('vector_mul',expr('vector_sub',b[i],a[i]),t[i]),a[i]) for i in range(4))
 raise ValueError(kind)
results=[]
for kind in ['Add','Mul','Lerp','Add4','Sub4','Mul4','Lerp4']:
 for alias in ['distinct','a','b','t']:
  vm=VM()
  for address,label in [(0x20000,'d'),(0x30000,'a'),(0x40000,'b'),(0x50000,'t')]:vm.store(address,tuple(label+str(i) for i in range(4)))
  if alias!='distinct':vm.r[3]={'a':0x30000,'b':0x40000,'t':0x50000}[alias]
  a,b,t,d=vm.load(vm.r[4]),vm.load(vm.r[5]),vm.load(vm.r[6]),vm.load(vm.r[3]);want=expected(kind,a,b,t,d)
  vm.run('ProbeVector'+kind);got=vm.load(vm.r[3]);assert got==want,(kind,alias,got,want)
  results.append({'probe':kind,'output_alias':alias,'pass':True})
# Bind the two ARTIST inline vector families to raw calibrated instruction words.
assert x360rd.rd(0x82815570,4)==bytes.fromhex('7C8A2378')
p=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8290B1F8.json');artist=json.loads(p.read_text());checks=[]
for kind,low,high in [('Lerp',0x8290B204,0x8290B298),('Lerp4',0x8290C3C0,0x8290C3E8)]:
 vm=VM();vm.store(0x20000,tuple('d'+str(i) for i in range(4)));vm.store(0x30000,tuple('a'+str(i) for i in range(4)));vm.store(0x40000,tuple('b'+str(i) for i in range(4)));vm.store(0x50000,('t',)*4)
 a,b,t,d=vm.load(0x30000),vm.load(0x40000),vm.load(0x50000),vm.load(0x20000)
 if kind=='Lerp4':vm.r.update({9:0x30000,10:0x20000,11:0,22:0});vm.v[0]=t
 words=[]
 for line in artist['assembly'].splitlines():
  q=line.split();addr=int(q[0],16)
  if low<=addr<high:
   word=int.from_bytes(x360rd.rd(addr,4),'big');vm.step(word,q[1]);words.append({'address':hex(addr),'word':hex(word),'mnemonic':q[1]})
 assert vm.load(0x20000)==expected(kind,a,b,t,d),(kind,vm.load(0x20000),expected(kind,a,b,t,d))
 checks.append({'family':kind,'artist_words':words,'expression_match':True})
(HERE/'cvector-dataflow-validation.json').write_text(json.dumps({'kind':'symbolic-instruction-dataflow','not_hardware_execution':True,'normalization':'commutative operand exchange only; no reassociation; no NaN-payload hardware claim','native_coff':str(obj),'native_coff_sha256':sha(obj),'native_listing_sha256':sha(asm),'artist_sha256':sha(p),'cases':results,'artist_inline_comparisons':checks},indent=2)+'\n')
print('PASS:28 actual native probe/alias dataflow cases and2 ARTIST inline expression comparisons. No hardware execution claim.')
