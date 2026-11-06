import json,re,collections,hashlib
from pathlib import Path
cur=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/full-stage1/compile-results.json')
old=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave5-full/compile-results.json')
report=json.loads(cur.read_text(encoding='utf-8'))
prev=json.loads(old.read_text(encoding='utf-8'))
pat=re.compile(r'^(.*?)(?:\((\d+)\))?\s*:\s*(?:fatal )?error\s+(C\d+|[A-Z]+\d+):\s*(.*)$',re.I)
def relpath(path):
 p=path.replace('\\','/')
 m=re.search(r'((?:src|vendor)/.*)$',p,re.I)
 return m.group(1) if m else Path(p).name
def parse(x):
 lp=Path(x['log']); out=[]
 if not lp.is_file(): return out
 for line in lp.read_text(encoding='utf-8',errors='replace').splitlines():
  s=line.strip()
  if 'error ' not in s.lower(): continue
  m=pat.match(s)
  if not m: continue
  loc=relpath(m.group(1)); ln=m.group(2) or ''; code=m.group(3).upper(); msg=' '.join(m.group(4).split())
  out.append({'location':loc,'line':ln,'code':code,'message':msg,'raw':s})
 return out
cr={x['source']:x for x in report['results']}; pr={x['source']:x for x in prev['results']}
passed=lambda x:{s for s,r in x.items() if r.get('passed')}
failed=lambda x:{s for s,r in x.items() if not r.get('passed')}
oldpass,newpass=passed(pr),passed(cr); oldfail,newfail=failed(pr),failed(cr)
trans={
 'old_fail_now_pass':sorted(oldfail & newpass),
 'old_pass_now_fail':sorted(oldpass & newfail),
 'fail_both':sorted(oldfail & newfail),
 'new_sources_pass':sorted((set(cr)-set(pr)) & newpass),
 'new_sources_fail':sorted((set(cr)-set(pr)) & newfail),
 'removed_sources':sorted(set(pr)-set(cr)),
}
rows=[]; site=collections.defaultdict(set); msg=collections.defaultdict(set); header=collections.defaultdict(set); allcode=collections.Counter()
for s,x in cr.items():
 if x.get('passed'): continue
 ds=parse(x); rows.append({'source':s,'log':x.get('log'),'diagnostics':ds})
 if not ds: continue
 d=ds[0]; k=(d['location'],d['line'],d['code'],d['message']); site[k].add(s)
 mk=(d['code'],d['message']); msg[mk].add(s)
 for dd in ds:
  allcode[(dd['code'],dd['message'])]+=1
  if dd['location'].lower().endswith(('.h','.hpp','.inl')): header[(dd['location'],dd['code'],dd['message'])].add(s)

def arr(counter,n=80):
 out=[]
 for k,ss in sorted(counter.items(),key=lambda kv:(-len(kv[1]),str(kv[0])))[:n]: out.append({'key':list(k),'tu_count':len(ss),'sources':sorted(ss)})
 return out
obj={
 'current':{'path':str(cur),'sha256':hashlib.sha256(cur.read_bytes()).hexdigest(),'requested':report['requested'],'passed':report['passed'],'failed':report['failed'],'state':report['state']},
 'previous':{'path':str(old),'sha256':hashlib.sha256(old.read_bytes()).hexdigest(),'requested':prev['requested'],'passed':prev['passed'],'failed':prev['failed'],'state':prev['state']},
 'transitions':{k:{'count':len(v),'sources':v} for k,v in trans.items()},
 'failed_rows':rows,
 'primary_sites':arr(site,242),
 'primary_signatures':arr(msg,120),
 'shared_header_diagnostics':arr(header,160),
 'all_diagnostic_frequency':[{'code':k[0],'message':k[1],'occurrences':n} for k,n in allcode.most_common(200)],
}
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json')
out.write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8')
print('REPORTS',obj['previous'],obj['current'])
for k,v in obj['transitions'].items(): print('TRANSITION',k,v['count'])
for title,k in [('PRIMARY_SITES','primary_sites'),('PRIMARY_SIGS','primary_signatures'),('SHARED_HEADERS','shared_header_diagnostics')]:
 print('---',title)
 for x in obj[k][:80]: print(x['tu_count'],*x['key'])
print('TOTAL_FAILED_LOGS',len(rows),'NO_DIAG',sum(not r['diagnostics'] for r in rows),'SUMMARY',out)
