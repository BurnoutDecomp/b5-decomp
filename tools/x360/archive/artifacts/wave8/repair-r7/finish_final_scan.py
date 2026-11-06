"""Extend stored R7 ready scan by only two final sealed nodes; no replay/collection."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');BASE='ef64014612b510bffa6fa17bebe91461bd95a0ab'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=WORK).decode().strip()
old=read(HERE/'R7_READY_SCAN_01.json');status=read(HERE/'R7_TARGETED_STATUS_01.json');assert git('rev-parse','HEAD')==BASE and not git('status','--porcelain=v1')
rows=list(old['candidates'])
for c in status['newly_sealed_pending_deltas']:
 rows.append({'commit':c['commit'],'parents':[c['parent']],'message':git('show','-s','--format=%B',c['commit']),'paths':[{'path':p['path'],'preimage':p['preimage'],'postimage':p['postimage'],'R6_blob':git('rev-parse',BASE+':'+p['path']),'first_touch':not any(p['path']==q['path']for r in rows for q in r['paths']),'planned_predecessor':p['preimage'],'preimage_matches':True}for p in c['paths']]})
tree={line.split('\t',1)[1]:line.split('\t',1)[0].split()[2]for line in git('ls-tree','-r','HEAD').splitlines()};virtual=dict(tree);seen=set();paths=set();new=[]
for r in rows:
 for p in r['paths']:
  assert virtual.get(p['path'])==p['preimage'];virtual[p['path']]=p['postimage'];seen.add(p['path']);paths.add(p['path'])
for commit in ['340a698e5cfa823c1bc48579530ebc966d15219a','7a3b9fe6178b95534973434dd034b7496f05b14c']:
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',commit).splitlines():
  left,path=line.split('\t',1);_,_,pre,post,kind=left[1:].split();assert kind in ['A','M'];expected=virtual.get(path,'0'*40)
  changes.append({'path':path,'status':kind,'preimage':pre,'postimage':post,'R6_blob':tree.get(path),'first_touch':path not in seen,'planned_predecessor':virtual.get(path),'preimage_matches':expected==pre});virtual[path]=post;seen.add(path);paths.add(path)
  if kind=='A':new.append(path)
 rows.append({'commit':commit,'parents':git('show','-s','--format=%P',commit).split(),'message':git('show','-s','--format=%B',commit),'paths':changes})
assert len(rows)==20 and len({r['commit']for r in rows})==20
raw=read(HERE.parent/'repair-r6/full-stage1/compile-results.json');index={r['source']:r for r in raw['results']}
source_binding={p:{'R6_blob':tree.get(p),'planned_postimage':virtual[p],'planned_git_image_sha256':hashlib.sha256(subprocess.check_output(['git','--no-optional-locks','cat-file','blob',virtual[p]],cwd=WORK)).hexdigest()}for p in sorted(paths)}
aliases=[{'original':'a3633b1049ee5a5e66376dcd7a503025886a2fa9','private_alias':'918541007db15a1f5570dcd63d8369cf57fc01f5','paths':['src/GameSource/Gui/Flow/HUD/Components/BrnEventInfo.cpp']},{'original':'75bbef15366ccfcffbcb0f06f7a1fc7d58a2bb7b','private_alias':'12aa940535f9002fa0ef50c6e7198b54db07b9c1','paths':['src/GameSource/Gui/Flow/HUD/Components/BrnEventInfo.cpp']},{'original':'48b937a6c0f5ca688732402598ee378768e0d5fb','private_alias':'7d96c145af6e38ff431c9adc0e623fdf3e30558e','paths':git('diff-tree','--no-commit-id','--name-only','-r','48b937a6c0f5ca688732402598ee378768e0d5fb').splitlines()}]
for a in aliases:
 a['all_original_alias_postimages_equal']=all(git('rev-parse',a['original']+':'+p)==git('rev-parse',a['private_alias']+':'+p)for p in a['paths']);assert a['all_original_alias_postimages_equal']
 a['action']='SKIP private alias/ancestry; use original dependency only once, or already-published Matrix union.'
out={'state':'FINAL20_ORIGINAL_R7_SOURCE_PREFLIGHT_NO_MUTATION','base':BASE,'proposed_originals':[r['commit']for r in rows],'candidates':rows,'unique_paths':sorted(paths),'owned_CPP_sources':sorted(p for p in paths if p.endswith('.cpp')),'changed_headers':sorted(p for p in paths if p.endswith(('.h','.hpp'))),'new_paths':new,'removed_paths':[],'preimage_conflicts':[p for r in rows for p in r['paths']if not p['preimage_matches']],'planned_source_images':source_binding,'owned_R6_pass_sources':sorted(p for p in paths if index.get(p,{}).get('passed')),'owned_R6_failed_sources':sorted(p for p in paths if p in index and not index[p]['passed']),'expected_native_inventory':3615,'expected_native_text_inputs':6299,'aliases':aliases,'prior_scan_reused_not_rerun':{'path':str(HERE/'R7_READY_SCAN_01.json'),'sha256':sha(HERE/'R7_READY_SCAN_01.json')},'source_changed':False,'compiler_started':False,'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'R7_FINAL_SOURCE_SCAN_01.json').open('x')as f:json.dump(out,f,indent=2)
print(json.dumps({'originals':len(rows),'paths':len(paths),'CPPs':len(out['owned_CPP_sources']),'headers':len(out['changed_headers']),'new_paths':new,'preimage_conflicts':out['preimage_conflicts'],'owned_R6_pass':len(out['owned_R6_pass_sources']),'owned_R6_fail':len(out['owned_R6_failed_sources']),'scan_sha256':sha(HERE/'R7_FINAL_SOURCE_SCAN_01.json')},indent=2))
