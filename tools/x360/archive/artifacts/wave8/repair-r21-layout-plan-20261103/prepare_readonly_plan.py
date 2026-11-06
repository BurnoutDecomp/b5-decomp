"""R21 immutable-blob projection. No source mutation or compile."""
from pathlib import Path
from collections import defaultdict,deque
from datetime import datetime,timezone
import json,hashlib,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r20-intake-20261103'
W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
BASE='5b7074599d5bf4282031094499750aac1417c15b';TEXT={'.cpp','.c','.h','.hpp','.inl','.inc','.ipp'}
COMMITS=['b85657d5feb9147ddc379106605408ea92192d76','ef8c37c279e070814a59880ff1ea6bed31c6d148','ba05880df217c6c75ec63b7a7534b6c2f9c31121','3937f02d3fade39ddc65a39ed294616ba83c6804']
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob(c,p):
 r=subprocess.run(['git','--no-optional-locks','rev-parse',c+':'+p],cwd=W,capture_output=True,text=True)
 return r.stdout.strip()if r.returncode==0 else None
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
assert not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip()
index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
pins={'FROZEN_COMPOSITE_DIAGNOSTIC_R20.json':'e2db247d2eff3a2ea4860160fc8a9ec5f8a272a17754ba6f02c6a99c29468666','composite-active-native.json':'51fe2e2903bfe91a62b8076f9c6ec3f954a2e8243ebb963e153caedc6480de99','composite-artifact-audit.json':'f3069b1bd726be2cda8bb1ef2c1a86d43c75fe232b6edffb3a4442c1a8378894'}
for p,s in pins.items():assert sha(B/p)==s
baseline=read(B/'composite-active-native.json');audit=read(B/'composite-artifact-audit.json');snapshot=audit['source_snapshot'];old={r['source']:r for r in baseline['results']}
assert len(old)==3616 and baseline['passed']==3615 and baseline['failed']==1 and len(snapshot['hashes'])==6305
images={};finaltext={};candidates=[];issues=[];counts=defaultdict(int)
for c in COMMITS:
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
  info,p=line.split('\t',1);_,_,pre,post,status=info.split();assert status in ['M','A']and Path(p).suffix.lower()in TEXT
  counts[p]+=1;initial=blob(BASE,p);expected=None if pre=='0'*40 else pre;current=images.get(p,{}).get('final',initial)
  if current!=expected:issues.append({'commit':c,'path':p,'projected_preimage_blob':current,'required_preimage_blob':expected})
  images[p]={'baseline':initial,'final':post};finaltext[p]=git('show',c+':'+p).decode('utf-8-sig',errors='replace')
  changes.append({'path':p,'status':status,'preimage_blob':expected,'postimage_blob':post})
 candidates.append({'commit':c,'parent':git('rev-parse',c+'^').decode().strip(),'paths':changes})
paths=sorted(images);cpp=sorted(p for p in paths if Path(p).suffix.lower()in {'.cpp','.c'});headers=sorted(set(paths)-set(cpp));added=sorted(p for p,v in images.items()if v['baseline']is None)
assert len(paths)==22 and len(cpp)==10 and len(headers)==12 and not added
futurekeys=set(snapshot['hashes'])|set(added);native=sorted(set(old)|set(cpp));assert len(futurekeys)==6305 and len(native)==3616
byname=defaultdict(set);lookup={p.lower():p for p in futurekeys}
for p in futurekeys:byname[Path(p).name.lower()].add(p)
graph={};dynamic=[]
for p in futurekeys:
 text=finaltext.get(p)
 if text is None:text=(W/p).read_text(encoding='utf-8-sig',errors='replace')
 deps=set()
 for directive in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',text,re.M):
  match=re.match(r'[<"]([^>"\r\n]+)[>"]',directive)
  if not match:
   if p in finaltext:dynamic.append({'path':p,'directive':directive})
   continue
  inc=match[1].replace('\\','/');deps.update(byname.get(Path(inc).name.lower(),[]));local=(W/Path(p).parent/inc).resolve()
  try:rel=local.relative_to(W).as_posix().lower()
  except ValueError:rel=''
  if rel in lookup:deps.add(lookup[rel])
 graph[p]=sorted(deps)
reverse=defaultdict(set)
for g in [snapshot['dependencies'],graph]:
 for p,ch in g.items():
  for c in ch:reverse[c].add(p)
def closure(seeds):
 reached=set(seeds);q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 return reached
