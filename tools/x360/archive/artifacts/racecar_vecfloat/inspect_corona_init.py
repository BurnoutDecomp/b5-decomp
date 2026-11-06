import sys,struct,json,hashlib
from pathlib import Path
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
class TraceMem(V.Mem):
 def __init__(self):super().__init__();self.writes=[];self.record=False
 def wr(self,a,data):
  if self.record and 0x82f24310 <= a < 0x82f24310+25*48:self.writes.append({'address':hex(a),'size':len(data),'data':bytes(data).hex()})
  return super().wr(a,data)
m=TraceMem();base=0x82f24000;m.add(base,0x4000,V.x360rd.rd(base,0x4000));m.add(0x44000000,0x20000);c=E.CPU64(m);c.setg(1,0x44010000);before=m.rd(0x82f24310,25*48);m.record=True;c.run(0x82c4f418);after=m.rd(0x82f24310,25*48);changes=[]
for i in range(25):
 b=before[i*48:(i+1)*48];a=after[i*48:(i+1)*48];changes.append({'index':i,'image':b.hex(),'after':a.hex(),'changed_words':[j for j in range(0,48,4) if a[j:j+4]!=b[j:j+4]]})
r={'original':'ARTIST CRT initializer 0x82C4F418','image_sha256':hashlib.sha256(before).hexdigest(),'initialized_sha256':hashlib.sha256(after).hexdigest(),'writes':m.writes,'records':changes,'scope':'original image bytes and actual initializer emulated once; no native source initializer equivalence or full CRT ordering claim'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/corona-original-init.json').write_text(json.dumps(r,indent=2));print('image',r['image_sha256'],'after',r['initialized_sha256']);print('changed words by record',[(x['index'],x['changed_words']) for x in changes])
