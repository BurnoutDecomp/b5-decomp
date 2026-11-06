import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'abi-final/compile-results.json').read_text());p=Path(next(x['object'] for x in report['results'] if x['source'].endswith('/PlugIn.cpp')));b=F.Bundle([F.Coff(p,'?CreateInstance@PlugIn@')]);b.select('?CreateInstance@PlugIn@');selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0xa818)
for i in range(128):
 img=bytearray(rng.randbytes(0x100));config=bytearray(rng.randbytes(0x40));desc=bytearray(rng.randbytes(0x100));struct.pack_into('>I',img,0,aux+0x2000);struct.pack_into('>I',desc,8,0x84801000);struct.pack_into('>I',config,0,aux+0x1500);table=struct.pack('>IIII',0x84801010,0,0,0x84801020);factory=[0,1,0x100,0x101][i%4];both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x83270000,0x2000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x100,config);c.mem.wr(aux+0x300,desc);c.mem.wr(aux+0x2000,table);c.setg(3,selfaddr);c.setg(4,aux+0x800);c.setg(5,aux+0x300);c.setg(6,aux+0x100);c.setg(7,i&255);F.put(c,0x83271928,aux+0x500)
  for at,n in b.unresolved.items():
   if n=='off_83271928':F.put(c,at,aux+0x500)
  trace=[]
  def fac(x):trace.append(('create',x.g(3)&0xffffffff,x.g(4)&0xffffffff));x.setg(3,factory)
  def release(x):trace.append(('release',x.g(3)&0xffffffff))
  def destr(x):trace.append(('destructor',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
  c.hooks[0x84801000]=fac;c.hooks[0x84801010]=release;c.hooks[0x84801020]=destr
  c.run(b.entry if compiled else 0x82b6a818);both.append((c.mem.rd(selfaddr,0x100),trace,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'orig_trace':both[0][1],'compiled_trace':both[1][1],'ret':[both[0][2],both[1][2]],'diff':[(hex(j),a,z) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20]})
r={'cases':128,'mismatches':errors,'original':'82B6A818','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Full actual compiled PlugIn::CreateInstance vs original PPC, including low-byte factory result, controlled virtual slot0/slot3 callbacks, arguments, return and full object/guard bytes. Real derived destructor thunk providers, plugin implementations, full link, concurrency and hardware not covered.'};(out/'plugin-create-dispatch-final-comparison-abi-final.json').write_text(json.dumps(r,indent=2)+'\n');print('cases128 mismatches',len(errors));print(errors[:1]);assert not errors
