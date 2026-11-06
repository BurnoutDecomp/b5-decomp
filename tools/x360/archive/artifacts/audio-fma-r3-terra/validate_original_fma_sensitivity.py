import sys,struct,json,random
from pathlib import Path
import numpy as np
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra');sys.path.insert(0,str(out));from probe_original import WordCPU,F
b=F.Bundle([F.Coff(Path('D:/Reverse/BP-X360-Experimental-Artifacts/state-vtable-r3-terra/CgsState.obj'),'?IsAttachedToThis@State@')]);b.select('?IsAttachedToThis@State@');rng=random.Random(0xf360);rows={}
for name,addr in [('XasEnc',0x82ba6e98),('Xas1Enc',0x82ba74a8)]:
 samples=0;differences=0;sites={}
 for i in range(16):
  ch=1+i%2;raw=b''.join(struct.pack('>f',rng.uniform(-0.98,0.98)) for j in range(128*ch));c=F.cpu(b);c.__class__=WordCPU;c.mem.wr(F.M.S.AUX+0x1000,raw);c.setg(3,F.M.S.SELF);c.setg(4,F.M.S.AUX+0x1000);c.setg(5,F.M.S.AUX+0x9000);c.setg(6,ch);c.max_steps=3000000;counts=[0,0]
  def observer(pc,w):
   if w>>26!=59 or (w>>1)&31!=29:return
   a=c.f((w>>16)&31);bv=c.f((w>>11)&31);cc=c.f((w>>6)&31);fused=F.M.V.fma(a,cc,bv);separate=np.float32(np.float32(a*cc)+bv);counts[0]+=1
   if struct.pack('>f',fused)!=struct.pack('>f',separate):counts[1]+=1;sites[hex(pc)]=sites.get(hex(pc),0)+1
  c.instruction_observer=observer;c.run(addr);samples+=counts[0];differences+=counts[1]
 rows[name]={'input_cases':16,'original_fmadds_executed':samples,'fused_vs_separate_differences':differences,'sites':sites}
(out/'original-fma-sensitivity.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows,indent=2))

