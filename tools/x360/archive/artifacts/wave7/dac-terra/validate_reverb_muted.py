import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'reverb-native-build';objs=[d/'ReverbModel1.obj',d/'ReverbFilters.obj',d/'TimerHandle.obj',d/'DelayLine.obj'];prefix='?Process@ReverbModel1@';b=F.Bundle([F.Coff(objs[0],prefix),F.Coff(objs[1],'?CombFilterApplyFunc@CombFilter@'),F.Coff(objs[2],'??0TimerHandle@'),F.Coff(objs[3],'?SetFilter@DelayLine@')]);b.select(prefix);errors=[];rng=random.Random(0xf7d0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
names=[('?AllPassFilterApplyFunc@AllPassFilter@',0x82b64640),('?AllPassFilterResetFunc@AllPassFilter@',0x82ad5078),('?CombFilterApplyFunc@CombFilter@',0x82b64d00),('?CombFilterResetFunc@CombFilter@',0x82b64d88)];comp=[F.address(b,n) for n,a in names]
for i in range(32):
 img=bytearray(0x600);mode=[1,2,4,6][i%4];img[0x21]=mode;img[0x439]=[1,2,2,3][i%4];struct.pack_into('>I',img,4,aux+0x3000);buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,bytes.fromhex('3f000000')*1536);F.put(c,aux+0x3000,aux+0x4000);F.put(c,aux+0x400c,aux+0xf000);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);c.setg(3,selfaddr);c.setg(4,mix)
  def memset(x):
   dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
  c.hooks[0x82926fd0]=memset
  for at,n in b.unresolved.items():
   if n=='memset':c.hooks[at]=memset
  c.run(b.entry if compiled else 0x82b9f7d0);obj=bytearray(c.mem.rd(selfaddr,0x600));
  for base,count,stride,indices in [(0x40,img[0x439],0x18,(0,1)),(0x1dc,6,0x24,(2,3))]:
   for j in range(count):
    for k,idx in enumerate(indices):
     pos=base+j*stride+k*4;got=struct.unpack_from('>I',obj,pos)[0];expect=comp[idx] if compiled else names[idx][1];assert got==expect,(i,hex(pos),hex(got),hex(expect));obj[pos:pos+4]=bytes(4)
  both.append((obj,c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0x8000,6144),c.mem.rd(aux+0x4000,0x20),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:25],'samplediff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':32,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual zero-decay Process, real DelayLine setup functions, 1/2/4/6 channels; XMemSet controlled, dispatch function identities verified and normalized; no active reverb DSP/hardware claim.'};(out/'reverb-process-muted-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
