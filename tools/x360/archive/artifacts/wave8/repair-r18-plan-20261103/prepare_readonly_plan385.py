"""Read-only exact R18 finite native gate planning;no collection,build or old-job restart."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r17-lifecycle-repair-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='8741b36e43dfe0d899dc442533f54131d174b393'
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip();assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip()
index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
assert sha(B/'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R17.json')=='7631d7237ee05236a15753f0f5302dfcfd0c4a33f612adadb0cdeec8ed343555';assert sha(B/'composite-effective.json')=='78c919e6ea4256df4c55f571d5c64f55348a02827a53beb15eabb57b4384a32c';assert sha(B/'composite-artifact-audit.json')=='743b23ca7039ddba8ff31ae64f01b553843aa84ce71ce31a8957ac1aa27b8f0d'
manifest=A/'wave14/R18_READY_CANDIDATES.json';m=read(manifest);assert m['baseline']==BASE;pre=A/'wave14/r18-preflight-luna/FINAL.json';assert sha(pre)=='5b8697e7fb998da8735bf91df20e9a1d8a3b6237cbfb9ace1ae3d30bb88cf9b2';pf=read(pre);review=A/'wave14/r18-fresh-review-terra/REVIEW.md';assert 'VERDICT: pass'in review.read_text()
audit=read(B/'composite-artifact-audit.json');snapshot=audit['source_snapshot'];baseline=read(B/'composite-effective.json');old={r['source']:r for r in baseline['results']};assert len(old)==3617 and baseline['passed']==3464 and baseline['failed']==153 and len(snapshot['hashes'])==6307
images={};owners={};finaltext={};candidates=[];receipts=[];issues=[]
for group in m['groups']:
 receipt=Path(group['receipt']);receipts.append({'group':group['name'],'path':str(receipt),'sha256':sha(receipt)})
 grouppaths=set()
 for c in group['commits']:
  parent=git('rev-parse',c+'^').decode().strip();changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status=='M'and Path(p).suffix.lower()in ['.cpp','.h','.hpp']and p.startswith(('src/','vendor/'))and 'pc'not in [part.lower()for part in Path(p).parts]
   if p in images:
    assert owners[p]==group['name']
    if images[p]['final']!=a:issues.append({'commit':c,'path':p,'projected':images[p]['final'],'expected':a})
   else:
    actual=git('rev-parse',BASE+':'+p).decode().strip()
    if actual!=a:issues.append({'commit':c,'path':p,'baseline':actual,'expected':a})
    images[p]={'baseline':actual,'final':actual};owners[p]=group['name']
   images[p]['final']=b;finaltext[p]=git('show',c+':'+p).decode('utf-8-sig',errors='replace');changes.append({'path':p,'status':status,'preimage_blob':a,'postimage_blob':b});grouppaths.add(p)
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'paths':changes})
 assert grouppaths==set(group['paths'])
paths=sorted(images);cpp=sorted(p for p in paths if p.endswith('.cpp'));headers=sorted(set(paths)-set(cpp));commits=[c['commit']for c in candidates];assert len(candidates)==18 and len(m['groups'])==16 and len(paths)==91 and len(cpp)==82 and len(headers)==9
assert '22795e60f07c044e0a6ada8c3f3e9ef3ec49c4e3'in commits and next(c['parent']for c in candidates if c['commit']=='22795e60f07c044e0a6ada8c3f3e9ef3ec49c4e3')==BASE
assert git('rev-parse','6b50aa8eef0354de2970d3e0beb7392a8548966c^').decode().strip()=='19e810c11dec1a1d2a09ad0a4f1d83c12b980a03';assert commits.index('19e810c11dec1a1d2a09ad0a4f1d83c12b980a03')<commits.index('6b50aa8eef0354de2970d3e0beb7392a8548966c')
assert not any(Path(p).name.lower()=='shadowingdevice.cpp'for p in paths);assert not any(c.startswith(('e7d','b856','093fc91'))for c in commits)
byname=defaultdict(set);lookup={p.lower():p for p in snapshot['hashes']}
for p in snapshot['hashes']:byname[Path(p).name.lower()].add(p)
graph={p:list(ch)for p,ch in snapshot['dependencies'].items()};dynamic=[]
for p,text in finaltext.items():
 deps=set()
 for directive in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',text,re.M):
  match=re.match(r'[<"]([^>"\r\n]+)[>"]',directive)
  if not match:dynamic.append({'path':p,'directive':directive});continue
  inc=match[1].replace('\\','/');deps.update(byname.get(Path(inc).name.lower(),[]));local=(W/Path(p).parent/inc).resolve()
  try:rel=local.relative_to(W).as_posix().lower()
  except ValueError:rel=''
  if rel in lookup:deps.add(lookup[rel])
 graph[p]=sorted(deps)
reverse=defaultdict(set)
for g in [snapshot['dependencies'],graph]:
 for p,ch in g.items():
  for target in ch:reverse[target].add(p)
def closure(seeds):
 reached=set(seeds);q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 return reached
native=set(old);hcons=sorted(closure(headers)&native);selected=sorted((closure(paths)&native)|set(cpp));retained=sorted(native-set(selected));overlap=sorted(set(cpp)&set(hcons));perheader=[{'header':h,'native_consumers':len(closure([h])&native)}for h in headers]
assert len(hcons)==337 and len(selected)==385 and len(overlap)==34 and len(retained)==3232 and not dynamic;assert pf['header_gate_plan']['finite_cpp_gate_union']==373
stale=pf['cross_context_gaps']['affected_cpp'];assert len(stale)==5 and set(stale)<=set(selected)
# Reconcile retained actual include traces by the true origin report of each current row;no product rehash.
targets={str((W/h).resolve()).lower():h for h in headers};origin_groups=defaultdict(list)
for row in baseline['results']:origin_groups[str(Path(row['origin_report']).parent.parent/'actual-includes.json')].append(row['source'])
actual_hits={};traceproof=[]
for tracepath,sources in origin_groups.items():
 trace=read(tracepath)['rows'];assert set(sources)<=set(trace)
 for source in sources:
  hit=sorted({targets[p.lower()]for p in trace[source]if p.lower()in targets})
  if hit:actual_hits[source]=hit
 traceproof.append({'path':tracepath,'current_rows_reconciled':len(sources),'changed_header_consumer_rows':sum(source in actual_hits for source in sources)});del trace
assert set(actual_hits)<=set(selected)
# No external wrappers may introduce an additional path into the changed-header dependency closure.
affected=closure(paths);affectednames={Path(p).name.lower()for p in affected};external=audit['external_inputs'];external_edges=[]
for directory,tree in external['include_trees'].items():
 for rel in tree['files']:
  p=Path(directory)/rel
  if p.suffix.lower()not in ['.cpp','.c','.h','.hpp','.inl','.inc','.ipp']:continue
  for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
   inc=match[1].replace('\\','/')
   if Path(inc).name.lower()in affectednames:external_edges.append({'source':str(p),'include':inc})
assert not external_edges
# Determine exact existing safeguards impacted from actual include traces;preserve true origin on later reuse.
namedpath=H.parent/'repair-r17-intake-20261103/named-safeguard-results.json';named=read(namedpath);impact=[{'name':r['name'],'affected_headers':sorted({targets[p.lower()]for p in r['actual_included_headers']if p.lower()in targets}),'rerun_required':any(p.lower()in targets for p in r['actual_included_headers'])}for r in named['rows']]
# Exact conservative dependency witnesses for every selected source replace any unsaved prior-estimate delta.
nextnode={h:None for h in headers};queue=deque(headers)
while queue:
 for parent in reverse[queue.popleft()]:
  if parent not in nextnode:
   # Find a reached child;all reverse graph edges are preserved old/post literal possibilities.
   child=next((c for c in set(snapshot['dependencies'].get(parent,[]))|set(graph.get(parent,[]))if c in nextnode),None)
   if child is not None:nextnode[parent]=child;queue.append(parent)
witnesses={}
for source in selected:
 chain=[];node=source;visited=set()
 while node in nextnode and node not in visited:
  visited.add(node);chain.append(node);node=nextnode[node]
  if node is None:break
 witnesses[source]={'changed_CPP':source in cpp,'conservative_header_dependency_path':chain,'path_scope':'All basename/inactive literal edges;actual hits separately recorded.'}
save(H/'exact385-path-coverage-witnesses.json',{'selected_count':385,'header_consumer_count':337,'sources':witnesses,'original373_path_array_available':False,'exact12_omission_list_claimed':False,'limitation':'Original preflight373 saved aggregate counts only;no exact prior373 membership exists to subtract. This file pins all authoritative385 concrete native paths and their reasons.'})
facts={'method':'Read-only union of sealed baseline dependencies and immutable final candidate literal includes;all basename matches/inactive branches retained. Native inventory filter excludespc. Reconciled all3617currentorigin actualinclude rows plus external literal wrappers. Must recompute on actualpostcollection tree.','header_consumers':hcons,'header_consumer_count':337,'candidate_CPP_count':82,'candidate_header_overlap_count':34,'selected_sources':selected,'selected_count':385,'retained_count':3232,'per_header':perheader,'actual_changed_header_consumer_rows':actual_hits,'actual_trace_origins':traceproof,'all_current_actual_header_hits_within_selection':True,'external_literal_edges_into_affected_nodes':external_edges,'new_nonliteral_candidate_include_sites':dynamic,'five_stale_private_Device_contexts':stale,'all_stale_contexts_selected':True,'selected_prior_PASS':sum(old[p]['passed']for p in selected),'selected_prior_FAIL':sum(not old[p]['passed']for p in selected),'candidate82_prior_PASS':sum(old[p]['passed']for p in cpp),'candidate82_prior_FAIL':sum(not old[p]['passed']for p in cpp),'native_inventory':3617,'text_inputs':6307,'new_or_removed_files':[]};save(H/'projected-header-consumer-plan.json',facts);save(H/'native-safeguard-impact-plan.json',{'baseline_named_report':str(namedpath),'baseline_named_report_sha256':sha(namedpath),'impact':impact,'actual_affected_gate_count':sum(r['rerun_required']for r in impact),'retained_unaffected_gate_count':sum(not r['rerun_required']for r in impact)})
plan={'state':'READ_ONLY_PREPARED_AWAITING_ROOT_RELEASE','base':BASE,'manifest':str(manifest),'manifest_sha256':sha(manifest),'commits':commits,'candidates':candidates,'receipts':receipts,'paths':paths,'planned_source_images':images,'changed_CPP':cpp,'changed_headers':headers,'new_paths':[],'removed_paths':[],'native_inventory':3617,'text_inputs':6307,'baseline_passed':3464,'baseline_failed':153,'unexpected_preimage_issues':issues,'preflight':str(pre),'preflight_sha256':sha(pre),'review':str(review),'review_sha256':sha(review),'preflight_count_reconciliation':{'original_header_count':325,'original_gate_estimate':373,'independent_conservative_headers':337,'independent_gate':385,'more_compile_attempts_than_original_estimate':12,'exact_original_373_path_set_unavailable':True,'all385_path_witnesses':str(H/'exact385-path-coverage-witnesses.json'),'no_fabricated12_path_omission_list':True,'Root_accepts_conservative385_pending_final_release':True},'recommended_gate':'FINITE_FRESH_NATIVE_385_PLUS3232_VERIFIED_UNAFFECTED','projection':str(H/'projected-header-consumer-plan.json'),'projection_sha256':sha(H/'projected-header-consumer-plan.json'),'safeguard_impact':str(H/'native-safeguard-impact-plan.json'),'safeguard_impact_sha256':sha(H/'native-safeguard-impact-plan.json'),'source_order_notes':['OnlyMain22795fourCPP at8741base;excludee7d5CPP/b856partial andunfixedBrnGameModule layoutguard.','Replay19e810 precedes6b50;6b50parentverified19e;exclude093ABIwrong.','Graphicsca951+c131 include4assigned plus2existingnativeproviderCPP(device.cpp,indexbuffer.cpp),notshadowingdevice.cpp.','Allgroupsdisjoint;privateauthorcontextnotfinalproductevidence.'],'resource_proposal':{'central_native_jobs_max':8,'private_workers_max':4,'private_jobs_per_worker':1,'aggregate_native_jobs_max':12,'launch_requires_Root_release':True},'execution':['WaitexplicitRootrelease;do notcollect/build fromthisplanning.','Sequentialnormalcherry-picks ofexact18originals,verifyrawscope/preimages/postimages percommit,stopunexpectedscope/conflicts.','Freezeall91postimages,recompute6307text/native3617/noinventorychange andconservativeold/newclosure. Exact385mustinclude82candidateCPP+all337conservativeheaderconsumers (34overlap),allactualretainedincludehitsand5staleDevicecontexts.','Fresh385native/all9canonicalfallbacks/showIncludes/D-onlyoutputsTEMP underRootresourcecap. No authorproducts asfresh.','Verifyactual3232reusedR17products/input/header/tool/external/submodule/flagcontext once;preserveperrowR17/R16/R15origins. Do notfullreplayunlessclosureunbounded andRootreview.','Compareall3464priorpasses,genuineoldFAILgains,knownoldfailedconsumers. Allcodedguardsstay;recordactualnewregressions/timeouts andholdsealfornarrowfixes.','Rerunonlymateriallyaffectednative/layoutsafeguards peractualtrace;retainothersevidencehonestly.','No link/CRT/runtime/broadnumeric/byte-parity/image/launch/publicationclaim. AuthorIndexBuffer48-byteproofremainsnarrowauthorclaimunlessseparatelyrebound;nooverallmatchingpercent.'], 'excluded':m['excluded'],'limits':m['limits'],'parent_gitlink_index':index,'no_collection_build_patchapply_or_dryrun':True,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R18_FINITE_INTAKE_GATE_PLAN.json',plan)
config={'base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R18_FINITE_INTAKE_GATE_PLAN.json'),'baseline_seal':str(B/'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R17.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'command.json'),'baseline_named_gates':str(namedpath)}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:config[k+'_sha256']=sha(config[k])
save(H/'campaign-preparation.json',config)
summary={'state':plan['state'],'base':BASE,'commits':18,'groups':16,'paths':91,'CPP':82,'headers':9,'header_native_consumers':337,'CPP_header_overlap':34,'fresh_gate':385,'retained':3232,'selected_prior_PASS':facts['selected_prior_PASS'],'selected_prior_FAIL':facts['selected_prior_FAIL'],'candidate82_prior_PASS':facts['candidate82_prior_PASS'],'candidate82_prior_FAIL':facts['candidate82_prior_FAIL'],'actual_prior_include_consumers':len(actual_hits),'all_actual_include_consumers_selected':True,'staleDevicecontexts':5,'affected_named_gates':[r['name']for r in impact if r['rerun_required']],'native_inventory':3617,'text_inputs':6307,'issues':issues,'plan_sha256':sha(H/'R18_FINITE_INTAKE_GATE_PLAN.json'),'projection_sha256':sha(H/'projected-header-consumer-plan.json'),'source_mutated':False,'compiler_started':False};save(H/'preparation-summary.json',summary);print(json.dumps(summary,indent=2))
