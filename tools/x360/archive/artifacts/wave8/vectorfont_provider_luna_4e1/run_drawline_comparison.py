import sys,struct,re,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk');import validate_native_math as M
assert M.V.x360rd.u32(0x82815570)==0x7c8a2378 and M.V.x360rd.u32(0x82F264A8)==4
obj=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\delivery-build\objects\CgsVectorFont-55126aa3.obj');data=obj.read_bytes()
_,nsec,_,symoff,nsyms,optsize,_=struct.unpack_from('<HHIIIHH',data);secs={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}; strings=data[symoff+nsyms*18:]
def nm(raw):
 if raw[:4]==b'\0'*4:
  at=struct.unpack_from('<I',raw,4)[0];return strings[at:strings.index(b'\0',at)].decode()
 return raw.rstrip(b'\0').decode()
sym={};i=0
while i<nsyms:
 raw,val,sec,k,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18);sym[i]=(nm(raw),val,sec);i+=1+aux
cands=[v for v in sym.values() if v[0].startswith('?DrawLine@VectorFont@CgsDev@@') and v[2]>0];assert len(cands)==1,cands
fn=cands[0];used=set();todo=[fn[2]]
def relocs(s):
 t=secs[s];return [struct.unpack_from('<IIH',data,t[5]+j*10) for j in range(t[7])]
while todo:
 s=todo.pop()
 if s in used:continue
 used.add(s)
 for at,ix,k in relocs(s):
  if k==0x12:continue
  _,_,t=sym[ix]
  if t>0:todo.append(t)
bases={s:0x83800000+j*0x10000 for j,s in enumerate(sorted(used))};images={};unresolved={}
for s in used:
 t=secs[s];body=bytearray(data[t[4]:t[4]+t[3]]) if t[4] else bytearray(t[3])
 for at,ix,k in relocs(s):
  if k==0x12:continue
  n,v,tgt=sym[ix];crt=re.fullmatch(r'__(save|rest)gprlr_(\d+)',n)
  if tgt>0:a=bases[tgt]+v
  elif crt:a=(0x82c08eb0 if crt[1]=='save' else 0x82c08f00)+4*(int(crt[2])-14)
  else:a=0x84800000+len(unresolved)*4;unresolved[a]=n
  w=struct.unpack_from('>I',body,at)[0]
  if k in (0x10,0x11):w=(w&0xffff0000)|(((a+0x8000)>>16 if k==0x10 else a)&0xffff)
  elif k==6:w=(w&0xfc000003)|((a-(bases[s]+at))&0x03fffffc)
  elif k==2:w=(w+a)&0xffffffff
  else:raise AssertionError((k,n))
  struct.pack_into('>I',body,at,w)
 images[bases[s]]=body
scenarios=[((1.25,-7.5),(0.0,3.0)),((-0.0,1.0e-30),(9.0,-2.25)),((123.5,0.0),(-0.25,8.0))]
report={'compiled_symbol':fn[0],'object':str(obj),'object_sha256':hashlib.sha256(data).hexdigest(),'original':'ARTIST CgsDev::VectorFont::DrawLine @0x8281F150; capture at Draw2DLineForVectorFont @0x8281D298','cases':[],'mismatches':[],'limits':'3D renderer branch only; records endpoint VMX arguments and receiver/color; interpreter comparison only; no renderer/GPU/game claim'}
for idx,(p1,p2) in enumerate(scenarios):
 outputs=[]
 for compiled in (False,True):
  c=M.new_cpu()
  for a,b in images.items():c.mem.add(a,len(b),b)
  c.mem.wr(M.S.SELF+20,struct.pack('>I',0));c.mem.wr(M.S.SELF+24,struct.pack('>I',M.S.AUX))
  captured=[]
  def sink(cpu):captured.append({'receiver':hex(cpu.g(3)),'colour':hex(cpu.g(4)&0xffffffff),'from':[hex(M.bits(x)) for x in cpu.vf(1)],'to':[hex(M.bits(x)) for x in cpu.vf(2)]})
  if compiled:
   for a,n in unresolved.items():
    if n.startswith('?Draw2DLineForVectorFont@'):c.hooks[a]=sink
    else:c.hooks[a]=lambda cpu,n=n: (_ for _ in ()).throw(AssertionError(('unexpected call',n)))
   entry=bases[fn[2]]+fn[1]
  else:c.hooks[0x8281D298]=sink;entry=0x8281F150
  c.setg(3,M.S.SELF);c.setg(4,0xaabbccdd);c.setvf(1,[p1[0],p1[1],0,0]);c.setvf(2,[p2[0],p2[1],0,0]);c.run(entry)
  outputs.append(captured)
 row={'case':idx,'input':[p1,p2],'original':outputs[0],'compiled':outputs[1],'equal':outputs[0]==outputs[1]}
 report['cases'].append(row)
 if not row['equal']:report['mismatches'].append(row)
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\drawline-comparison-delivery.json');out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));assert not report['mismatches']

