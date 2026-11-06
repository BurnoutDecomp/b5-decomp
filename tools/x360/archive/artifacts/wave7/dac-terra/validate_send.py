import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');report=json.loads((out/'send-final/compile-results.json').read_text());p=Path(report['results'][0]['object']);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0x3f00);assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378
specs=[('create',0x82ba3e98,'?CreateInstance@Send@'),('event',0x82ba3f00,'?EventEvent@Send@'),('release',0x82ba3ef8,'?ReleaseEvent@Send@')]
for kind,original,prefix in specs:
 b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*4;struct.pack_into('>I',system,0x10b8,cursor);payload=aux+0x7000;name=bytes([65+(j%26) for j in range(i%40)])+b'\0';both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7100,name);F.put(c,payload,aux+0x7100 if i&1 else aux+0x7900);c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,payload);trace=[]
   def disconnect(x):trace.append(('disconnect',x.g(3)&0xffffffff))
   if kind=='release':c.hooks[0x82b9fe38]=disconnect;c.hooks[F.address(b,'?DisconnectImmediate@Send@')]=disconnect
   c.run(b.entry if compiled else original);obj=bytearray(c.mem.rd(selfaddr,0x100));command=bytearray(c.mem.rd(aux+0x4000,0x200))
   if kind=='create':obj[:4]=b'\0'*4
   if kind=='event':
    got=int.from_bytes(command[cursor:cursor+4],'big')
    if compiled:
     expected=(next(a for a,n in b.unresolved.items() if '?ConnectByNameHandler@Send@' in n) if i&1 else F.address(b,'?ConnectByPointerHandler@Send@'))
    else:expected=0x82b9ff80 if i&1 else 0x82b9fef8
    assert got==expected,(kind,i,hex(got),hex(expected))
    command[cursor:cursor+4]=b'\0'*4
   both.append((obj,c.mem.rd(aux+0x1000,0x1200),command,trace))
  if both[0]!=both[1]:errors.append({'kind':kind,'case':i,'object_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:16],'system_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:16],'ring_diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][2],both[1][2])) if a!=z][:16],'trace':[both[0][3],both[1][3]]})
r={'cases':384,'mismatches':errors,'original':['82BA3E98','82BA3F00','82BA3EF8'],'object':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual compiled Send constructor, event and release vs ARTIST; pointer/name command contents, length/cursor, System, Send and guard bytes compared. Vtable pointer and handler identities normalized; DisconnectImmediate controlled for release. Not deleting-dtor provider, execution of queued handlers, actual audio, concurrent or full link proof.'};(out/'send-create-event-release-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('384 cases',len(errors),'mismatches');print(errors[:2]);assert not errors
