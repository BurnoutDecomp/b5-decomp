import json,re
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json').read_text(encoding='utf-8'))
need={'C2118','C2334','C2337','C2338','C2352','C2370','C2371','C2374','C2070','C2027','C2036','C3646'}
for x in d['failed_rows']:
 es=[e for e in x['diagnostics'] if e['code'] in need or re.search(r'\b(sizeof|static_assert|offsetof|alignof|layout|size)\b',e['message'],re.I)]
 if es:
  print('---',x['source'],'LOG',x['log'])
  for e in es[:12]:print(f" {e['location']}:{e['line']} {e['code']} {e['message']}")
