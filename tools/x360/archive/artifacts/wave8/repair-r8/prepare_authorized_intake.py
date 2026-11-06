"""Pin the Root-authorized finite19 R8 graph before any source mutation."""
from collections import defaultdict,deque
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent;R7=HERE.parent/'repair-r7'
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');CANON=PARENT/'b5-decomp'
BASE='129afef95856e383366a55fb8f30a66ceb74de42';PUBLISHED='3534f7f16103a58ff904bbd1604e18e9934868f4'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
assert not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=CANON).decode().strip()==PUBLISHED and not git('status','--porcelain=v1',cwd=CANON).strip()
parent_index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==parent_index
preflight_path=ART/'wave9/r8-preflight-luna-20261102/preflight.json';raw=preflight_path.read_bytes()
assert hashlib.sha256(raw).hexdigest()=='0443b387bb7e6731ad82087ed597bebe2fcbad2c54bca220530000eb259a9f00'
preflight=json.loads(raw);assert len(preflight['candidates'])==19
with(HERE/'R8_FINAL_OWNER_PREFLIGHT_19.json').open('xb')as f:f.write(raw)
singles=[c for c in preflight['candidates']if c['commit']not in ['3e14d4ee26e0678f2ad95508081560dc4f8b108d','5e7486b29751d8867c9affe42eecd1d612906b45']]
byid={c['commit']:c for c in preflight['candidates']};ordered=[]
for c in singles:
 ordered.append(c)
 if c['commit']=='e52679267f7c4b1e0403b9f240aa5f3a1758bec1':ordered.append(byid['3e14d4ee26e0678f2ad95508081560dc4f8b108d'])
 if c['commit']=='cb450f33a222724a6a7194dbf707d4e8cfe1ef35':ordered.append(byid['5e7486b29751d8867c9affe42eecd1d612906b45'])
assert len(ordered)==19
tree={}
for record in git('ls-tree','-rz','--full-tree',BASE,'--','src','vendor').split(b'\0'):
 if record:
  info,path=record.split(b'\t',1);tree[path.decode()]=info.decode().split()[2]
initial={};images=dict(tree);candidates=[];entries=0
for c in ordered:
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c['commit']).decode().splitlines():
  info,path=line.split('\t',1);_,_,old,new,status=info.split();assert status=='M'and path.startswith(('src/','vendor/')),(c['commit'],path,status)
  assert images.get(path)==old,('Unexpected preimage; stop before collection',c['commit'],path,images.get(path),old)
  initial.setdefault(path,old);images[path]=new;changes.append({'path':path,'preimage':old,'postimage':new,'status':status});entries+=1
 receipt=Path(c['artifact']);assert receipt.is_file();message=git('show','-s','--format=%B',c['commit']).decode()
 candidates.append({'label':c['label'],'commit':c['commit'],'parent':git('rev-parse',c['commit']+'^').decode().strip(),'paths':changes,
  'receipt':str(receipt),'receipt_sha256':sha(receipt),'compile_scope':c.get('compile_proof'),'evidence_caveat':c.get('evidence_caveat'),
  'model_trailers':[s for s in message.splitlines()if s.lower().startswith('co-authored-by:')]})
assert len(initial)==66 and entries==69
headers={p for p in initial if Path(p).suffix.lower()in['.h','.hpp','.inl']};cpp={p for p in initial if Path(p).suffix.lower()in['.c','.cpp']};assert len(cpp)==64 and len(headers)==2
for commit,expected in [('3e14d4ee26e0678f2ad95508081560dc4f8b108d','6220e5056f8520eafb8c52efe9d67ef9c755c196d8705f88c64df5c1934cea0f'),('5e7486b29751d8867c9affe42eecd1d612906b45','c14bc212b05b24338d3ac9fcc802f16032c2d6ae1d31025e04e6ca2595a6474b')]:
 assert next(c for c in candidates if c['commit']==commit)['receipt_sha256']==expected
audit=read(R7/'full-recovered-terminal-audit.json');report=read(audit['report']);idx={r['source']:r for r in report['results']};rev=defaultdict(set)
for p,children in audit['source_snapshot']['dependencies'].items():
 for child in children:rev[child].add(p)
