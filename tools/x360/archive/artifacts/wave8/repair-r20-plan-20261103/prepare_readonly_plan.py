"""Read-only R20 exact finite-provider/Entry gate plan;no collection/build."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r19-intake-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='6be513e1b3b69f9c28ed42a01ffafdc92924f857';TEXT={'.cpp','.c','.h','.hpp','.inl','.inc','.ipp'}
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob(commit,path):
 r=subprocess.run(['git','rev-parse',commit+':'+path],cwd=W,capture_output=True,text=True);return r.stdout.strip()if r.returncode==0 else None
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip();assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip();index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R19.json')=='257cefafcea22a7366e66d3a291e0e95e64e03979ce9d9b2f0c260691b80b0de';assert sha(B/'composite-active-native.json')=='c764542ef51ff5bf123ec925110ee3b568791015be3c07ff07648e4c06f2451a';assert sha(B/'composite-artifact-audit.json')=='7b4b5ffb2d318f1acf9aad41afac25631c3d61273f562f403bc4092a441ec844'
manifest=A/'wave16/R20_READY_CANDIDATES.json';m=read(manifest);assert m['baseline']==BASE;baseline=read(B/'composite-active-native.json');old={r['source']:r for r in baseline['results']};audit=read(B/'composite-artifact-audit.json');snapshot=audit['source_snapshot'];assert len(old)==3615 and baseline['passed']==3613 and baseline['failed']==2 and len(snapshot['hashes'])==6303
images={};owners={};finaltext={};candidates=[];receipts=[];issues=[]
for group in m['groups']:
 for rel in group['receipts']:receipts.append({'group':group['name'],'path':str(A/rel),'sha256':sha(A/rel)})
 grouppaths=set()
 for c in group['commits']:
  parent=git('rev-parse',c+'^').decode().strip();assert parent=='99e26860e1a2e3071ebdeb4273dd629908a489dd';changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status in ['M','A']and Path(p).suffix.lower()in ['.cpp','.h','.hpp'];assert p not in images;initial=blob(BASE,p);expected=None if a=='0'*40 else a
   if initial!=expected:issues.append({'commit':c,'path':p,'baseline_blob':initial,'expected':expected})
   images[p]={'baseline':initial,'final':b};owners[p]=group['name'];finaltext[p]=git('show',c+':'+p).decode('utf-8-sig',errors='replace');changes.append({'path':p,'status':status,'preimage_blob':expected,'postimage_blob':b});grouppaths.add(p)
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'paths':changes})
 assert grouppaths==set(group['paths'])
paths=sorted(images);cpp=sorted(p for p in paths if p.endswith('.cpp'));headers=sorted(set(paths)-set(cpp));added=sorted(p for p,v in images.items()if v['baseline']is None);provider='src/SDKs/RenderEngineClub/MAIN/components/src/states/vertexprogramstate.cpp';shadow='src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp';entry='src/GameSource/Main/BrnMainX360.cpp';assert len(candidates)==2 and len(paths)==5 and len(cpp)==3 and len(headers)==2 and len(added)==2 and provider in added and not old[shadow]['passed']and old[entry]['passed']
futurekeys=set(snapshot['hashes'])|set(added);native=sorted(set(old)|{provider});assert len(futurekeys)==6305 and len(native)==3616
byname=defaultdict(set);lookup={p.lower():p for p in futurekeys}
for p in futurekeys:byname[Path(p).name.lower()].add(p)
graph={p:list(v)for p,v in snapshot['dependencies'].items()};dynamic=[]
# Refresh unchanged sources too for newly resolvable added header paths,using baseline graph plus explicit literal scans.
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
# Current actual include hits use true row origins and are reconciled only for the existing header.
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
# R19 retained the unchanged seven native safeguards;their actual include sources remain in R18 report.
namedpath=H.parent/'repair-r18-intake-20261103/named-safeguard-results.json';named=read(namedpath);impact=[{'name':r['name'],'changed_header_hits':sorted({targets[p.lower()]for p in r['actual_included_headers']if p.lower()in targets}),'rerun_required':any(p.lower()in targets for p in r['actual_included_headers'])}for r in named['rows']]
projection={'baseline':BASE,'method':'All6305 projected native text nodes rebuilt read-only from existing source plus immutable final blobs;newheader name resolution included. Union old/new conservative literal basename graph,all inactive branches. Current3615actualorigintraces checked;externalwrappers checked. Postcollectionmustrecompute.','header_consumers':header_cons,'header_consumer_count':len(header_cons),'selected_sources':selected,'selected_count':len(selected),'retained_count':len(retained),'selected_prior_PASS':sum(old[p]['passed']for p in selected if p in old),'selected_prior_FAIL':sum(not old[p]['passed']for p in selected if p in old),'selected_new_provider':1,'actual_existing_header_consumers':actual_hits,'actual_trace_origins':traces,'all_actual_hits_covered':True,'external_edges':external_edges,'new_nonliteral_candidate_sites':dynamic,'native_before':3615,'native_after':3616,'native_text_before':6303,'native_text_after':6305,'new_provider_CPP':provider,'new_header':next(p for p in added if p.endswith('.h'))};save(H/'projected-native-inventory-and-closure.json',projection);save(H/'native-safeguard-impact-plan.json',{'original_report':str(namedpath),'original_report_sha256':sha(namedpath),'R19_retention_proof':str(B/'native-safeguard-current-reuse.json'),'R19_retention_proof_sha256':sha(B/'native-safeguard-current-reuse.json'),'impact':impact})
refs=read(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R19.json')['SDK_reference_records'];assert len(refs)==2 and all(r['status']=='SDK_PROVIDED_REFERENCE'and not r['compiler_gain']for r in refs)
accounting={'baseline_native':{'PASS':3613,'FAIL':2,'count':3615},'preserved_SDK_references':2,'baseline_broader_dispositions':3617,'future_native_count':3616,'future_broader_dispositions':3618,'old_failure_repair_candidate':{'source':shadow,'baseline_passed':False,'compile_gain_if_fresh_PASS':1},'old_PASS_controlflow_correction':{'source':entry,'baseline_passed':True,'compile_gain_if_fresh_PASS':0},'new_provider':{'source':provider,'baseline_native_row_exists':False,'new_native_PASS_if_fresh_PASS':1,'old_failure_repair':False},'new_provider_contract':'0x28 minimum closed-use NoBind resource payload;opaque Resource ownership. Original fullsizeof/alignment unclaimed. Neverinstantiateinterface bysizeof or inventprefix fields.','narrow_author_section_proofs':'binder24/getter12;IndexBuffer48retained;DeviceInitialize0x74nonrelocbitsmatch;ResourceCRT5zero stores with redundantnative lane0zero unreplicated. No originalwholeclass/compilerflags/globalmatchingclaim.','SDK_reference_records':refs,'R19_provider_policy_inventory_note':'tools/x360_sdk_providers.json records historical R19 relative3617to3615 disposition;do not reinterpret as fixed current denominator after R20 adds1provider. Policy staysunchanged.','expected_if_only_intended_gains':'native3615PASS1FAIL/3616 plus2SDKreferences;1oldfailuregain+1newproviderPASS;Entryisnotagainingoldfailure. Notpromiseduntilintegrationgate.'};save(H/'source-provider-accounting-plan.json',accounting)
key=Path(provider).stem+'-'+hashlib.sha256(provider.lower().encode()).hexdigest()[:8];route={'provider':provider,'eligible_native_collector':True,'route':'Tracked new .cpp under src/SDKs/.../states automatically selected bybuild_x360.py --all;reference/SDK copies remainexcluded. No PC CMake oridentity/ledger mount change.','expected_object':key+'.obj','expected_listing':key+'.asm','native_count_before':3615,'native_count_after':3616,'text_before':6303,'text_after':6305,'compile_only':True,'game_link_not_run':True};save(H/'new-provider-native-route.json',route)
plan={'state':'READ_ONLY_PREPARED_AWAIT_METADATA_REVIEW_ROOT_RELEASE','base':BASE,'manifest':str(manifest),'manifest_sha256':sha(manifest),'commits':[c['commit']for c in candidates],'candidates':candidates,'receipts':receipts,'paths':paths,'planned_source_images':images,'CPP':cpp,'headers':headers,'added_paths':added,'removed_paths':[],'unexpected_preimage_issues':issues,'baseline_native_PASS':3613,'baseline_native_FAIL':2,'baseline_native_count':3615,'preserved_SDK_reference_units':2,'future_native_count':3616,'baseline_broad_dispositions':3617,'future_broad_dispositions':3618,'native_text_before':6303,'native_text_after':6305,'projection':str(H/'projected-native-inventory-and-closure.json'),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'accounting':str(H/'source-provider-accounting-plan.json'),'accounting_sha256':sha(H/'source-provider-accounting-plan.json'),'provider_route':str(H/'new-provider-native-route.json'),'provider_route_sha256':sha(H/'new-provider-native-route.json'),'safeguard_plan':str(H/'native-safeguard-impact-plan.json'),'safeguard_plan_sha256':sha(H/'native-safeguard-impact-plan.json'),'recommended_gate':'FINITE_NATIVE_'+str(len(selected))+'_PLUS_'+str(len(retained))+'_VERIFIED_UNAFFECTED_PLUS_2_SDK_REFERENCES','resource_proposal':{'central_jobs_max':8,'private_workers_max':4,'private_jobs_per_worker':1,'aggregate_jobs_max':12,'launch_requires_Root_release':True},'execution':['Wait finalmetadata/onebatchreview/explicitRootrelease;no collection/build fromthisplan.','Normalcherry-pick exactVertex301926 thenEntryb142;bothauthorparent99e,current6bepreimagesverified. Verifyall5postimages/twoaddedfiles and no extraSource/layoutpartials.','Recomputeactualnative3616/text6305 andold/newcompleteheader/CPPclosure aftercollection;refresh allnewheadername edges. Freshall3CPPs/allheaderconsumers,no authorobjects asfresh.','Canonical9fallbacks/showIncludes/D-only outputsTEMP underRootbudget. Verify untouched active-native products/context once;preserveactualpriororigins. Retain2SDKreference dispositionsunchanged andseparate fromnative gains.','Compareall3613priorPASS;ShadowingoldFAILrepair1,EntryoldPASSbehaviorcorrection0gain,newproviderPASS1 separately. Native+broad denominators explicit,no silentdispositionchanges.','Runonly materiallyaffected nativeguardcompiles;retainedones needinputproof. Recordrealregressions andholdsealfornarrowrepairs withoutpadding/fakefields/bypass.','Pin source-backedminimumNoBindcontract andnarrow24/12nativefunction proofs asboundedauthor/integration evidence;no originalfullsizeof/alignment orwholeprogrambyteparity claim.','No ICE/Main/Replay typedpartial/layoutrecoveries,gameLinkpolicy/probechanges,fullreplayunlessunboundedandRootfollowup,CRT/runtime/launch/publication.'], 'excluded':m['excluded'],'limits':m['limits'],'parent_gitlink_index':index,'no_collection_build_link_or_patch_dryrun':True,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R20_FINITE_INTAKE_GATE_PLAN.json',plan)
config={'base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R20_FINITE_INTAKE_GATE_PLAN.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R19.json'),'baseline_report':str(B/'composite-active-native.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native116-command.json'),'baseline_guard_retention':str(B/'native-safeguard-current-reuse.json'),'baseline_named_gates':str(namedpath),'baseline_SDK_provider_pins':str(B/'SDK-provider-reference-pins-after.json')}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_guard_retention','baseline_named_gates','baseline_SDK_provider_pins']:config[k+'_sha256']=sha(config[k])
save(H/'campaign-preparation.json',config)
summary={'state':plan['state'],'base':BASE,'commits':2,'groups':2,'paths':5,'CPP':3,'headers':2,'added_paths':added,'new_provider':provider,'future_native':3616,'future_text':6305,'SDK_refs':2,'future_broad_dispositions':3618,'header_native_consumers':len(header_cons),'fresh_gate':len(selected),'retained_native':len(retained),'selected_prior_PASS':projection['selected_prior_PASS'],'selected_prior_FAIL':projection['selected_prior_FAIL'],'selected_new_provider':1,'actual_existing_header_hits':len(actual_hits),'affected_guards':[r['name']for r in impact if r['rerun_required']],'issues':issues,'plan_sha256':sha(H/'R20_FINITE_INTAKE_GATE_PLAN.json'),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'accounting_sha256':sha(H/'source-provider-accounting-plan.json'),'source_mutated':False,'compiler_started':False};save(H/'preparation-summary.json',summary);print(json.dumps(summary,indent=2))
