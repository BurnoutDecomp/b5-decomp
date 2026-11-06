import json
from pathlib import Path
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/current-failure-triage-luna/triage_inventory.json')
d=json.loads(p.read_text(encoding='utf-8'))
d['unevaluable_layout_trait_guards'][0]['log']=r'D:\Reverse\BP-X360-Experimental-Artifacts\wave6\full-stage1\BrnReplayGPUDiskWriteStream-e4575ae1.log'
p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
md=p.parent/'TRIAGE.md'
s=md.read_text(encoding='utf-8')
s=s.replace('BrnReplayGPUDiskWriteStream-e45751e1a.log','BrnReplayGPUDiskWriteStream-e4575ae1.log')
md.write_text(s,encoding='utf-8')
print(d['unevaluable_layout_trait_guards'][0]['log'])
print('notes',p.exists(),md.exists())
