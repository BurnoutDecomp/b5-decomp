import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');r=json.loads((out/'committed-checkpoint/compile-results.json').read_text());p=Path(next(x['object'] for x in r['results'] if x['source'].endswith('/Mixer.cpp')));b=F.Bundle([F.Coff(p,'?Mixer_ctor@Mixer@')]);b.select('?Mixer_ctor@Mixer@');assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
m=0x50000000;sysaddr=F.M.S.AUX;stack=sysaddr+0x1500;errors=[];rng=random.Random(0x6d880)
for i in range(48):
 initial=rng.randbytes(0x30100);st=rng.randbytes(0x50);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(m,0x31000);c.mem.add(0x83270000,0x8000);c.mem.wr(m,initial);c.mem.wr(stack,st);F.put(c,sysaddr,stack);c.setg(3,m)
  ext=next(a for a,n in b.unresolved.items() if n=='off_83271928') if compiled else 0x83271928;F.put(c,ext,sysaddr)
  scratch=F.address(b,'sMixerScratchArena') if compiled else 0x83271d00
  c.run(b.entry if compiled else 0x82b6d880)
  image=c.mem.rd(m,0x30100);vals=struct.unpack('>IIII',c.mem.rd(stack,16));norm=(vals[0],vals[1]-scratch,vals[2]-scratch,vals[3]-scratch);both.append((image,norm,c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'original_stack':both[0][1],'compiled_stack':both[1][1]})
r={'cases':48,'mismatches':errors,'original':'82B6D880','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Entire original and actual compiled Mixer constructor executed, full 0x30100 bytes including guards compared; native scratch arena addresses normalized to own base. Real memory allocator, playback, hardware and full game not tested.'};(out/'mixer-constructor-comparison-committed.json').write_text(json.dumps(r,indent=2)+'\n');print('48 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
