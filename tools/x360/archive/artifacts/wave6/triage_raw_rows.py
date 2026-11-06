import json,re,collections
from pathlib import Path
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/full-stage1/compile-results.json')
r=json.loads(p.read_text(encoding='utf-8'))
failed=[x for x in r['results'] if not x.get('passed')]
print('REPORT',r.get('state'),r.get('requested'),r.get('passed'),r.get('failed'),'actual_failed_rows',len(failed))
pat=re.compile(r'^(.*?)(?:\((\d+)\))?\s*:\s*(fatal )?error\s+(C\d+|[A-Z]+\d+):\s*(.*)$',re.I)
keys=collections.Counter(); rows=collections.defaultdict(list); missing=[]; all_counts=collections.Counter()
for x in failed:
    lp=Path(x.get('log',''))
    if not lp.is_file(): missing.append((x['source'],str(lp))); continue
    lines=lp.read_text(encoding='utf-8',errors='replace').splitlines()
    ds=[]
    for l in lines:
        if 'error ' not in l.lower() and 'fatal error' not in l.lower(): continue
        m=pat.match(l.strip())
        if m:
            loc=m.group(1).replace('\\','/').split('/')[-1]
            line=m.group(2) or ''
            fatal=m.group(3) or ''
            code=m.group(4).upper(); msg=' '.join(m.group(5).split())
            ds.append((loc,line,fatal,code,msg,l.strip()))
        else: ds.append(('unparsed','','','',l.strip(),l.strip()))
    if not ds:
        missing.append((x['source'],'no diagnostics parsed')); continue
    primary=ds[0]
    key=(primary[0],primary[3],primary[4])
    keys[key]+=1; rows[key].append(x['source'])
    for d in ds: all_counts[(d[0],d[3],d[4])]+=1
print('PRIMARY_GROUPS',len(keys),'missing',len(missing))
for (loc,code,msg),n in keys.most_common():
 print(f'ROOT\t{n}\t{loc}\t{code}\t{msg}')
 print(' SOURCES\t'+' | '.join(rows[(loc,code,msg)][:12]))
 if len(rows[(loc,code,msg)])>12: print(' MORE\t'+str(len(rows[(loc,code,msg)])-12))
print('ALL_DIAGNOSTIC_KEYS',len(all_counts))
for (loc,code,msg),n in all_counts.most_common(100): print(f'DIAG\t{n}\t{loc}\t{code}\t{msg}')
if missing:
 print('MISSING')
 for x in missing: print('MISS\t'+repr(x))
