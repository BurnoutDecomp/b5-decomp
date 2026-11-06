"""Read-only R19 exact source/move/provider-disposition and finite native gate plan."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r18-intake-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='99e26860e1a2e3071ebdeb4273dd629908a489dd';TEXT={'.cpp','.c','.h','.hpp','.inl','.inc','.ipp'}
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob_at(commit,path):
 r=subprocess.run(['git','rev-parse',commit+':'+path],cwd=W,capture_output=True,text=True);return r.stdout.strip()if r.returncode==0 else None
def eligible(p):return p.startswith(('src/','vendor/'))and 'pc'not in [part.lower()for part in Path(p).parts]
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip();assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip();index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R18.json')=='aefdbddd349a293f23cae872056c4e0c8ff7586bcef9781a8dec294b2fe21251';assert sha(B/'composite-effective.json')=='0f4f90f99d6752d16a5765071c66874007fadb6fdd4257ceaac59ef4f20bd20a';assert sha(B/'composite-artifact-audit.json')=='49a7a40110aa70eaefb21631f18f7f51a60f42024765ba077c9c9c0d666ecc80'
manifest=A/'wave15/R19_READY_CANDIDATES.json';m=read(manifest);assert m['baseline']==BASE;audit=read(B/'composite-artifact-audit.json');snapshot=audit['source_snapshot'];baseline=read(B/'composite-effective.json');old={r['source']:r for r in baseline['results']};assert len(old)==3617 and baseline['passed']==3544 and baseline['failed']==73 and len(snapshot['hashes'])==6307
images={};owners={};finaltext={};candidates=[];receipts=[];issues=[];renames=[];canonical_paths=set()
def ensure(path,group):
 if path not in images:images[path]={'baseline':blob_at(BASE,path),'final':blob_at(BASE,path)};owners[path]=group
 else:assert owners[path]==group
for group in m['groups']:
 for rel in group['receipts']:receipts.append({'group':group['name'],'path':rel,'sha256':sha(rel)})
 groupcanonical=set()
 for c in group['commits']:
  parent=git('rev-parse',c+'^').decode().strip();changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r','-M',c).decode().splitlines():
   meta,*paths=line.split('\t');_,_,a,b,status=meta.split()
   if status=='R100':
    assert len(paths)==2 and a==b;src,dst=paths;assert src.startswith('src/SDKs/D3D/')and dst.startswith('reference/x360-sdk/D3D/');ensure(src,group['name']);ensure(dst,group['name'])
    if images[src]['final']!=a:issues.append({'commit':c,'path':src,'projected':images[src]['final'],'expected':a})
    if images[dst]['final']is not None:issues.append({'commit':c,'path':dst,'projected':images[dst]['final'],'expected':None})
    images[src]['final']=None;images[dst]['final']=b;finaltext[dst]=git('show',c+':'+dst).decode('utf-8-sig',errors='replace');renames.append({'commit':c,'source':src,'destination':dst,'source_blob':a,'destination_blob':b,'byte_identical_R100':True});changes.append({'status':status,'source':src,'destination':dst,'preimage_blob':a,'postimage_blob':b});groupcanonical.add(dst);canonical_paths.add(dst)
   else:
    assert status in ['M','A']and len(paths)==1;path=paths[0];ensure(path,group['name']);expected=None if a=='0'*40 else a
    if images[path]['final']!=expected:issues.append({'commit':c,'path':path,'projected':images[path]['final'],'expected':expected})
    images[path]['final']=b;finaltext[path]=git('show',c+':'+path).decode('utf-8-sig',errors='replace');changes.append({'status':status,'path':path,'preimage_blob':expected,'postimage_blob':b});groupcanonical.add(path);canonical_paths.add(path)
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'changes':changes})
 assert groupcanonical==set(group['paths'])
commits=[c['commit']for c in candidates];assert len(commits)==16 and len(m['groups'])==14 and not any(c.startswith(('0d9a0653f','e506a05'))for c in commits);assert commits[-3:]==['3e4cc2042a8a825337980a5c0a502d4a07fc738b','7cfe919c7f58307e4eadf3303b22ea750a250ad3','2af72c111f7fd42de581f182db7b15352b799a5a']
assert len(canonical_paths)==87 and len([p for p in canonical_paths if p.endswith('.cpp')])==74 and len([p for p in canonical_paths if Path(p).suffix.lower()in ['.h','.hpp']])==11 and len(renames)==4
allpaths=sorted(images);removed=sorted(p for p,v in images.items()if v['final']is None);added=sorted(p for p,v in images.items()if v['baseline']is None);modified=sorted(p for p,v in images.items()if v['baseline']is not None and v['final']is not None and v['baseline']!=v['final']);assert len(allpaths)==91 and len(removed)==4 and len(added)==6 and len(modified)==81
removed_cpp=sorted(p for p in removed if p.endswith('.cpp'));assert removed_cpp==['src/SDKs/D3D/D3DBlocker.cpp','src/SDKs/D3D/GPUTimingMarkerSmall.cpp'];assert all(not old[p]['passed']for p in removed_cpp)
futurekeys=set(snapshot['hashes'])-{p for p in removed if p in snapshot['hashes']};futurekeys|={p for p in added if eligible(p)and Path(p).suffix.lower()in TEXT};future_native=sorted((set(old)-set(removed_cpp))|{p for p in added if eligible(p)and Path(p).suffix.lower()in ['.c','.cpp']});assert len(futurekeys)==6303 and len(future_native)==3615
active_cpp=sorted(p for p in modified if eligible(p)and p.endswith('.cpp'));active_headers=sorted(p for p in modified if eligible(p)and Path(p).suffix.lower()in ['.h','.hpp']);removed_headers=sorted(p for p in removed if Path(p).suffix.lower()in ['.h','.hpp']);assert len(active_cpp)==72 and len(active_headers)==9 and len(removed_headers)==2
# Project all final native headers/sources and preserve old reverse edges for moved-out headers/providers.
byname=defaultdict(set);lookup={p.lower():p for p in futurekeys}
for p in futurekeys:byname[Path(p).name.lower()].add(p)
graph={p:[ch for ch in children if ch in futurekeys]for p,children in snapshot['dependencies'].items()if p in futurekeys};dynamic=[]
for p,text in finaltext.items():
 if p not in futurekeys:continue
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
 for p,children in g.items():
  for child in children:reverse[child].add(p)
def closure(seeds):
 reached=set(seeds);q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 return reached
header_seeds=active_headers+removed_headers;changed_native_inputs=sorted(p for p in allpaths if eligible(p)and Path(p).suffix.lower()in TEXT);header_cons=sorted(closure(header_seeds)&set(future_native));selected=sorted((closure(changed_native_inputs)&set(future_native))|set(active_cpp));retained=sorted(set(future_native)-set(selected));assert not dynamic
# Actual prior include coverage follows each current row's real origin,including rows becoming SDKreferences.
targets={str((W/p).resolve()).lower():p for p in header_seeds};origin_groups=defaultdict(list)
for row in baseline['results']:origin_groups[str(Path(row['origin_report']).parent.parent/'actual-includes.json')].append(row['source'])
actual_hits={};traceproof=[]
for tracepath,sources in origin_groups.items():
 trace=read(tracepath)['rows'];assert set(sources)<=set(trace)
 for source in sources:
  hit=sorted({targets[p.lower()]for p in trace[source]if p.lower()in targets})
  if hit:actual_hits[source]=hit
 traceproof.append({'path':tracepath,'current_rows_checked':len(sources)});del trace
active_actual={p:v for p,v in actual_hits.items()if p in future_native};assert set(active_actual)<=set(selected)
affected=closure(changed_native_inputs);affectednames={Path(p).name.lower()for p in affected};external_edges=[]
for directory,tree in audit['external_inputs']['include_trees'].items():
 for rel in tree['files']:
  p=Path(directory)/rel
  if p.suffix.lower()not in TEXT:continue
  for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
   inc=match[1].replace('\\','/')
   if Path(inc).name.lower()in affectednames:external_edges.append({'source':str(p),'include':inc})
assert not external_edges
namedpath=B/'named-safeguard-results.json';named=read(namedpath);impact=[{'name':r['name'],'affected_headers':sorted({targets[p.lower()]for p in r['actual_included_headers']if p.lower()in targets}),'rerun_required':any(p.lower()in targets for p in r['actual_included_headers'])}for r in named['rows']]
projection={'baseline':BASE,'method':'Conservative union old and immutable-final native include graphs;old removed-header edges retained. Actualcurrent3617-origin include paths reconciled. Native collector excludes reference/CPPs,not silently counted PASS. Must recompute after release.','active_modified_CPP':active_cpp,'active_modified_headers':active_headers,'removed_native_headers':removed_headers,'header_seed_count':11,'header_native_consumers':header_cons,'header_consumer_count':len(header_cons),'selected_sources':selected,'selected_count':len(selected),'retained_count':len(retained),'selected_prior_PASS':sum(old[p]['passed']for p in selected),'selected_prior_FAIL':sum(not old[p]['passed']for p in selected),'active_candidate72_prior_PASS':sum(old[p]['passed']for p in active_cpp),'active_candidate72_prior_FAIL':sum(not old[p]['passed']for p in active_cpp),'future_native_inventory':future_native,'native_before':3617,'native_after':3615,'native_text_inputs_before':6307,'native_text_inputs_after':6303,'actual_current_changed_header_consumers':actual_hits,'active_actual_consumers_all_selected':True,'actual_trace_origins':traceproof,'external_literal_edges_into_affected_nodes':external_edges,'new_nonliteral_candidate_sites':dynamic};save(H/'projected-native-inventory-and-closure.json',projection);save(H/'safeguard-impact-plan.json',{'report':str(namedpath),'sha256':sha(namedpath),'impact':impact})
policy_path='tools/x360_sdk_providers.json';policy=json.loads(finaltext[policy_path]);assert policy['inventory_change']['previous_eligible_sources']==3617 and policy['inventory_change']['new_eligible_sources']==3615 and policy['inventory_change']['compile_gains']==0
assert {d['former_source']for d in policy['inventory_change']['dispositions']}==set(removed_cpp)
sdk=Path(baseline['sdk']);archive=sdk/policy['provider']['archive'];assert sha(archive)==policy['provider']['archive_sha256'];sdk_deps=[]
for lib in policy['sdk_only_link_libraries']:
 path=sdk/lib['path'];sdk_deps.append({'path':str(path),'expected_sha256':lib['sha256'],'exists':path.is_file()})
coverage=[]
for d in policy['inventory_change']['dispositions']:
 src=d['former_source'];dest=d['reference'];move=next(x for x in renames if x['source']==src);coverage.append({'former_native_source':src,'baseline_passed':old[src]['passed'],'baseline_log':old[src]['log'],'status':'SDK_PROVIDED_REFERENCE','reference':dest,'reference_postimage_blob':images[dest]['final'],'move_blob_identity':move['byte_identical_R100'],'provider_archive':str(archive),'provider_archive_sha256':policy['provider']['archive_sha256'],'compiler_gain':False,'native_compile_PASS':False})
providerproof={'selected_policy_path':policy_path,'selected_policy_blob':images[policy_path]['final'],'selected_policy_content_sha256':hashlib.sha256(git('show',commits[-1]+':'+policy_path)).hexdigest(),'coverage_records':coverage,'SDK_reference_units':2,'moved_reference_files':renames,'native_collector_change':'build_x360.py --all selects tracked src/vendor C/C++;reference/x360-sdk/D3D is excluded by prefix. No new blacklist or hidden source filtering.','native_before':3617,'native_after':3615,'broader_disposition_denominator':3617,'compiler_gain_from_reclassification':0,'old_failure_reclassification':2,'remaining_active_baseline_PASS':3544,'remaining_active_baseline_FAIL':71,'selected_provider':policy['provider'],'approved_proof':policy['proof'],'SDK_only_dependencies':sdk_deps,'runtime_requirements':policy['runtime_requirements'],'game_wide_provider_policy_selected':False,'full_game_link_validated':False,'new_link_or_runtime_test_performed':False,'CAP_xbdm_explicit':True};save(H/'SDK-provider-coverage-and-accounting.json',providerproof)
# Actual R18 prerequisites replace private preparation copies.
r18=read(B/'intake-receipts.json')['receipts'];prior={r['source_commit']:r for r in r18};prereqs=[]
for original in ['c13197f46916b2a0c96822125724b3db34d7fe1f','476ee815d3bcff53e79fbff41f754013d1e0b274']:
 integrated=prior[original]['integrated_commit'];assert subprocess.run(['git','merge-base','--is-ancestor',integrated,BASE],cwd=W).returncode==0;prereqs.append({'original_R18':original,'integrated':integrated,'already_in_baseline':True,'do_not_recollect':True})
remaining_ungated_baseline_failures=sorted(p for p,r in old.items()if not r['passed']and p not in selected and p not in removed_cpp)
plan={'state':'READ_ONLY_PREPARED_AWAITING_METADATA_REVIEW_ROOT_RELEASE','base':BASE,'manifest':str(manifest),'manifest_sha256':sha(manifest),'commits':commits,'candidates':candidates,'receipts':receipts,'manifest_canonical_paths':sorted(canonical_paths),'all_affected_path_endpoints':allpaths,'planned_source_images':images,'modified_paths':modified,'removed_source_paths':removed,'added_reference_policy_paths':added,'active_modified_CPP':active_cpp,'active_modified_headers':active_headers,'moves':renames,'raw_manifest_path_count':87,'all_affected_path_endpoint_count':91,'raw_manifest_CPP_destinations':74,'raw_manifest_headers':11,'active_CPP_gate_endpoints':72,'SDK_covered_reference_CPP_units':2,'projection':str(H/'projected-native-inventory-and-closure.json'),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'SDK_coverage':str(H/'SDK-provider-coverage-and-accounting.json'),'SDK_coverage_sha256':sha(H/'SDK-provider-coverage-and-accounting.json'),'safeguard_plan':str(H/'safeguard-impact-plan.json'),'safeguard_plan_sha256':sha(H/'safeguard-impact-plan.json'),'unexpected_preimage_issues':issues,'recommended_gate':'FINITE_NATIVE_'+str(len(selected))+'_PLUS_'+str(len(retained))+'_VERIFIED_UNAFFECTED_PLUS_2_SDK_REFERENCE_RECORDS','baseline_native':3617,'future_native':3615,'baseline_text':6307,'future_native_text':6303,'baseline_PASS':3544,'baseline_FAIL':73,'preserved_broader_disposition_denominator':3617,'SDK_coverage_records':coverage,'native_repair_gains_expected_not_promised':69,'remaining_ungated_baseline_failures':remaining_ungated_baseline_failures,'R18_prerequisites_already_present':prereqs,'excluded_private_preparations':['0d9a0653f','e506a05f9854217e35f9f703bef577c3c3de17a4'],'resource_proposal':{'central_jobs_max':8,'private_workers_max':4,'private_jobs_per_worker':1,'aggregate_jobs_max':12,'launch_requires_Root_release':True},'execution':['Wait final metadata,one-batch review,and explicitRootrelease. No collection/build fromthisplan.','Collect exact16 originals/14groups sequentially;SDK3e4→7cfe→2af complete. Nevercollect prep0d9/e506;R18c131/476 alreadycontext. Verify percommit rawrename/M/A preimages,postimages,move100percentidentity and allowedpolicy/docs paths.','Verifyall87manifestendpoints plus4oldmoved-source endpoints;active native3615/text6303;reference4fileblobs andtoolsproviderJSON separatelybound. No silentdenominator change orCPPdeletioncalledPASS.','Recomputeold/newcompleteheader/CPP closure,includingremovedoldheaders/CPPproviders. Freshall72activesourcecandidates pluscompleteconsumers,all9fallbacks/showIncludes,Donly. No authorobjects asfresh.','Verify retained active-native products/input/header/tools/external/submodule/flags once ifbounded,preserveactualoriginheads. SDKcoveredreferencesare separate records;never pretendcompiledproducts.','Compareall3544priorPASS(noPASS removed),real oldFAIL→PASS gains inremainingnative3615,removed2oldFAIL→SDKreference distinctly. CurrentnativePASS+nativeFAIL+2SDKrefs reconcilesbroader3617 dispositions.','Rerun onlyaffected nativeguards. Ifregressions,exactsource/headerdiagnostic andholdsealfornarrowrepair,neverguardbypass/fakeTCBdata/sourcecountmanipulation.','Record selectedd3d9i/CAP-xbdm dependencies and approvedfivefunctionproof;no new SDK/game link orruntime claim. No global provider/duplicate-policy migration.','Exclude two independent SolMaxlayoutrecoveries andallnonmanifestfuturework;no fullreplayunless unboundedclosure andRootfollowup.'], 'limits':m['limits'],'parent_gitlink_index':index,'no_collection_build_patch_apply_or_dryrun':True,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R19_FINITE_INTAKE_GATE_PLAN.json',plan)
config={'base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R19_FINITE_INTAKE_GATE_PLAN.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R18.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native385-command.json'),'baseline_named_gates':str(namedpath)}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:config[k+'_sha256']=sha(config[k])
save(H/'campaign-preparation.json',config)
summary={'state':plan['state'],'base':BASE,'groups':14,'commits':16,'manifest_paths':87,'full_rename_endpoints':91,'active_changed_CPP':72,'active_changed_headers':9,'removed_native_CPP':2,'SDK_provided_reference_units':2,'move_R100_files':4,'future_native_inventory':3615,'future_native_text':6303,'projected_header_consumers':len(header_cons),'fresh_gate':len(selected),'retained_native_rows':len(retained),'selected_prior_PASS':projection['selected_prior_PASS'],'selected_prior_FAIL':projection['selected_prior_FAIL'],'candidate72_prior_PASS':projection['active_candidate72_prior_PASS'],'candidate72_prior_FAIL':projection['active_candidate72_prior_FAIL'],'active_actual_header_hits':len(active_actual),'affected_native_guards':[r['name']for r in impact if r['rerun_required']],'remaining_ungated_FAIL':remaining_ungated_baseline_failures,'issues':issues,'plan_sha256':sha(H/'R19_FINITE_INTAKE_GATE_PLAN.json'),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'SDK_coverage_sha256':sha(H/'SDK-provider-coverage-and-accounting.json'),'source_mutated':False,'compiler_started':False};save(H/'preparation-summary.json',summary);print(json.dumps(summary,indent=2))
