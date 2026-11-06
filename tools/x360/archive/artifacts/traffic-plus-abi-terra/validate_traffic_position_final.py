from pathlib import Path
import sys,random,struct,hashlib,json
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/traffic-plus-abi-terra');j=json.loads((O/'final/compile-results.json').read_text());obj=Path(next(x['object'] for x in j['results'] if x['source'].endswith('traffic_probe.cpp')));sym='?DirectorTrafficPositionProbe@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);A=F.M.S.AUX;COL=A+0x1000;TABLE=A+0x2000;origpc=0x8221a5c4;w=F.M.V.x360rd.u32(origpc);rng=random.Random(origpc)
for i in range(128):
 idx=i%7;data=rng.randbytes(16*7);c=F.cpu(b);c.mem.wr(COL,b'\x00\x07'+bytes(0x12a));c.mem.wr(COL+8,struct.pack('>I',TABLE));c.mem.wr(TABLE,data);c.setg(3,COL);c.setg(4,idx)
 for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:(_ for _ in ()).throw(AssertionError(name))
 c.run(b.entry);result=tuple(c.vr[1]);z=F.cpu(b);z.mem.wr(TABLE,data);z.setg(10,idx*16);z.setg(11,TABLE);assert z.exec_vector(origpc,w,"w%08X" % w)
 assert result==tuple(z.vr[0]),(i,result,z.vr[0])
r={'cases':128,'scope':'Original inline lvx128 at 8221A5C4 versus actual-header compiled GetInstancePos probe. Compare all four raw v1 output lanes for aligned 16-byte records with randomized bits and valid indices; assertions not exercised.','object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()};(O/'traffic-position-comparison-final.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))


