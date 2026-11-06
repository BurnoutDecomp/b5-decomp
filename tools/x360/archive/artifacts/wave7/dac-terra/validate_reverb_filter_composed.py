import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'ifilter-native-build/ReverbFilters.obj';errors=[];rng=random.Random(0x4640);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;ptrs=[aux+0x1000+j*0x1000 for j in range(6)];ctx=aux+0x8000;rows=[]
for name,orig in [('AllPassFilter',0x82b64640),('CombFilter',0x82b64d00)]:
 prefix='?'+name+'ApplyFunc@'+name+'@';b=F.Bundle([F.Coff(p,prefix)]);b.select(prefix)
 for i in range(128):
  count=[0,1,2,7,16,32][i%6];images=[b''.join(struct.pack('>f',rng.randrange(-16,17)/16.) for _ in range(64)) for j in range(6)];cross=(i//6)&1;img=bytearray(rng.randbytes(0x80))
  for j in range(5):struct.pack_into('>f',img,0x10+j*4,((i+j*3)%17-8)/16.)
  both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,img)
   for addr,data in zip(ptrs,images):c.mem.wr(addr,data)
   for j,addr in enumerate(ptrs):F.put(c,ctx+4*j,addr if j!=2 or cross else 0)
   c.setg(3,selfaddr);c.setg(4,count);c.setg(5,i%2);c.setg(6,i%6);c.setg(7,ctx)
   def memset(x):
    dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
   c.hooks[0x82926fd0]=memset
   for at,n in b.unresolved.items():
    if n=='memset':c.hooks[at]=memset
    if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
   c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x80),c.mem.rd(ctx,24),tuple(c.mem.rd(addr,256) for addr in ptrs)))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'cross':cross,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:12], 'arraydiff':[sum(a!=z for a,z in zip(x,y)) for x,y in zip(both[0][2],both[1][2])]})
 rows.append({'name':name,'original':hex(orig)})
r={'cases':256,'errors':errors,'rows':rows,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual AllPass/Comb IFilter callbacks and real recurrence kernels, both output modes and crossfade-null branch; finite randomized values/counts 0..32, memset/FPR helpers controlled. No alias/special-float/hardware claim.'};(out/'reverb-filter-composed-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
