import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'ifilter-native-build/DelayFilter.obj';prefix='?DelayFilterApplyFunc@DelayFilter@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix);errors=[];rng=random.Random(0x7dd0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;ptrs=[aux+0x1000+j*0x1000 for j in range(6)];ctx=aux+0x8000
for i in range(128):
 count=[0,1,2,7,16,32][i%6];images=[b''.join(struct.pack('>f',rng.randrange(-16,17)/16.) for _ in range(64)) for j in range(6)];cross=(i//6)&1;img=bytearray(rng.randbytes(0x80));struct.pack_into('>f',img,0x10,(i%17-8)/16.);struct.pack_into('>f',img,0x14,((i*3)%17-8)/16.);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(selfaddr,img)
  for addr,data in zip(ptrs,images):c.mem.wr(addr,data)
  for j,addr in enumerate(ptrs):F.put(c,ctx+4*j,addr if j!=2 or cross else 0)
  c.setg(3,selfaddr);c.setg(4,count);c.setg(5,i%3);c.setg(6,i%6);c.setg(7,ctx);trace=[];provider=F.address(b,'?DelayFilterCrossFadeFunc@') if compiled else 0x82b67d40
  def observe(pc,word):
   if pc==provider:trace.append((x:=c.g(3)&0xffffffff,*[c.g(z)&0xffffffff for z in (6,7,8,9,10)],int.from_bytes(c.mem.rd(c.g(1)+0x54,4),'big'),struct.pack('>2f',c.f(1),c.f(2)).hex()))
  c.instruction_observer=observe;c.run(b.entry if compiled else 0x82b67dd0);both.append((c.mem.rd(selfaddr,0x80),c.mem.rd(ctx,24),tuple(c.mem.rd(addr,256) for addr in ptrs),trace))
 if both[0]!=both[1]:errors.append({'case':i,'cross':cross,'arraydiff':[sum(a!=z for a,z in zip(x,y)) for x,y in zip(both[0][2],both[1][2])],'trace':[both[0][3],both[1][3]]})
r={'cases':128,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual DelayFilter apply→actual crossfade/direct kernel output and full state, pointer/stack register trace, finite randomized values and counts 0..32, no alias/special-float/hardware claim.'};(out/'delay-filter-composed-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
