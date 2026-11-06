import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');r=json.loads((out/'gain-committed/compile-results.json').read_text());p={Path(x['source']).name:Path(x['object']) for x in r['results']};objs=[F.Coff(p['gain_probe.cpp'],'?NativeGainDispatch'),F.Coff(p['Gain.cpp'],'?CreateInstance@Gain@'),F.Coff(p['PlugIn.cpp'],'?Event@PlugIn@')];b=F.Bundle(objs);b.select('?NativeGainDispatch');selfaddr=F.M.S.SELF;aux=F.M.S.AUX;errors=[];rng=random.Random(0x3f534)
for i in range(128):
 img=bytearray(rng.randbytes(0x100));system=bytearray(rng.randbytes(0x1200));ring=bytearray(rng.randbytes(0x200));struct.pack_into('>I',img,4,aux+0x1000);struct.pack_into('>I',system,0x20,aux+0x4000);cursor=(i%5)*4;struct.pack_into('>I',system,0x10b8,cursor);payload=aux+0x7000;name=rng.randbytes(16);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,system);c.mem.wr(aux+0x4000,ring);c.mem.wr(aux+0x7100,name);c.mem.wr(payload,name)
  if compiled:c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,payload);c.run(b.entry)
  else:c.setg(3,selfaddr);c.run(0x82ba2bd0);c.setg(3,selfaddr);c.setg(4,i&1);c.setg(5,payload);c.run(0x82b6a8f8)
  obj=bytearray(c.mem.rd(selfaddr,0x100));cmd=bytearray(c.mem.rd(aux+0x4000,0x200));obj[:4]=bytes(4);cmd[cursor:cursor+4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x1200),cmd))
 if both[0]!=both[1]:errors.append(i)
r={'cases':128,'mismatches':errors,'original':['82BA2BD0','82B6A8F8','8284CB38'],'objects':{k:{'path':str(v),'sha256':hashlib.sha256(v.read_bytes()).hexdigest()} for k,v in p.items()},'limits':'Actual Gain constructor and PlugIn::Event virtual dispatch chain compared to original PPC; vtable and handler pointer values normalized, object/system/command bytes compared. Deleting destructor slot is not called, and no actual XAudio/hardware/full link claim.'};(out/'gain-virtual-dispatch-comparison-committed.json').write_text(json.dumps(r,indent=2)+'\n');print('128 cases',len(errors),'mismatches');print(errors[:5]);assert not errors