seen=set(headers);q=deque(headers)
while q:
 for p in rev[q.popleft()]:
  if p not in seen:seen.add(p);q.append(p)
consumers=seen&set(idx);selected=cpp|consumers;assert len(selected)==264 and sum(idx[p]['passed']for p in selected)==109
originals=[c['commit']for c in candidates];now=datetime.now(timezone.utc).isoformat()
plan={'state':'ROOT_AUTHORIZED_FINITE_R8','base':BASE,'published_base':PUBLISHED,'proposed_originals':originals,'collect_now':originals,'candidates':candidates,
 'required_order':[['2a75b7a041faa406987c506d5cedead313d1b413','fbefa2a6e84bd2d5d36a751054cc2b703396d797'],['e52679267f7c4b1e0403b9f240aa5f3a1758bec1','3e14d4ee26e0678f2ad95508081560dc4f8b108d'],['cb450f33a222724a6a7194dbf707d4e8cfe1ef35','5e7486b29751d8867c9affe42eecd1d612906b45']],
 'unique_paths':sorted(initial),'changed_file_entries':69,'new_paths':[],'removed_paths':[],'owned_CPP_sources':sorted(cpp),'changed_headers':sorted(headers),
 'planned_source_images':{p:{'baseline':initial[p],'final':images[p]}for p in sorted(initial)},'preimage_conflicts':[],
 'selected_union_native_sources':sorted(selected),'selected_requested':264,'selected_R7_passes':109,'selected_R7_failed':155,
 'owned_R7_pass_sources':sorted(p for p in cpp if idx[p]['passed']),'owned_R7_failed_sources':sorted(p for p in cpp if not idx[p]['passed']),
 'expected_native_inventory':3615,'expected_native_text_inputs':6299,'native_inventory_change':False,'source_scope':'64 existing CPPs and2 existing headers, no new files/CPPs or build/ledger/index changes',
 'consumer_method':'Conservative pinned R7 include graph, all basename matches/inactive branches;216 header consumers union64 products yields264. No predicted global gains.',
 'parent_gitlink_index':parent_index,'owner_preflight':str(HERE/'R8_FINAL_OWNER_PREFLIGHT_19.json'),'owner_preflight_sha256':hashlib.sha256(raw).hexdigest(),
 'baseline_report':audit['report'],'baseline_report_sha256':audit['report_sha256'],'baseline_audit_sha256':sha(R7/'full-recovered-terminal-audit.json'),
 'required_gates':['Root authorized exact19; no private aliases or ancestors. Preserve all66 final postimages and existing R7 changes. Stop before any unexpected preimage/conflict/path expansion.',
  'Stable selected264 actual whole-source gate; retain all109 selected R7 passes and the one changed prior-pass CgsAptCallbackRender.cpp. Keep old failures explicit.',
  'Compile current positive native/header safeguards and independent HUD7920/7940 plus WorldEntity79E0/79F0 negatives; preserve all38RCE/40World/Render/Main and MainFlow protections. Protect header direct consumers in conservative closure.',
  'Debug e526 cannot be accepted independently of3e14; final Origin/RightUp initializer ordering must match root-reviewed actual ARTIST CRT. Gui cb450 must be followed by5e748 attested GetDefault and hostR1 removal. Retain original semantic negative/compile evidence.',
  'Retain FontRenderer native API/member shape and genuine BoundaryLine direct caller closure. Use existing owner ABI/CRT/case evidence with precise limits; no optional new runtime suites for mechanical lanes.',
  'After selected/native retention and stable hashes/context only, one actual3615/6299 full diagnostic jobs8 comparing all3168 R7 passes and3400 published passes. No forced link/inventory pruning or restart on timeout.',
  'All outputs/TEMP on D; prepared native SDK only. Canonical3534/main/dev/origin/parent-index/ledger untouched; no formal review/publication.'],
 'known_limits':['Most lane/C++03 batches are compile/codegen evidence, not whole behavior. Preserve old scalar/FP/estimate/unsupported-service debts.',
  'Stunt source-change.json after_sha256 is stale; final delivery/commit/source-object binding are authoritative and auxiliary receipt remains preserved.',
  'BrnFlaptRenderer whole source remains prior78 diagnostic failure despite native FontRenderer declaration closure; broad header consumers require actual integrated gate.',
  'R7 interrupted1700 plus missing1915 lineage remains immutable; R7 compile3168/447,321 published pass regressions. Root strict link diagnostic failed with unresolved/duplicates and no EXE; no link/image/launch proof.',
  'Retain HUD allocation,WorldEntity,mesh/cache/Prop/Scene,quartic/decoder,alignment,software-estimate/hardware and old raw failures; do not fabricate layout or normalize addresses.'],
 'excluded':['Geometry/scene later7b4e83/c423ad18','GameModule Sol WIP','Native link input exclusions/provider duplicate fixes','Any further unlisted source or optional owner-runtime expansion'],
 'authorization':'Root reviewed18 snapshot17769970 plus exact5e7486 and both successor diffs/receipt hashes; authorized exact19 collection,selected264jobs8 and one stable full conditional on retained passes. No further permission required for this finite scope.',
 'source_changed':False,'compiler_started':False,'formal_review_requested':False,'publication_allowed':False,'created_utc':now}
