import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'route-committed/compile-results.json').read_text());p=Path(report['results'][0]['object']);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0x3d40);assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
specs=[('create',0x82ba3d40,'?CreateInstance@Route@'),('event',0x82ba3dc8,'?EventEvent@Route@'),('release',0x82ba3d88,'?ReleaseEvent@Route@')]
for kind,original,prefix in specs:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));payload=rng.randbytes(16);struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*4;struct.pack_into('>I',system,0x10b8,cursor);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7000,payload);c.setg(3,selfaddr);c.setg(4,i&255);c.setg(5,aux+0x7000);trace=[]
   def disconnect(x):trace.append((x.g(3)&0xffffffff,x.g(4)&0xffffffff));x.setg(3,aux+0x9000)
   c.hooks[0x82b9c3c0]=disconnect
   for at,name in b.unresolved.items():
    if name.startswith('?Disconnect@SubMixConnector@'):c.hooks[at]=disconnect
   c.run(b.entry if compiled else original);obj=bytearray(c.mem.rd(selfaddr,0x100));command=bytearray(c.mem.rd(aux+0x4000,0x200))
   if kind=='create':obj[:4]=bytes(4)
   if kind=='event':
    got=int.from_bytes(command[cursor:cursor+4],'big');expected=F.address(b,'?ConnectByPointerHandler@Route@') if compiled else 0x82b9fb38;assert got==expected,(i,hex(got),hex(expected));command[cursor:cursor+4]=bytes(4)
   both.append((obj,c.mem.rd(aux+0x1000,0x1200),command,trace))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'object_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:16],'ring_diff':[(hex(j),a,z) for j,(a,z) in enumerate(zip(both[0][2],both[1][2])) if a!=z][:16],'trace':[both[0][3],both[1][3]]})
r={'cases':384,'mismatches':errors,'original':['82BA3D40','82BA3DC8','82BA3D88'],'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual native Route CreateInstance/EventEvent/ReleaseEvent vs original PPC, actual handler identities checked and then normalized, vtable identity normalized. Disconnect controlled identically; full object/system/ring/guard bytes and calls compared. No real plugin context, thread, hardware or game link claim.'};(out/'route-build2-comparison-committed.json').write_text(json.dumps(r,indent=2)+'\n');print('384 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
