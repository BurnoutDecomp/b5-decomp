"""READ ONLY repository scan for proposed R6; writes evidence only on D:."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
CANON=PARENT/'b5-decomp'
BASE='04ed276fcd79c0d91e5e91db9cd08438c4e0689d'
PUBLISHED='3534f7f16103a58ff904bbd1604e18e9934868f4'
GROUPS=[('XCam','f853f4441009ddebea4ef444b30c85ddb729b91e'),('XAudio','c125288518ce84b420c475dd86c5e49d8c30f6d2'),('MassiveSocket','6f514bbb067ba0bf8b57e4e8e44ccb84821b8365'),('GPU','19e7b58178ff514576df13cab8ad0831f8b45d0e'),('GPUFrequency','498c2ae1aaec0346e31186e667beb0add7677e9e'),('ButterworthFilter','00af37297afa14eb781609fa8ddc7073e4b76f01'),('GUI','e7fad0c3c6080e5a8bf6a50f6495c48fb70bb94a'),('GUI','f4a2d13fed6ffaa5d2e2ce9982f72e1b692c501a'),('GUI','ac6e1d3d58e04d5a2a274ad532d85b694c8cd727'),('GUI','745533f0125b2d16bf853292584bc32fb2135e1f'),('GUI','e40a86cbd057b176e02680f1aeff8308b0dd8850'),('KeyboardTriangle','85c1e4cea110f656e52c77859e82c6943ffb513f'),('KeyboardTriangle','e50eed670805dbd2a75928558a7052fc040bd202'),('ExternalBodyPhysics','fdf52c73c6bbebd03d89003dab4686ee75701a50')]
def git(*args,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=cwd)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,d):
 with Path(p).open('x',encoding='utf-8') as f:json.dump(d,f,indent=2);f.write('\n')
assert git('rev-parse','HEAD').decode().strip()==BASE
assert not git('status','--porcelain=v1').strip()
assert git('rev-parse','HEAD',cwd=CANON).decode().strip()==PUBLISHED and not git('status','--porcelain=v1',cwd=CANON).strip()
r5=HERE.parent/'repair-r5'
meta=json.loads((r5/'native-parallel-wave8-r5-full-stage1-patch.json').read_text())
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
tree={}
for line in git('ls-tree','-r','HEAD').decode().splitlines():
 left,path=line.split('\t',1);mode,kind,blob=left.split();tree[path]={'mode':mode,'kind':kind,'blob':blob}
virtual={p:v['blob'] for p,v in tree.items()};seen=set();candidates=[];allpaths=set();deletions=set();added=set();overlaps=[];post={}
patchdir=HERE/'original-diffs-02';assert not patchdir.exists();patchdir.mkdir()
for group,commit in GROUPS:
 message=git('show','-s','--format=%H%n%P%n%B',commit).decode();resolved,parent,*body=message.splitlines();assert resolved==commit
 patch=git('diff',commit+'^',commit,'--binary');patchpath=patchdir/(commit+'.diff');patchpath.write_bytes(patch)
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',commit).decode().splitlines():
  left,path=line.split('\t',1);oldmode,newmode,oldblob,newblob,status=left[1:].split();assert status in ['A','M','D'],(commit,status,path)
  first=path not in seen;expected=virtual.get(path,'0'*40)
  changes.append({'status':status,'path':path,'preimage':oldblob,'postimage':None if status=='D' else newblob,'first_touch':first,'R5_blob':tree.get(path,{}).get('blob'),'planned_predecessor_blob':None if expected=='0'*40 else expected,'preimage_matches_planned':oldblob==expected,'R5_passed':next((r['passed'] for r in json.loads((r5/'full-stage1/compile-results.json').read_text())['results'] if r['source']==path),None)})
  if not first:overlaps.append({'path':path,'commit':commit,'previous_planned_blob':expected,'next_preimage':oldblob,'matches':oldblob==expected})
  if status=='D':virtual.pop(path,None);deletions.add(path)
  else:virtual[path]=newblob;post[path]=newblob
  if status=='A':added.add(path)
  seen.add(path);allpaths.add(path)
 candidates.append({'group':group,'commit':commit,'parents':parent.split(),'message':'\n'.join(body),'patch':str(patchpath),'patch_sha256':sha(patchpath),'paths':changes})
native=set(meta['native_inventory']);prospective=set(native)
for p in deletions:prospective.discard(p)
for p in added:
 if Path(p).suffix.lower() in ['.c','.cpp'] and p.startswith(('src/','vendor/')) and 'pc' not in [s.lower() for s in Path(p).parts]:prospective.add(p)
inputs=set(meta['source_snapshot']['hashes']);nextinputs=inputs-deletions
assert not added,'New source classification must be inspected explicitly'
raw=json.loads((r5/'full-stage1/compile-results.json').read_text());index={r['source']:r for r in raw['results']}
ownedcpp=sorted(p for p in allpaths if Path(p).suffix.lower() in ['.cpp','.c'])
bindings={}
for p in sorted(allpaths):
 current=WORK/p
 bindings[p]={'R5_blob':tree.get(p,{}).get('blob'),'R5_raw_sha256':sha(current) if current.is_file() else None,'planned_postimage':None if p in deletions else virtual[p]}
 if p not in deletions:
  b=git('cat-file','blob',virtual[p]);bindings[p]['planned_git_image_sha256']=hashlib.sha256(b).hexdigest()
baseline={'head':BASE,'canonical':PUBLISHED,'source_clean':True,'canonical_clean':True,'source_input_hashes':len(inputs),'all_R5_input_hashes_match':True,'native_inventory':len(native),'R5_raw_report_sha256':sha(r5/'full-stage1/compile-results.json'),'R5_audit_sha256':sha(r5/'full-stage1-terminal-audit.json'),'parent_gitlink_index':git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip(),'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R6_BASELINE_BINDING_02.json',baseline)
result={'state':'READ_ONLY_PREFLIGHT_NO_SOURCE_OR_COMPILER_ACTION','base':BASE,'candidate_count':len(GROUPS),'candidates':candidates,'unique_paths':sorted(allpaths),'changed_existing_CPP_paths_including_removed':ownedcpp,'new_paths':sorted(added),'removed_paths':sorted(deletions),'changed_headers':sorted(p for p in allpaths if Path(p).suffix.lower() in ['.h','.hpp']), 'first_touch_or_repeated_preimage_conflicts':[x for c in candidates for x in c['paths'] if not x['preimage_matches_planned']],'overlapping_commit_paths':overlaps,'source_image_binding':bindings,'owned_R5_pass_sources':sorted(p for p in ownedcpp if index.get(p,{}).get('passed')),'owned_R5_failed_sources':sorted(p for p in ownedcpp if p in index and not index[p]['passed']),'retired_R5_passing_sources':sorted(p for p in deletions if index.get(p,{}).get('passed')),'retired_R5_failed_sources':sorted(p for p in deletions if p in index and not index[p]['passed']),'expected_native_inventory':len(prospective),'expected_native_text_inputs':len(nextinputs),'inventory_removed':sorted(native-prospective),'inventory_added':sorted(prospective-native),'CMake_tracked_delta_separately_bound':any(Path(p).name=='CMakeLists.txt' for p in allpaths),'baseline_binding_sha256':sha(HERE/'R6_BASELINE_BINDING_02.json'),'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R6_CANDIDATE_SCAN_02.json',result)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
print(json.dumps({k:result[k] for k in ['candidate_count','expected_native_inventory','expected_native_text_inputs','removed_paths','retired_R5_passing_sources','first_touch_or_repeated_preimage_conflicts']}))
print(json.dumps({'paths':len(allpaths),'CPP_including_removed':len(ownedcpp),'headers':len(result['changed_headers']),'owned_R5_pass':len(result['owned_R5_pass_sources']),'scan_sha256':sha(HERE/'R6_CANDIDATE_SCAN_02.json')}))
