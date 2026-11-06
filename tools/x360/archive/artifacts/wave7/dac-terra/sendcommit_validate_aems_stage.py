import sys,json,random,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');r=json.loads((out/'send-committed/compile-results.json').read_text());p=Path(next(x['object'] for x in r['results'] if 'CgsAemsInterfaceImplementation.cpp' in x['source']));b=F.Bundle([F.Coff(p,'?CreateInstance@AemsRWSampleFactory@')]);b.select('?CreateInstance@AemsRWSampleFactory@');rng=random.Random(0x2a70);aux=F.M.S.AUX;errors=[]
class Stop(Exception):pass
for i in range(96):
 channels=1+i%6;handles=[rng.getrandbits(32) for _ in range(7)];both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(0x83270000,0x2000);factory=aux+0x1000;player=aux+0x3000;sysaddr=aux+0x5000;F.put(c,0x83271928,sysaddr)
  for j,off in enumerate((0x3c,0x40,0x44,0x48,0x4c,0x50,0x54)):F.put(c,factory+off,handles[j])
  c.mem.wr(player+0x94,bytes([channels]));stack=c.g(1)&0xffffffff;c.mem.wr(stack+0x100,rng.randbytes(0xa8));trace=[]
  def capture(x):
   count=x.g(4)&0xffffffff;data=x.g(5)&0xffffffff;records=[]
   for j in range(count):
    q=data+j*12;records.append((int.from_bytes(x.mem.rd(q,4),'big'),int.from_bytes(x.mem.rd(q+4,4),'big'),x.mem.rd(q+8,1)[0]))
   trace.append((x.g(3)&0xffffffff,count,records,x.g(7)&0xffffffff));raise Stop()
  c.hooks[0x82b6ec50]=capture
  for a,n in b.unresolved.items():
   if n.startswith('?CreateInstance@Voice@'):c.hooks[a]=capture
  if compiled:c.setg(29,factory);c.setg(28,player);c.setg(31,0);c.setg(22,sysaddr);entry=b.entry+0x1bc
  else:c.setg(25,factory);c.setg(31,player);c.setg(29,0);c.setg(15,0x83270000);entry=0x826c2a70
  try:c.run(entry)
  except Stop:pass
  both.append(trace)
 if both[0]!=both[1]:errors.append({'case':i,'channels':channels,'original':both[0],'compiled':both[1]})
report={'cases':96,'mismatches':errors,'original_window':'826C2A70..826C2BA0','compiled_function_offset':'0x1BC','object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Execute actual compiled and original AEMS configuration-building instruction windows for valid channel counts 1..6 with matching factory/player/stack inputs; compare Voice::CreateInstance mode,count, every record context/handle/output byte and System pointer. Stops at controlled CreateInstance boundary; not a whole AEMS body, invalid inputs, hardware or link proof.'};(out/'aems-stage-window-committed.json').write_text(json.dumps(report,indent=2)+'\n');print('96 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
