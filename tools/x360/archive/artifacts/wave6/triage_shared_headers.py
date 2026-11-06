import json,collections
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json').read_text(encoding='utf-8'))
h=collections.defaultdict(set); first=collections.defaultdict(set)
for x in d['failed_rows']:
 for e in x['diagnostics']:
  loc=e['location']
  if loc.lower().endswith(('.h','.hpp','.inl')):h[loc].add(x['source'])
 if x['diagnostics'] and x['diagnostics'][0]['location'].lower().endswith(('.h','.hpp','.inl')):first[x['diagnostics'][0]['location']].add(x['source'])
print('SHARED HEADER ERROR TUS:')
for loc,src in sorted(h.items(),key=lambda kv:(-len(kv[1]),kv[0]))[:50]:
 print(len(src), 'primary='+str(len(first.get(loc,set()))), loc)
 for s in sorted(src)[:5]:print(' ',s)
