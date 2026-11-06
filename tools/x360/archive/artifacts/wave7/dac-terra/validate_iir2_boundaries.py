import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'iir2-build';q=d/'Iir2-foreign-3534.obj';errors=[];rng=random.Random(0xe1f8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;mix=0x50000000;rows=[]
specs=[('LowPassIir2',0x82b9e5c8,0x82b97dc0),('HighPassIir2',0x82b9e0a0,0x82b978c0),('BandPassIir2',0x82b9d778,0x82b96a50),('LowShelfIir2',0x82b9e720,0x82b97e80),('HighShelfIir2',0x82b9e1f8,0x82b97988),('PeakingIir2',0x82b9f168,0x82b9a470)]
for name,orig,coeff in specs:
 p=d/(name+'.obj');prefix='?Process@'+name+'@';b=F.Bundle([F.Coff(p,prefix),F.Coff(q,'?Filter@Iir2@')]);b.select(prefix)
 for i in range(32):
  initial=bytearray(0x200);initial[0x21]=1+i%2
  for at,val in [(0x28,[0.,1000.,24000.,50000.][i%4]),(0x30,[0.5,1.,2.,5.][i%4]),(0x38,1.)]:struct.pack_into('>f',initial,at,val)
  buf=bytearray(0x80);struct.pack_into('>I',buf,4,aux+0x8000);struct.pack_into('>H',buf,14,256);buf2=bytearray(buf);struct.pack_into('>I',buf2,4,aux+0xc000);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.add(mix,0x31000);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,buf);c.mem.wr(aux+0x2000,buf2);F.put(c,aux+0x500c,struct.unpack('>I',struct.pack('>f',48000.))[0]);F.put(c,mix+0x3000c,aux+0x1000);F.put(c,mix+0x30010,aux+0x2000);F.put(c,mix+0x30018,aux+0x5000);c.setg(3,selfaddr);c.setg(4,mix);trace=[]
   def calc(x):trace.append(('coeff',x.g(3)&0xffffffff))
   def filt(x):trace.append(('filter',*[x.g(z)&0xffffffff for z in (3,4,5,6)]))
   c.hooks[coeff]=calc;c.hooks[0x82b69dd0]=filt;c.hooks[F.address(b,'?CalculateFilterCoefficients@'+name+'@')]=calc;c.hooks[F.address(b,'?Filter@Iir2@')]=filt
   for at,n in b.unresolved.items():
    if n.startswith(('__savefpr_', '__restfpr_')):c.hooks[at]=lambda x:None
   c.run(b.entry if compiled else orig);both.append((c.mem.rd(selfaddr,0x200),c.mem.rd(mix+0x30000,0x40),trace,c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:20], 'trace':[both[0][2],both[1][2]]})
 rows.append({'name':name,'original':hex(orig),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
r={'cases':192,'errors':errors,'rows':rows,'limits':'Actual class Process branch/state and calls, coefficient calculation and Iir2::Filter controlled; save/restore fpr controlled; finite input only, no kernel-output claim.'};(out/'iir2-process-boundary-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('192 cases',len(errors),errors[:2]);assert not errors
