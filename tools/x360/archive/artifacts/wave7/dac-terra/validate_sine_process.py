import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');p=out/'sine-native-build/SinePlayer.obj';b=F.Bundle([F.Coff(p,'?Process@SinePlayer@')]);b.select('?Process@SinePlayer@');errors=[];rng=random.Random(0xb948);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for i in range(32):
 img=bytearray(0x100);img[0x21]=1+i%2;struct.pack_into('>d',img,0x28,0.0 if i&2 else 1.0);struct.pack_into('>f',img,0x30,100.0);struct.pack_into('>f',img,0x38,0.5);img[0x3c]=i&1;struct.pack_into('>i',img,0x40,8);buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,img);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x8000,bytes.fromhex('3f000000')*512);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);F.put(c,mix+0x3000c,aux+0x2000);F.put(c,mix+0x30010,aux+0x1000);F.put(c,mix+0x30018,aux+0x5000);F.put(c,mix+0x30028,struct.unpack('>I',struct.pack('>f',1.))[0]);c.setg(3,selfaddr);c.setg(4,mix)
  def sin(x):x.setf(1,math.sin(x.f(1)))
  c.hooks[0x82c095d0]=sin
  for at,n in b.unresolved.items():
   if n=='sin':c.hooks[at]=sin
   if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
  c.run(b.entry if compiled else 0x82b9b948);both.append((c.mem.rd(selfaddr,0x100),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0x8000,2048),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20],'samplediff':sum(a!=z for a,z in zip(both[0][2],both[1][2])), 'returns':[both[0][3],both[1][3]]})
r={'cases':32,'errors':errors,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'limits':'Actual Process sample/state, finite idle/playing and before/after start, 1–2 channels; sin and FPR save/restore controlled, no hardware claim.'};(out/'sine-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
