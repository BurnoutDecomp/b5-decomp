"""Read-only finite R16 metadata and projected consumer plan;no collection/build."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r15-network-repair-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='2beeee2e9b4445c51d2e7113820c6a3b5c1b1e5b'
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip()
index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
assert sha(B/'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R15.json')=='e11f2548b3694fe519fb1b940f3c996a09f9f3bdfca20a04b6cc9cc0836d1889'
assert sha(B/'composite-effective.json')=='91db33349225a7037821c3e4d286a83e6417803652ec05b5b046934a59c3271d'
assert sha(B/'composite-artifact-audit.json')=='6e63df0663a81de51306b5dde4605886e42a6c3b64fd1d8d2f374b15ec7236a5'
manifest=A/'wave12/R16_READY_CANDIDATES.json';m=read(manifest);assert m['baseline']==BASE
baseline=read(B/'composite-effective.json');old={r['source']:r for r in baseline['results']};audit=read(B/'composite-artifact-audit.json');snap=audit['source_snapshot'];assert len(snap['hashes'])==6303
images={};owners={};finaltext={};candidates=[];receipts=[]
for group in m['groups']:
 receipt=A/group['receipt'];receipts.append({'path':str(receipt),'sha256':sha(receipt),'group':group['name']})
 for c in group['commits']:
  parent=git('rev-parse',c+'^').decode().strip();changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status=='M'and Path(p).suffix.lower()in ['.cpp','.h','.hpp']
   if p in images:assert owners[p]==group['name']and images[p]['final']==a
   else:assert git('rev-parse',BASE+':'+p).decode().strip()==a;images[p]={'baseline':a,'final':a};owners[p]=group['name']
   images[p]['final']=b;finaltext[p]=git('show',c+':'+p).decode('utf-8-sig',errors='replace');changes.append({'path':p,'preimage_blob':a,'postimage_blob':b,'status':status})
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'paths':changes})
paths=sorted(images);cpp=sorted(p for p in paths if p.endswith('.cpp'));headers=sorted(set(paths)-set(cpp));assert len(candidates)==10 and len(paths)==41 and len(cpp)==40 and headers==['src/SharedClasses/Traffic/Junctions/BrnTrafficStopLine.h']
# Project literal dependencies from immutable final blobs only;no patch application/dry-run or working-tree edits.
byname=defaultdict(set);lookup={p.lower():p for p in snap['hashes']}
for p in snap['hashes']:byname[Path(p).name.lower()].add(p)
projected={p:list(v)for p,v in snap['dependencies'].items()};new_dynamic=[]
for p,text in finaltext.items():
 deps=set()
 for directive in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',text,re.M):
  match=re.match(r'[<"]([^>"\r\n]+)[>"]',directive)
  if not match:new_dynamic.append({'path':p,'directive':directive});continue
  inc=match[1].replace('\\','/');deps.update(byname.get(Path(inc).name.lower(),[]));local=(W/Path(p).parent/inc).resolve()
  try:rel=local.relative_to(W).as_posix().lower()
  except ValueError:rel=''
  if rel in lookup:deps.add(lookup[rel])
 projected[p]=sorted(deps)
reverse=defaultdict(set)
for graph in [snap['dependencies'],projected]:
 for parent,children in graph.items():
  for child in children:reverse[child].add(parent)
def closure(seeds):
 reached=set(seeds);q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 return reached
native=set(old);hcons=sorted(closure(headers)&native);selected=sorted(closure(paths)&native);cppincoming={p:sorted(set(ch)&set(cpp))for p,ch in projected.items()if set(ch)&set(cpp)};assert set(cpp)<=set(selected)and not new_dynamic
facts={'baseline_graph_head':BASE,'baseline_graph_hash_rows':6303,'method':'Conservative union of sealed baseline dependencies and final immutable candidate-blob literal includes;all basename matches and inactive branches retained. Planning projection only;must recompute after authorized collection and reconcile exact preflight.','changed_header':headers[0],'header_native_consumers':hcons,'header_native_consumer_count':len(hcons),'selected_sources':selected,'selected_native_count':len(selected),'retained_native_count':3616-len(selected),'selected_prior_PASS':sum(old[p]['passed']for p in selected),'selected_prior_FAIL':sum(not old[p]['passed']for p in selected),'candidate40_prior_PASS':sum(old[p]['passed']for p in cpp),'candidate40_prior_FAIL':sum(not old[p]['passed']for p in cpp),'new_nonliteral_sites_in_candidates':new_dynamic,'incoming_changed_CPP_literal_edges':cppincoming,'header_include_directives_changed':snap['dependencies'][headers[0]]!=projected[headers[0]],'external_context_recheck':'Deferred until authorized runtime input pin;finite preflight provides exact consumer reconciliation.'}
save(H/'projected-consumer-plan.json',facts)
plan={'state':'READ_ONLY_PREPARED_AWAITING_PREFLIGHT_REVIEW_ROOT_RELEASE','base':BASE,'author_base':m['author_base'],'manifest':str(manifest),'manifest_sha256':sha(manifest),'commits':[c['commit']for c in candidates],'candidates':candidates,'receipts':receipts,'paths':paths,'planned_source_images':images,'changed_CPP':cpp,'changed_headers':headers,'new_paths':[],'removed_paths':[],'native_inventory':3616,'text_inputs':6303,'baseline_passed':3367,'baseline_failed':249,'projected_consumer_plan':str(H/'projected-consumer-plan.json'),'projected_consumer_plan_sha256':sha(H/'projected-consumer-plan.json'),'proposed_native_jobs':'At most8;finalRootresourceallocation required before launch.','parent_gitlink_index':index,'required_execution':['Require exact sealed metadata preflight,one-batch fresh review and separate explicit Root collection release before any source mutation.','Collect the10 manifest commits sequentially with each raw path/preimage/postimage verified;bothscene commits required;do not collect private preparation/nonmanifest refs.','Recompute source6303/native3616 and conservative old/new header+CPP include closure on final frozen tree;reconcile preflight actual include consumers;expand selected gate only if evidence requires.','Compile all40 changed CPP plus every actual/conservative changed-header or changed-CPP consumer fresh;canonical9fallbacks/showIncludes/D-only TEMP+outputs. No author objects substituted.','Verify reused R15 actual objects/listings/logs against current sealed manifest once;prove unaffected source/header/input/tool/external/submodule/flag identity. Preserve underlying3200 phase1 and2be repair build provenance per retained row.','Rerun only materially affected named native safeguards. Compare all3367 prior passes and real gains;old failed consumers stay FAIL unless fresh native gate proves a gain.','If regressions/timeouts appear,pin exact logs and remain pending for necessary genuine fixes;no stubs,fakefields,guardbypass,inventorydrops or residual-regression seal.','Explicit complete coverage must identifyfresh selected rows +verified unaffectedR15 rows,not a fresh full sweep. No full replay unless consumer closure cannot be soundly bounded.'], 'excluded':m['excluded'],'limits':m['limits'],'no_collection_or_build_performed':True,'no_patch_apply_or_dry_run':True,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R16_FINITE_GATE_PLAN.json',plan)
config={'base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R16_FINITE_GATE_PLAN.json'),'baseline_seal':str(B/'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R15.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'command.json')}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command']:config[k+'_sha256']=sha(config[k])
save(H/'campaign-preparation.json',config)
md=f"# R16 finite native gate preparation\n\n**Read-only preparation;not authorized to collect or build.** Intake remains clean detached `{BASE}`. Exact closed manifest:10 sequential original commits/9groups/41paths =40CPP+oneStopLine header. No new files or inventory changes;expected6303text inputs/3616native units.\n\nAll first preimages match currentR15;required scene successor order is preserved. Immutable-source projection finds **{len(hcons)} StopLine header native consumers** and **{len(selected)} total native units** after union with changedCPP/include consumers,leaving **{3616-len(selected)} provisional unaffected rows**. Selected prior status:{facts['selected_prior_PASS']}PASS/{facts['selected_prior_FAIL']}FAIL;candidate40 status:{facts['candidate40_prior_PASS']}PASS/{facts['candidate40_prior_FAIL']}FAIL. These are planning counts;exact preflight and postcollection closure must agree before the final gate is fixed.\n\nAfter explicit Root release:collect exact order,freeze/recompute closure,compile all selected units freshly with canonical9fallbacks/showIncludes/privateD TEMP,verify unaffectedR15 products and full input context once,and preserve real per-row3200/2be build origins. Compare all3367prior passes;pin genuine regressions rather than bypassing them. Rerun only affected native safeguards. No full replay unless closure cannot be bounded.\n\nVideo-only82477 is included;activeAPT,NFSMix/DAC/GUI-clear successors and all nonmanifest refs are excluded. Canonical3534,parentindex209,main/dev/origin/ledger/publication remain untouched. Await sealed metadata preflight,batch review,and explicit collection release.\n\nPlan SHA256 `{sha(H/'R16_FINITE_GATE_PLAN.json')}`. Projection SHA256 `{sha(H/'projected-consumer-plan.json')}`.\n"
with(H/'R16_FINITE_GATE_PLAN.md').open('x',encoding='utf-8')as f:f.write(md)
print(json.dumps({'state':plan['state'],'base':BASE,'commits':10,'CPP':40,'headers':1,'projected_header_consumers':len(hcons),'projected_gate':len(selected),'projected_retained':3616-len(selected),'selected_prior_PASS':facts['selected_prior_PASS'],'selected_prior_FAIL':facts['selected_prior_FAIL'],'plan_sha256':sha(H/'R16_FINITE_GATE_PLAN.json'),'projection_sha256':sha(H/'projected-consumer-plan.json'),'source_mutated':False,'compiler_started':False},indent=2))
