import sys,json,random,struct,hashlib
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');d=out/'iir2-build';q=d/'Iir2-foreign-3534.obj';errors=[];rng=random.Random(0x3130);selfaddr=F.M.S.SELF;aux=F.M.S.AUX;rows=[]
for name,orig in [('LowPassIir2',0x82ba3130),('HighPassIir2',0x82ba2e40),('BandPassIir2',0x82ba2398),('LowShelfIir2',0x82ba3198),('HighShelfIir2',0x82ba2ea8),('PeakingIir2',0x82ba3708)]:
 p=d/(name+'.obj');prefix='?CreateInstance@'+name+'@';b=F.Bundle([F.Coff(p,prefix),F.Coff(q,'?ClearBuffer@Iir2@')]);b.select(prefix)
 for i in range(64):
  initial=bytearray(rng.randbytes(0x200));voice=bytearray(rng.randbytes(0x100));struct.pack_into('>I',initial,8,aux+0x1000);struct.pack_into('>f',initial,0x18,(i%17)/2.0);struct.pack_into('>f',voice,0x28,(i%13)/2.0);both=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.mem.wr(aux+0x1000,voice);c.setg(3,selfaddr);c.run(b.entry if compiled else orig);obj=bytearray(c.mem.rd(selfaddr,0x200));obj[:4]=bytes(4);both.append((obj,c.mem.rd(aux+0x1000,0x100),c.g(3)&0xffffffff))
  if both[0]!=both[1]:errors.append({'name':name,'case':i,'objdiff':[hex(j) for j,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z][:24], 'voicediff':[hex(j) for j,(a,z) in enumerate(zip(both[0][1],both[1][1])) if a!=z][:24],'returns':[both[0][2],both[1][2]]})
 rows.append({'name':name,'original':hex(orig),'object_sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
r={'cases':384,'errors':errors,'rows':rows,'foreign_iir2_sha256':hashlib.sha256(q.read_bytes()).hexdigest(),'limits':'Actual constructors and actual compiled Iir2::ClearBuffer against ARTIST, finite decay inputs; derived vtable addresses normalized, native sizes separately compile-gated. Not Process or hardware proof.'};(out/'iir2-create-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print('384 cases',len(errors),errors[:2]);assert not errors
