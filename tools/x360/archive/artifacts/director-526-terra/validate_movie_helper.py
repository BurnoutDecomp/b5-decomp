from pathlib import Path
import json,hashlib,random,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'helper-index/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnICEMoviePlayer.cpp'));sym='?Construct@ICEMoviePlayer@BrnDirector@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
rng=random.Random(0x8225f8d0);S=F.M.S.SELF
for i in range(64):
 raw=rng.randbytes(8);outs=[]
 for compiled in [False,True]:
  c=F.cpu(b);c.mem.wr(S+0x674,raw);c.setg(3,S)
  if compiled:
   for a,n in b.unresolved.items():
    if '?Construct@Camera@' in n:c.hooks[a]=lambda cc:None
   c.run(b.entry)
  else:
   c.hooks[target(0x8225f8f0)]=lambda cc:None;c.run(0x8225f8d0)
  outs.append(c.mem.rd(S+0x674,8))
 assert outs==[raw,raw],(i,raw.hex(),[x.hex() for x in outs])
r={'cases':64,'fields':['0x674','0x678'],'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Compare the two independently poisoned helper-index words after actual original and compiled ICEMoviePlayer::Construct; only Camera::Construct is mocked. Other fields not compared.'};(O/'movie-helper-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
