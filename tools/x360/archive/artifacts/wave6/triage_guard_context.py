import json
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json').read_text(encoding='utf-8'))
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
for x in d['failed_rows']:
 es=[e for e in x['diagnostics'] if e['code'] in {'C2118','C2338','C2337','C2070','C2036','C2466'}]
 if not es: continue
 print('---',x['source'])
 f=root/x['source']
 if f.is_file():
  lines=f.read_text(encoding='utf-8',errors='replace').splitlines()
 else: lines=[]
 for e in es:
  n=int(e['line']) if e['line'].isdigit() else None
  print(f"{e['location']}:{e['line']} {e['code']} {e['message']}")
  if n and lines:
   for ix in range(max(0,n-2),min(len(lines),n+1)): print(f' {ix+1}: {lines[ix]}')
