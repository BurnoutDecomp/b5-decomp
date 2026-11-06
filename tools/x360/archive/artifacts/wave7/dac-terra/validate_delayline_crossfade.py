import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'ifilter-native-build';objs=[d/'Delay.obj',d/'DelayLine.obj',d/'DelayFilter.obj'];b=F.Bundle([F.Coff(objs[0],'?Process@Delay@'),F.Coff(objs[1],'?ApplyFilter@DelayLine@'),F.Coff(objs[2],'?DelayFilterApplyFunc@DelayFilter@')]);b.select('?Process@Delay@');errors=[];rng=random.Random(0xe4a0);line=F.M.S.SELF;aux=F.M.S.AUX;filt=aux+0x1000;srcdesc=aux+0x2000;dstdesc=aux+0x3000;inp=aux+0x4000;full=aux+0x5000;zero=aux+0x6000;dst=aux+0x7000
for i in range(64):
 count=[1,2,7,16][i%4];state=bytearray(0x100);struct.pack_into('>I',state,0,aux+0xa000);struct.pack_into('>I',state,0x14,128);struct.pack_into('>I',state,0x30,1);struct.pack_into('>I',state,0x34,32);state[0x38]=1;filterdata=bytearray(rng.randbytes(0x40));struct.pack_into('>f',filterdata,0x10,((i%9)-4)/8.);source=b''.join(struct.pack('>f',rng.randrange(-16,17)/16.) for _ in range(64));work=b''.join(struct.pack('>f',rng.randrange(-16,17)/16.) for _ in range(64));both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(line,state);c.mem.wr(filt,filterdata);F.put(c,srcdesc+4,inp);F.put(c,dstdesc+4,dst);c.mem.wr(inp,source);c.mem.wr(full,work);c.mem.wr(aux+0xb000,source);c.mem.wr(zero,bytes(256));c.mem.wr(dst,bytes(256));c.mem.wr(aux+0xa000,bytes(512));trace=[]
  def marshal(x):
   ctx=x.g(7)&0xffffffff;trace.append(('marshal',x.g(4)&0xffffffff,x.g(5)&0xffffffff,x.g(6)&0xffffffff));F.put(x,ctx+4,full);F.put(x,ctx+8,aux+0xb000);F.put(x,ctx+16,zero);x.setg(3,count)
  def copy(x):
   dest,source,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dest,x.mem.rd(source,n));x.setg(3,dest)
  c.hooks[0x82b6cf10]=marshal;c.hooks[F.address(b,'?MarshalDelayData@DelayLine@')]=marshal;c.hooks[0x82926ba0]=copy
  for at,n in b.unresolved.items():
   if n in ('XMemCpy','memcpy'):c.hooks[at]=copy
  F.put(c,filt,F.address(b,'?DelayFilterApplyFunc@DelayFilter@') if compiled else 0x82b67dd0);F.put(c,line+4,filt);c.setg(3,line);c.setg(4,count);c.setg(5,srcdesc);c.setg(6,dstdesc);c.setg(7,0);c.run(F.address(b,'?ApplyFilter@DelayLine@') if compiled else 0x82b6e4a0)
  fd=bytearray(c.mem.rd(filt,0x40));expected=F.address(b,'?DelayFilterApplyFunc@DelayFilter@') if compiled else 0x82b67dd0;assert int.from_bytes(fd[:4],'big')==expected;fd[:4]=bytes(4);both.append((c.mem.rd(line,0x100),fd,c.mem.rd(zero,256),c.mem.rd(dst,256),c.mem.rd(aux+0xa000,512),trace))
 if both[0]!=both[1]:errors.append({'case':i,'trace':[both[0][5],both[1][5]],'linediff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'zero_diff':sum(a!=z for a,z in zip(both[0][2],both[1][2]))})
r={'cases':64,'errors':errors,'objects':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objs},'limits':'Actual DelayLine::ApplyFilter indirect IFilter callback→actual DelayFilter direct kernel; Marshal and memcpy controlled; real Unmarshal; finite crossfade cases, no real ring/hardware claim.'};(out/'delayline-ifilter-crossfade-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:1]);assert not errors
