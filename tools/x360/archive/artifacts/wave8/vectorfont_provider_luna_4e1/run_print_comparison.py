import sys,struct,re,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import validate_native_math as M
assert M.V.x360rd.u32(0x82815570)==0x7c8a2378 and M.V.x360rd.u32(0x82F264A8)==4
obj=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\delivery-build\objects\CgsVectorFont-55126aa3.obj');d=obj.read_bytes();_,ns,_,so,nsy,osz,_=struct.unpack_from('<HHIIIHH',d);ss={i+1:struct.unpack_from('<8sIIIIIIHHI',d,20+osz+i*40) for i in range(ns)};st=d[so+nsy*18:]
def name(raw):
 if raw[:4]==b'\0'*4:
  off=struct.unpack_from('<I',raw,4)[0];return st[off:st.index(b'\0',off)].decode()
 return raw.rstrip(b'\0').decode()
syms={};i=0
while i<nsy:
 raw,v,s,k,sc,aux=struct.unpack_from('<8sIhHBB',d,so+i*18);syms[i]=(name(raw),v,s);i+=1+aux
f=[v for v in syms.values() if v[0].startswith('?Print@VectorFont@CgsDev@@') and v[2]>0];assert len(f)==1,f
fn=f[0]
def rel(s):
 t=ss[s];return [struct.unpack_from('<IIH',d,t[5]+j*10) for j in range(t[7])]
used=set();todo=[fn[2]]
while todo:
 s=todo.pop()
 if s in used:continue
 used.add(s)
 for at,ix,k in rel(s):
  if k==0x12:continue
  if syms[ix][2]>0:todo.append(syms[ix][2])
bases={s:0x83800000+j*0x10000 for j,s in enumerate(sorted(used))};images={};unres={}
for s in used:
 t=ss[s];body=bytearray(d[t[4]:t[4]+t[3]]) if t[4] else bytearray(t[3])
 for at,ix,k in rel(s):
  if k==0x12:continue
  n,v,tgt=syms[ix];crt=re.fullmatch(r'__(save|rest)gprlr_(\d+)',n)
  if tgt>0:a=bases[tgt]+v
  elif crt:a=(0x82c08eb0 if crt[1]=='save' else 0x82c08f00)+4*(int(crt[2])-14)
  else:a=0x84800000+len(unres)*4;unres[a]=n
  w=struct.unpack_from('>I',body,at)[0]
  if k in (0x10,0x11):w=(w&0xffff0000)|(((a+0x8000)>>16 if k==0x10 else a)&0xffff)
  elif k==6:w=(w&0xfc000003)|((a-(bases[s]+at))&0x03fffffc)
  elif k==2:w=(w+a)&0xffffffff
  else:raise AssertionError((k,n))
  struct.pack_into('>I',body,at,w)
 images[bases[s]]=body
cases=['','A','A B','A\tB','A\nBC','A\n\tB']
report={'symbol':fn[0],'object_sha256':hashlib.sha256(d).hexdigest(),'unresolved':sorted(set(unres.values())),'cases':[],'mismatches':[],'limits':'Captures vector-font line callback values and pen return only via interpreter; no GPU/rendering/game claim'}
for idx,text in enumerate(cases):
 outs=[]
 for compiled in (False,True):
  c=M.new_cpu()
  for a,b in images.items():c.mem.add(a,len(b),b)
  this=M.S.SELF;strp=M.S.SELF+0x1000;c.mem.wr(strp,text.encode()+b'\0');c.mem.wr(this,struct.pack('>4f',1.75,1.75,0,0));c.mem.wr(this+16,struct.pack('>I',0xffffffff));c.mem.wr(this+20,struct.pack('>I',0));c.mem.wr(this+24,struct.pack('>I',M.S.AUX))
  captures=[]
  def sink(cpu):captures.append({'from':[hex(M.bits(x)) for x in cpu.vf(1)],'to':[hex(M.bits(x)) for x in cpu.vf(2)],'colour':hex(cpu.g(4)&0xffffffff)})
  if compiled:
   for a,n in unres.items():
    if n.startswith('?Draw2DLineForVectorFont@'): c.hooks[a]=sink
    elif n=='??0Vector2@vpu@math@rw@@QAA@MM@Z': c.hooks[a]=lambda cpu: cpu.mem.wr(cpu.g(3),struct.pack('>ffII',cpu.f(1),cpu.f(2),0,0))
    elif n.startswith('__save') or n.startswith('__rest'): c.hooks[a]=lambda cpu: None
    else: c.hooks[a]=lambda cpu,n=n: (_ for _ in ()).throw(AssertionError(('unexpected call',n)))
   entry=bases[fn[2]]+fn[1]
  else:c.hooks[0x8281D298]=sink;entry=0x8281FBF8
  c.setg(3,this);c.setg(6,strp);c.setg(7,0xabcdef12);c.setf(1,10.0);c.setf(2,20.0);c.run(entry);outs.append({'lines':captures,'return':hex(M.bits(c.f(1)))})
 row={'text':text.encode().hex(),'original':outs[0],'compiled':outs[1],'equal':outs[0]==outs[1]};report['cases'].append(row)
 if not row['equal']:report['mismatches'].append(row)

out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\print-comparison-delivery.json');out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert not report['mismatches']





