"""Preserve actual raw native compilation provenance for the thread/join checkpoint."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';SCRATCH=Path(__file__).parent
meta=json.loads((SCRATCH/'native-thread-join-patch.json').read_text())
assert hashlib.sha256(subprocess.check_output(['git','diff','--binary',meta['base']],cwd=ROOT)).hexdigest()==meta['sha256']
for p,expected in meta['source_sha256'].items():assert hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==expected,p
def read(p):
    d=json.loads((ROOT/p).read_text());assert d['state']=='complete'
    assert d['passed']==sum(r['passed'] for r in d['results'])
    assert d['requested']==len(d['results'])==d['passed']+d['failed'];return d
raw_path='build/x360/native-thread-join-final/compile-results.json'
doc_path='build/x360/native-thread-join-doc/compile-results.json'
raw=read(raw_path);doc=read(doc_path)
repair_path='build/x360/native-thread-join-recheck2/compile-results.json'
repair=read(repair_path)
assert len(repair['results'])==1 and repair['results'][0]['source']=='src/SDKs/EATech/eathread/BrnEAThreadX360.cpp' and repair['results'][0]['passed']
assert len(doc['results'])==1 and doc['results'][0]['source']=='src/SDKs/EATech/eajobs/job_scheduler.cpp' and doc['results'][0]['passed']
rows={r['source']:dict(r,verified_in_report=raw_path) for r in raw['results']}
rows.update({r['source']:dict(r,verified_in_report=doc_path) for r in doc['results']})
rows.update({r['source']:dict(r,verified_in_report=repair_path) for r in repair['results']})
selection=json.loads((SCRATCH/'native-thread-join-selection.json').read_text())
assert set(rows)==set(selection['sources']) and selection['patch_sha256']==meta['sha256']
complete={k:v for k,v in raw.items() if k!='results'};complete['results']=sorted(rows.values(),key=lambda r:r['source'])
complete['provenance']={'raw_sweep':raw_path,'comment_only_followup':doc_path,'thread_publication_recheck':repair_path,'patch_sha256':meta['sha256']}
dest=ROOT/'build/x360/native-thread-join-complete/compile-results.json';dest.parent.mkdir(exist_ok=True)
dest.write_text(json.dumps(complete,indent=2)+'\n')
prior_path='build/x360/native-frame-effective.json';prior=read(prior_path)
allrows={r['source']:r for r in prior['results']};assert set(rows)<=set(allrows);allrows.update(rows)
passed=sum(r['passed'] for r in allrows.values());gains=sorted(r['source'] for r in prior['results'] if not r['passed'] and allrows[r['source']]['passed'])
regressions=[r['source'] for r in prior['results'] if r['passed'] and not allrows[r['source']]['passed']];assert not regressions,regressions
effective={k:v for k,v in raw.items() if k!='results'}
effective.update(requested=len(allrows),passed=passed,failed=len(allrows)-passed,results=sorted(allrows.values(),key=lambda r:r['source']),
                 provenance={'method':'prior verified full/effective set plus666 old/new literal include closure and final thread recheck/comment-only CPP followups',
                             'base_report':prior_path,'delta_report':'build/x360/native-thread-join-complete/compile-results.json',
                             'selection':'scratch/x360_sdk/native-thread-join-selection.json','patch_sha256':meta['sha256']})
(ROOT/'build/x360/native-thread-join-effective.json').write_text(json.dumps(effective,indent=2)+'\n')
print(json.dumps({'passed':passed,'failed':len(allrows)-passed,'total':len(allrows),'old_passes_retained':prior['passed'],'new_passes':gains,'regressions':regressions},indent=2))
