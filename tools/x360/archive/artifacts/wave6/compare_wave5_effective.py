import json
from pathlib import Path
cur=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/full-stage1/compile-results.json').read_text(encoding='utf-8'))
base=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave5-delivery/native-parallel-wave5-final-effective.json').read_text(encoding='utf-8'))
get=lambda d:{x['source']:bool(x['passed']) for x in d['results']}
a,b=get(base),get(cur)
print('effective',base.get('requested'),base.get('passed'),base.get('failed'),'raw',cur.get('requested'),cur.get('passed'),cur.get('failed'))
print('old effective pass -> fail',sum(a[k] and not b[k] for k in a.keys()&b.keys()))
print('old effective fail -> pass',sum(not a[k] and b[k] for k in a.keys()&b.keys()))
print('new source passed',sum(b[k] for k in b.keys()-a.keys()))
print('new source failed',sum(not b[k] for k in b.keys()-a.keys()))
print('failed both',sum(not a[k] and not b[k] for k in a.keys()&b.keys()))
print('missing old',len(a.keys()-b.keys()))
for s in sorted(a.keys()&b.keys()):
 if a[s] and not b[s]: print('REGRESSION',s)
