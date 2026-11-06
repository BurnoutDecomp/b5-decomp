import sys,json,random,hashlib,struct
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/state-vtable-r3-terra');sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
p=out/'CgsStateManager.obj';o=F.Coff(p,'?GetStateObj@StateManager@');b=F.Bundle([o]);b.select('?GetStateObj@StateManager@');BASE=0x57000000;SELF=BASE+0x100;STATES=BASE+0x2000;VT=BASE+0x5000;HOOK=BASE+0x6000;errors=[];rng=random.Random(0xd8e8)
for i in range(128):
 img=bytearray(rng.randbytes(0x10000));count=i%6;needle=BASE+0x8000;struct.pack_into('>I',img,SELF-BASE+0x18,STATES if count else 0);struct.pack_into('>I',img,VT-BASE+0x20,HOOK)
 for j in range(count):
  off=STATES-BASE+j*0x100;struct.pack_into('>I',img,off,VT);struct.pack_into('>I',img,off+0x18,STATES+(j+1)*0x100 if j+1<count else 0);img[off+0x48]=(i+j)%3!=0
 results=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(BASE,len(img),img);trace=[]
  def attached(cpu):
   addr=cpu.g(3)&0xffffffff;arg=cpu.g(4)&0xffffffff;trace.append((addr,arg));cpu.setg(3,int(((addr-STATES)//0x100+i)%4==0))
  c.hooks[HOOK]=attached;c.setg(3,SELF);c.setg(4,needle);c.run(b.entry if compiled else 0x8268d8e8);results.append((c.g(3)&0xffffffff,c.mem.rd(BASE,len(img)),trace))
 if results[0]!=results[1]:errors.append(i)
r={'cases':128,'mismatches':errors,'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual original/compiled whole StateManager GetStateObj, attached/list traversal and virtual slot +0x20; IsAttachedToThis controlled with matching pointer-sensitive result. Single seeded vtable slot, no hardware or full sound claim.'};(out/'state-get-object-comparison.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print('128 cases mismatches',errors);assert not errors