save(HERE/'R8_PREFLIGHT_PLAN_01.json',plan)
save(HERE/'R8_ROOT_AUTHORIZATION_01.json',{'plan_sha256':sha(HERE/'R8_PREFLIGHT_PLAN_01.json'),'originals':originals,'root_reviewed18_preflight_sha256':'177699701f0055b1a536f6c7740f035ba5aad4feac304edc3de102417de3dfd2','final19_owner_preflight_sha256':hashlib.sha256(raw).hexdigest(),'authorization':plan['authorization'],'created_utc':now})
config={'campaign':'wave8-repair-r8','base':BASE,'published_base':PUBLISHED,'workspace':str(WORK),'canonical':str(CANON),'artifact_root':str(HERE),
 'baseline_metadata':str(R7/'native-parallel-wave8-r7-full-recovered-patch.json'),'baseline_report':audit['report'],'baseline_artifact_manifest':str(R7/'full-recovered-terminal-audit.json'),
 'published_report':read(R7/'campaign.json')['published_report'],'published_artifact_manifest':read(R7/'campaign.json')['published_artifact_manifest'],
 'baseline_passed':3168,'baseline_failed':447,'baseline_requested':3615,'current_plan':str(HERE/'R8_PREFLIGHT_PLAN_01.json')}
for name in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:config[name+'_sha256']=sha(config[name])
save(HERE/'campaign.json',config);save(HERE/'repair-r8-receipts.json',{'state':'complete','workspace':str(WORK),'head':BASE,'receipts':[],'queue':[],'todo':[]})
text='# Authorized finite R8 plan\n\n19 exact originals;66 existing paths (64 CPPs,2 headers),no inventory/input-count change:3615 native sources,6299 code inputs. All current preimages agree except the three explicitly ordered successor paths,which follow their exact parent deltas. No private ancestry or aliases are collected.\n\nThe selected264-source union protects109 R7 passes,including changed CgsAptCallbackRender.cpp. It covers216 conservative FontRenderer/BoundaryLine header consumers and all64 owned CPPs. Old155 selected failures remain explicit; private compile totals do not predict global gains.\n\nPairs:2a75→fbefa; e526→3e14 inseparably (actual native CRT ordering); cb450→5e748 (attested default transform and host witness removal). All existing native guards and independent HUD/WorldEntity negatives remain. Existing owner evidence is retained with its original limits; no optional runtime-suite expansion.\n\nAfter stable selected/header retention,one full3615/6299 diagnostic may run. No canonical/main/dev/index/ledger/publication changes. Geometry,scene,GameModule and link-input work stay outside this finite cutoff.\n'
with(HERE/'R8_PREFLIGHT_SUMMARY_01.md').open('x',encoding='utf-8')as f:f.write(text)
print('PINNED',sha(HERE/'R8_PREFLIGHT_PLAN_01.json'),'originals',19,'paths',66,'selected',264,'protected',109,flush=True)