header_cons=sorted(closure(headers)&set(native));selected=sorted((closure(paths)&set(native))|set(cpp));retained=sorted(set(native)-set(selected));assert not dynamic
targets={str((W/p).resolve()).lower():p for p in headers};groups=defaultdict(list)
for row in baseline['results']:groups[str(Path(row['origin_report']).parent.parent/'actual-includes.json')].append(row['source'])
actual_hits={};traces=[]
for path,sources in groups.items():
 trace=read(path)['rows'];assert set(sources)<=set(trace)
 for source in sources:
  hit=sorted({targets[p.lower()]for p in trace[source]if p.lower()in targets})
  if hit:actual_hits[source]=hit
 traces.append({'path':path,'current_rows_checked':len(sources)});del trace
assert set(actual_hits)<=set(selected)
affected=closure(paths);names={Path(p).name.lower()for p in affected};external_edges=[]
for d,t in audit['external_inputs']['include_trees'].items():
 for rel in t['files']:
  p=Path(d)/rel
  if p.suffix.lower()not in TEXT:continue
  for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
   if Path(match[1].replace('\\','/')).name.lower()in names:external_edges.append({'source':str(p),'include':match[1]})
assert not external_edges
namedpath=H.parent/'repair-r18-intake-20261103/named-safeguard-results.json';named=read(namedpath)
impact=[{'name':r['name'],'changed_header_hits':sorted({targets[p.lower()]for p in r['actual_included_headers']if p.lower()in targets}),'rerun_required':any(p.lower()in targets for p in r['actual_included_headers'])}for r in named['rows']]
projection={'baseline':BASE,'method':'Union old/new conservative literal basename graph of all6305 projected text nodes, including inactive branches;current3616 true-origin actualinclude rows reconciled;externalwrapper edges scanned. Postcollection must independently recompute.','changed_headers':headers,'header_consumers':header_cons,'header_consumer_count':len(header_cons),'changed_native_CPP':cpp,'changed_native_CPP_count':len(cpp),'header_CPP_overlap':len(set(header_cons)&set(cpp)),'selected_sources':selected,'selected_count':len(selected),'retained_sources':retained,'retained_count':len(retained),'selected_prior_PASS':sum(old[p]['passed']for p in selected),'selected_prior_FAIL':sum(not old[p]['passed']for p in selected),'actual_changed_header_consumers':actual_hits,'actual_trace_origins':traces,'all_actual_hits_covered':True,'external_edges':external_edges,'nonliteral_changed_sites':dynamic,'native_before':3616,'native_after':3616,'text_before':6305,'text_after':6305,'added_CPP':[],'added_paths':[]}
save(H/'projected-native-inventory-and-closure.json',projection)
save(H/'native-safeguard-impact-plan.json',{'original_report':str(namedpath),'original_report_sha256':sha(namedpath),'R20_retention_proof':str(B/'native-safeguard-current-reuse.json'),'R20_retention_sha256':sha(B/'native-safeguard-current-reuse.json'),'impact':impact,'policy':'Only actual affected guards rerun;all original guards and originalGame0x9A1300/Network0x8C3600 assertions remain active. No expected value changes or bypass.'})
main='22795e60f07c044e0a6ada8c3f3e9ef3ec49c4e3';mainpaths=git('diff-tree','--no-commit-id','--name-only','-r',main).decode().splitlines()
composition={'independent_Main22795_already_in_R20':[{ 'path':p,'R20_blob':blob(BASE,p),'22795_postimage_blob':blob(main,p),'equal':blob(BASE,p)==blob(main,p)}for p in mainpaths],'note':'Originalcommit ancestry is not expected after cherry-pick. Allfour22795 postimages matchR20. TwoCPPs also touched byb856 use thoseexactpreimages;fullb856 preserves priorfourCPP recovery. Do not collect22795 or e7d again.','overlapping_paths':[{'path':p,'layers':n}for p,n in counts.items()if n>1],'chain_order':COMMITS,'preimage_issues':issues}
assert all(r['equal']for r in composition['independent_Main22795_already_in_R20'])
save(H/'ordered-composition-and-preimages.json',{'commits':candidates,'source_images':images,**composition})
luna=A/'wave17/r21-preflight-luna/header-closure.json';reconciliation={'luna_projection':str(luna),'luna_sha256':sha(luna),'method_difference':'Luna unique-basename fallback versus intake all-basename conservative union plus current-origin actualtrace proof. Use bounded larger union if difference exists.'}
l=read(luna);reconciliation.update({'Luna_count':l['finite_native_source_gate_count'],'intake_count':len(selected),'intake_only':sorted(set(selected)-set(l['finite_native_source_gate'])),'Luna_only':sorted(set(l['finite_native_source_gate'])-set(selected))});assert not reconciliation['Luna_only'];save(H/'LUNA_CLOSURE_RECONCILIATION.json',reconciliation)
plan={'state':'READ_ONLY_PREPARED_AWAIT_FINAL_REPLAY_RECEIPT_ONEBATCH_REVIEW_ROOT_RELEASE','base':BASE,'commits':COMMITS,'groups':4,'unique_paths':paths,'raw_path_endpoints':sum(counts.values()),'CPP':cpp,'headers':headers,'added_paths':added,'removed_paths':[],'inventory_before_after':{'native':[3616,3616],'text':[6305,6305],'SDK_reference_dispositions':[2,2],'broad_dispositions':[3618,3618]},'baseline_PASS':3615,'baseline_FAIL':1,'prior_PASS_required_retained':3615,'only_old_failure':'src/GameSource/Game/BrnGameModule.cpp','old_failure_gain_potential':1,'new_provider_PASS_potential':0,'composition':str(H/'ordered-composition-and-preimages.json'),'composition_sha256':sha(H/'ordered-composition-and-preimages.json'),'projection':str(H/'projected-native-inventory-and-closure.json'),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'safeguard_plan':str(H/'native-safeguard-impact-plan.json'),'safeguard_plan_sha256':sha(H/'native-safeguard-impact-plan.json'),'closure_reconciliation':str(H/'LUNA_CLOSURE_RECONCILIATION.json'),'closure_reconciliation_sha256':sha(H/'LUNA_CLOSURE_RECONCILIATION.json'),'preimage_issues':issues,'recommended_gate':{'fresh_native':len(selected),'verified_retained_native':len(retained),'SDK_refs_separate':2},'resource_proposal':{'central_jobs_max':8,'private_workers_max_when_central_active':4,'private_jobs_each':1,'central_requires_explicit_Root_release':True},'execution':['Wait final Replay exactreceipt,closedmanifest,preflight,onebatchreview,andRootcollection/buildrelease.','Collect onlyexact4listedimmutablecommits in order;verify22netpostimages23rawendpoints and no Sourceextras. Excludee7d,22795 separately,and allwave17linkrepair/R22sources.','Recomputeactualsource6305/native3616 complete conservative old/newheaderandCPPclosure;freeze tools/external9fallback flags beforefreshgate.','Fresh selectednativeonly/all9fallbacks/showIncludes/DoutputsTEMP;verify retained activeproducts/source/actualheaders/tools/externalcontext ONCE,preserve genuine origins;privateauthorobjects are not products.','Compare all3615priorPASS;only GameoldFAIL gaincandidate1,no newCPP gains. BothSDKreference dispositions stay separate.','Rerunonlyactualaffected namedguards. Keep exactoriginal Game0x9A1300/Network0x8C3600 guards active, no stub/padding fit/bypass.','If regressions pin exactsources/headerdiagnostics andholdseal for ownedrepair;no fullreplayor linkwithoutRootfollowup.','If allproofvalid seal real composite origins/actualcounts/localcleancheckpoint/terminalprocessreadback.'],'limits':['Source/layout compile evidence only;no runtime,wholeclassparity,fullgameloink orpublication.','Opaque intervals remain bounded lifetime storage with unknown private identity,notpermission to inventmembers/initialization/capacity.','Originalb856 checkpoint was partial;eligibility depends entireController→Wrapper→Replayfinalchain and review.','No linkrepair sources or SDKpolicy/ledger/layoutguard bypasses inR21.'],'parent_gitlink_index':index,'no_collection_or_build':True,'baseline_pins':pins,'created_utc':datetime.now(timezone.utc).isoformat()}
save(H/'R21_FINITE_LAYOUT_GATE_PLAN.json',plan)
summary={'base':BASE,'commits':4,'raw_path_endpoints':sum(counts.values()),'unique_paths':len(paths),'CPP':len(cpp),'headers':len(headers),'new_paths':len(added),'native_before_after':[3616,3616],'text_before_after':[6305,6305],'fresh_gate':len(selected),'retained_native':len(retained),'header_consumers':len(header_cons),'CPP_header_overlap':len(set(cpp)&set(header_cons)),'actual_changed_header_hits':len(actual_hits),'selected_prior_PASS':projection['selected_prior_PASS'],'selected_prior_FAIL':projection['selected_prior_FAIL'],'affected_guards':[r['name']for r in impact if r['rerun_required']],'preimage_issues':issues,'Luna_delta':reconciliation,'plan_sha256':sha(H/'R21_FINITE_LAYOUT_GATE_PLAN.json'),'no_collection_or_build':True};save(H/'preparation-summary.json',summary);print(json.dumps(summary,indent=2))
