import sys,json,random,struct,hashlib,math
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'butterworth-build';q=d/'Butterworth.obj';errors=[];rng=random.Random(0x76e0);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000
for name,orig in [('HighPassButterworth',0x82b976e0),('LowPassButterworth',0x82b97c00)]:
 p=d/(name+'.obj');prefix='?Process@'+name+'@';b=F.Bundle([F.Coff(p,prefix),F.Coff(q,'?CalculateFilterCoefficients@Butterworth@')]);b.select(prefix)
 for i in range(24):
  initial=bytearray(0x400);initial[0x21]=1+i%2;struct.pack_into('>f',initial,0x28,[1000.,0.,30000.][i%3]);struct.pack_into('>f',initial,0x30,float(1+i%4));struct.pack_into('>f',initial,0x38,[0.5,1.,2.][(i//4)%3]);struct.pack_into('>H',initial,0x58,0x60);struct.pack_into('>I',initial,0x88,1+i%2);struct.pack_into('>H',initial,0x8c,0x30);struct.pack_into('>H',initial,0x8e,0x58);buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);c.mem.wr(aux+0x8000,b''.join(struct.pack('>f',((j%31)-15)/8.0) for j in range(512)));c.mem.wr(aux+0xc000,bytes.fromhex('3f000000')*512);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30018,aux+0x5000);c.setg(3,selfaddr);c.setg(4,mix);c.setg(5,0);trace=[]
   provider=F.address(b,'?CalculateFilterCoefficients@Butterworth@') if compiled else 0x82b64698
   def observe(pc,word):
    if pc==provider:trace.append((c.g(3)&0xffffffff,c.g(5)&0xffffffff,c.g(7)&0xffffffff,struct.pack('>3f',c.f(1),c.f(2),c.f(3)).hex()))
   c.instruction_observer=observe
   def memset(x):
    dst,val,n=[x.g(z)&0xffffffff for z in (3,4,5)];x.mem.wr(dst,bytes([val&255])*n);x.setg(3,dst)
   def tan(x):x.setf(1,math.tan(x.f(1)))
   def powf(x):x.setf(1,math.pow(x.f(1),x.f(2)))
   c.hooks[0x82926fd0]=memset;c.hooks[0x82c09e70]=tan;c.hooks[0x82c09970]=powf
   for at,n in b.unresolved.items():
    if n=='memset':c.hooks[at]=memset
    if n=='tan':c.hooks[at]=tan
    if n=='pow':c.hooks[at]=powf
    if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
   c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x400),c.mem.rd(mix+0x30000,0x40),c.mem.rd(aux+0xc000,2048),trace))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:30],'dstdiff':sum(a!=z for a,z in zip(both[0][2],both[1][2])),'trace':[both[0][3],both[1][3]]})
r={'cases':48,'errors':errors,'provider_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual High/Low Processâ†’designerâ†’Filter, 1-2 channels, orders 1–4 and finite active/bypass inputs, tan/pow/memset/FPR helpers controlled; no hardware claim.'};(out/'butterworth-composed-process-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(len(errors),errors[:2]);assert not errors
