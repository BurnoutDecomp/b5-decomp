import json,sys,re,hashlib
from pathlib import Path
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
assert u32(0x82815570)==0x7C8A2378
assert u32(0x82F264A8)==4
exp=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
sel=[]
for p in exp.glob('*.json'):
 try:d=json.loads(p.read_text(encoding='utf-8'))
 except Exception:continue
 name=d.get('name','')
 if name.startswith('ICE::ICEController::'):
  sel.append(d)
  (out/(d['address']+'.txt')).write_text(name+'\n\n'+d.get('pseudocode','')+'\n\n'+d.get('assembly','')+'\n',encoding='utf-8')
(out/'controller-index.json').write_text(json.dumps([{'address':d['address'],'name':d['name'],'xrefs_to':d.get('xrefs_to',[]),'xrefs_from':d.get('xrefs_from',[])} for d in sel],indent=2),encoding='utf-8')
print('calibration verified; controller bodies',len(sel))
for d in sel:print(d['address'],d['name'],len(d.get('assembly','').splitlines()))
