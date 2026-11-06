import sys,struct,re,json,hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
assert M.V.x360rd.u32(0x82815570)==0x7c8a2378 and M.V.x360rd.u32(0x82F264A8)==4
obj=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\delivery-build\objects\CgsVectorFont-55126aa3.obj')
data=obj.read_bytes()
_,nsec,_,symoff,nsyms,optsize,_=struct.unpack_from('<HHIIIHH',data)
sections={i+1:struct.unpack_from('<8sIIIIIIHHI',data,20+optsize+i*40) for i in range(nsec)}
strings=data[symoff+nsyms*18:]
def nm(raw):
 if raw[:4]==b'\0'*4:
  at=struct.unpack_from('<I',raw,4)[0]; return strings[at:strings.index(b'\0',at)].decode()
 return raw.rstrip(b'\0').decode()
symbols={}; i=0
while i<nsyms:
 raw,value,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',data,symoff+i*18)
 symbols[i]=(nm(raw),value,sec); i+=1+aux
matches=[v for v in symbols.values() if v[0].startswith('?ComputeTextExtent@VectorFont@CgsDev@@') and v[2]>0]
assert len(matches)==1,matches
fn=matches[0]; used=set(); pending=[fn[2]]
def relocs(sec):
 s=sections[sec]; return [struct.unpack_from('<IIH',data,s[5]+j*10) for j in range(s[7])]
while pending:
 sec=pending.pop()
 if sec in used: continue
 used.add(sec)
 for at,index,kind in relocs(sec):
  if kind==0x12: continue
  sym,val,target=symbols[index]
  if target>0: pending.append(target)
bases={sec:0x83800000+j*0x10000 for j,sec in enumerate(sorted(used))}
images={}; unresolved={}
for sec in used:
 s=sections[sec]; body=bytearray(data[s[4]:s[4]+s[3]]) if s[4] else bytearray(s[3])
 for at,index,kind in relocs(sec):
  if kind==0x12: continue
  sym,val,target=symbols[index]
  crt=re.fullmatch(r'__(save|rest)gprlr_(\d+)',sym)
  if target>0: addr=bases[target]+val
  elif crt: addr=(0x82c08eb0 if crt[1]=='save' else 0x82c08f00)+4*(int(crt[2])-14)
  else:
   addr=0x84800000+len(unresolved)*4; unresolved[addr]=sym
  word=struct.unpack_from('>I',body,at)[0]
  if kind in (0x10,0x11):
   imm=(addr+0x8000)>>16 if kind==0x10 else addr; word=(word&0xffff0000)|(imm&0xffff)
  elif kind==6:
   delta=addr-(bases[sec]+at); assert -(1<<25)<=delta<(1<<25); word=(word&0xfc000003)|(delta&0x03fffffc)
  elif kind==2: word=(word+addr)&0xffffffff
  else: raise AssertionError((kind,sym))
  struct.pack_into('>I',body,at,word)
 images[bases[sec]]=body

def cpu():
 c=M.new_cpu()
 for addr,b in images.items(): c.mem.add(addr,len(b),b)
 def reject(sym):
  def cb(cpu): raise AssertionError(('unresolved call',sym))
  return cb
 def vector2_ctor(cpu):
  cpu.mem.wr(cpu.g(3),struct.pack('>ffII',cpu.f(1),cpu.f(2),0,0))
 for addr,sym in unresolved.items():
  c.hooks[addr]=vector2_ctor if sym=='??0Vector2@vpu@math@rw@@QAA@MM@Z' else reject(sym)
 return c
cases=[('',14.0,14.0),('A',14.0,14.0),('A\nBC',14.0,14.0),('A\tB',14.0,14.0),(' \x01\x88',14.0,14.0),('A',M.value(0x00000001),14.0),('A',14.0,M.value(0x80000001)),('HELLO WORLD',3.25,5.5)]
report={'original':'ARTIST 0x82818E10','compiled_symbol':fn[0],'object':str(obj),'object_sha256':hashlib.sha256(data).hexdigest(),'sections_loaded':len(images),'unresolved_symbols':sorted(set(unresolved.values())),'cases':[],'mismatches':[],'external_call_model':'The compiled routine has one unresolved Vector2(float,float) constructor. The comparison hook writes its attested native result: the two FPR float arguments followed by zero Z/W words; this operation is separately verified by the prior compiled-provider vs ARTIST lane report.', 'limits':'PPC interpreter comparison over these strings/sizes only; constructor hook models its attested result; no renderer/GPU or game claim'}
for i,(s,x,y) in enumerate(cases):
 payload=s.encode('latin1')+b'\0'; p=M.S.SELF
 outs=[]
 for entry in (0x82818E10,bases[fn[2]]+fn[1]):
  c=cpu(); c.mem.wr(p,payload); c.setg(3,p); c.setvf(1,[x,y,0.0,0.0]); c.run(entry); outs.append(c.vf(1))
 a,b=outs; row={'case':i,'text_hex':payload.hex(),'size':[x,y,0.0,0.0],'original':[hex(M.bits(v)) for v in a],'compiled':[hex(M.bits(v)) for v in b],'equal':[M.bits(v)==M.bits(w) for v,w in zip(a,b)]}; report['cases'].append(row)
 if not all(row['equal']):report['mismatches'].append(row)
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\vectorfont_provider_luna_4e1\extent-comparison-delivery.json'); out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2)); assert not report['mismatches']





