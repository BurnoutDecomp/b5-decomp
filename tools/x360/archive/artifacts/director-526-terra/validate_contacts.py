from pathlib import Path
import json,sys,random,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'world-status/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnDirectorModuleIOInputBuffer.cpp'));sym='?AppendContacts@InputBuffer@DirectorIO@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);S=F.M.S.SELF;A=F.M.S.AUX;rng=random.Random(0x823b3198)
for i in range(64):
 before=rng.randbytes(24);value=rng.randbytes(4);outputs=[]
 for orig in (True,False):
  c=F.cpu(b);c.mem.wr(S,b'\x08');c.mem.wr(S+0x6ab0,before);c.mem.wr(A,value);c.setg(3,S);c.setg(4,A)
  if not orig:
   for a,n in b.unresolved.items():c.hooks[a]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
  c.run(0x823b3198 if orig else b.entry);outputs.append(c.mem.rd(S+0x6ab0,24))
 assert outputs[0]==outputs[1],i
r={'cases':64,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Native compiled InputBuffer::AppendContacts vs ARTIST823B3198, valid write lock and poisoned 24-byte surrounding area, comparing whole 24 bytes. Invalid lock/assert path excluded.'};(O/'contacts-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
