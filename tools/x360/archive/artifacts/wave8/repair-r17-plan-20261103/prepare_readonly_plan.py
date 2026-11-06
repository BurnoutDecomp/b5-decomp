"""Read-only R17 source-image,inventory,mount and projected-header plan;no mutation/build."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,re,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r16-intake-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='c033db619a2645fe09e6c59fcd67d08f663f4f2b'
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=P/'b5-decomp').decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=P/'b5-decomp').strip()
index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==index
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R16.json')=='57efb24348779692a644d3b63ee578eaf6748863869d507d4248437f0a769c18';assert sha(B/'composite-effective.json')=='f6bb7012cc7c162083ae466b5806162b68f761216a909d2c61b5d1c63e994397';assert sha(B/'composite-artifact-audit.json')=='e068c26eb0a9d988132d76cd03031341dfd9a3431c09420f634ef9f1753ce740'
manifest=A/'wave13/R17_READY_CANDIDATES.json';m=read(manifest);assert m['baseline']==BASE;audit=read(B/'composite-artifact-audit.json');snap=audit['source_snapshot'];baseline=read(B/'composite-effective.json');old={r['source']:r for r in baseline['results']};assert len(old)==3616 and baseline['passed']==3408 and baseline['failed']==208 and len(snap['hashes'])==6303
# Stable topological group order obeys explicit dependencies instead of blindly collecting deformation before Parts.
pending=list(m['groups']);ordered=[];done=set()
while pending:
 ready=next((g for g in pending if not g.get('depends_on')or g['depends_on']in done),None);assert ready is not None,'Cyclic/missing group dependency';ordered.append(ready);done.add(ready['name']);pending.remove(ready)
commits=[c for g in ordered for c in g['commits']];assert len(commits)==23 and len(set(commits))==23 and '3a25b4dd2ac7a80796718f555d29a83360d7a13e'not in commits
assert commits.index('312bc353d5744c3be63923e14383eb67d0c563f2')<commits.index('2dd172dc41af44c65f49921b6261fe1d016d218e');assert commits.index('58c6db4876809b8e7bea3813e0f78352db3f4913')>commits.index('b1da2a3388e70f1bd11b9c45baf53c3d998402c9')
images={};projected={};finaltext={};candidates=[];receipts=[];issues=[];already=[];changes_by_group=defaultdict(set)
for group in ordered:
 rs=group.get('receipts')or[group['receipt']]
 for rel in rs:
  path=Path(rel)if Path(rel).is_absolute()else A/rel;receipts.append({'group':group['name'],'path':str(path),'sha256':sha(path)})
 for c in group['commits']:
  parent=git('rev-parse',c+'^').decode().strip();changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status in ['M','A']and Path(p).suffix.lower()in ['.cpp','.h','.hpp'];assert p.startswith(('src/','vendor/'))and 'pc'not in [part.lower()for part in Path(p).parts]
   if p not in images:
    initial=subprocess.run(['git','rev-parse',BASE+':'+p],cwd=W,capture_output=True,text=True);blob=initial.stdout.strip()if initial.returncode==0 else None;images[p]={'baseline':blob,'final':blob};projected[p]=blob
   expected=None if a=='0'*40 else a
   if projected[p]==expected:disposition='normal preimage match'
   elif projected[p]==b:
    disposition='already identical final postimage;preserve during normal cherry-pick';already.append({'commit':c,'path':p,'current_blob':projected[p],'candidate_preimage':expected,'candidate_postimage':b})
   else:
    disposition='UNEXPECTED_PREIMAGE';issues.append({'commit':c,'path':p,'actual_projected_blob':projected[p],'expected_preimage':expected,'postimage':b})
   projected[p]=b;images[p]['final']=b;finaltext[p]=git('show',c+':'+p).decode('utf-8-sig',errors='replace');changes.append({'path':p,'status':status,'preimage_blob':expected,'postimage_blob':b,'projected_disposition':disposition});changes_by_group[group['name']].add(p)
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'changes':changes})
rawpaths=sorted(images);rawcpp=sorted(p for p in rawpaths if p.endswith('.cpp'));rawheaders=sorted(set(rawpaths)-set(rawcpp));netpaths=sorted(p for p,v in images.items()if v['baseline']!=v['final']);added=sorted(p for p,v in images.items()if v['baseline']is None);netcpp=sorted(p for p in netpaths if p.endswith('.cpp'));netheaders=sorted(set(netpaths)-set(netcpp));unchanged=sorted(set(rawpaths)-set(netpaths));newcpp=[p for p in added if p.endswith('.cpp')]
assert len(rawpaths)==137 and len(rawcpp)==104 and len(rawheaders)==33 and len(added)==4 and newcpp==['src/SDKs/EATech/include/NFSMix/NFSLiveLink.cpp'];assert len(already)==2 and all(r['commit']=='53876e869c6e4d9eba8d93687547f462957d4fba'for r in already)
# Native source collection follows build_x360.py,not PC CMake. Newly tracked include-directory CPP is intentionally included.
newkeys=set(snap['hashes'])|set(added);native=sorted(set(old)|set(newcpp));assert len(native)==3617 and len(newkeys)==6307
byname=defaultdict(set);lookup={p.lower():p for p in newkeys}
for p in newkeys:byname[Path(p).name.lower()].add(p)
graph={p:list(ch)for p,ch in snap['dependencies'].items()};dynamic=[]
for p,text in finaltext.items():
 deps=set()
 for directive in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',text,re.M):
  match=re.match(r'[<"]([^>"\r\n]+)[>"]',directive)
  if not match:dynamic.append({'path':p,'directive':directive});continue
  inc=match[1].replace('\\','/');deps.update(byname.get(Path(inc).name.lower(),[]));local=(W/Path(p).parent/inc).resolve()
  try:relative=local.relative_to(W).as_posix().lower()
  except ValueError:relative=''
  if relative in lookup:deps.add(lookup[relative])
 graph[p]=sorted(deps)
reverse=defaultdict(set)
for g in [snap['dependencies'],graph]:
 for p,children in g.items():
  for ch in children:reverse[ch].add(p)
def closure(seeds):
 reached=set(seeds);q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 return reached
hcons=sorted(closure(netheaders)&set(native));selected=sorted((closure(netpaths)&set(native))|set(rawcpp));retained=sorted(set(native)-set(selected));perheader=[{'header':p,'native_consumers':len(closure([p])&set(native))}for p in netheaders]
projection={'baseline':BASE,'method':'Read-only projected graph from immutable final candidate blobs,union sealed old graph;all literal basename matches/inactive branches retained. No patch apply/dry-run. External/actual include reconciliation deferred to final preflight/postcollection.','raw_header_count':33,'net_header_count':len(netheaders),'header_native_consumer_count':len(hcons),'header_native_consumers':hcons,'changed_source_plus_header_consumer_gate_count':len(selected),'projected_selected_sources':selected,'projected_retained_count':len(retained),'per_header':perheader,'new_nonliteral_candidate_sites':dynamic,'baseline_native':3616,'projected_native':3617,'baseline_text_inputs':6303,'projected_text_inputs':6307,'new_real_provider_CPP':newcpp,'all_raw104CPP_in_gate':set(rawcpp)<=set(selected)};save(H/'projected-inventory-and-header-closure.json',projection)
# Verify already-landed parent contexts through the published R16 collection receipts,not original-author ancestry guesses.
r16receipts=read(B/'intake-receipts.json')['receipts'];bycommit={r['source_commit']:r for r in r16receipts};parentproof=[]
for original in ['a363e3da3f56aaaff670e45f9824a9d0e1b34b2c','9265b55af6908b05e7123fb51ab51c9996cfcd62','82477a6ede28e59c0758ee9049346fb1705f9d79']:
 row=bycommit[original];integrated=row['integrated_commit'];assert subprocess.run(['git','merge-base','--is-ancestor',integrated,BASE],cwd=W).returncode==0;parentproof.append({'author_original':original,'integrated_R16_commit':integrated,'already_in_baseline':True,'do_not_recollect':True})
provider=newcpp[0];key=Path(provider).stem+'-'+hashlib.sha256(provider.lower().encode()).hexdigest()[:8];driver=W/'tools/build_x360.py';linkprep=W/'tools/prepare_x360_linker.py';prepared=P/'b5-decomp/build/x360/tool-host/link.exe'
route={'provider_CPP':provider,'author_created_by':'f70dfc30a26d56b8f3860fedb935222bf04725c0','existing_functions':'13 existing ledger functions per Root metadata;no production identity/ledger changes.','native_collector':'tools/build_x360.py --all:git ls-files *.cpp/*.c,then src/vendor prefix and no pc path segment. include/ directory does not excludeCPP.','provider_matches_native_collector':provider.startswith(('src/','vendor/'))and Path(provider).suffix.lower()=='.cpp'and 'pc'not in [p.lower()for p in Path(provider).parts],'projected_native_inventory':3617,'expected_object_basename':key+'.obj','expected_listing_basename':key+'.asm','expected_log_basename':key+'.log','build_driver_sha256':sha(driver),'build_driver_source_lines':'73-91 selection;112-138 object and report emission.','compile_stage':'Compile-only. A passing fresh provider row must enter current compile-results.json with PPC COFF0x01F2 and source/header bindings.','link_route':'No automatic game link stage exists in build_x360.py. A separately authorized strict native link must explicitly consume the fresh provider row/object along with selected native objects via preparedlink.exe;no PC CMake mount is involved.','prepared_linker':str(prepared),'prepared_linker_exists':prepared.is_file(),'linker_preparation_tool':str(linkprep),'linker_preparation_tool_sha256':sha(linkprep),'native_compile_mount_change_needed':False,'PC_CMakeLists_change_needed':False,'link_run_performed':False,'weak_E_alias_G':'Final58c required;Root verified legitimate weakE fallbackG. Not a source/link blocker or grounds for a fabricated provider/table. No new strictlink result claimed.'};save(H/'native-provider-build-route.json',route)
recommend='ONE_FRESH_FULL_NATIVE_3617';reason=f'Projected finite union is{len(selected)}/3617 native sources across33raw headers and multiple ABI/provider families;onefreshfull avoids a large reuse/composite surface and independently exercises the newly mounted provider.'
plan={'state':'READ_ONLY_PREPARED_AWAITING_METADATA_REVIEW_AND_ROOT_RELEASE','base':BASE,'manifest':str(manifest),'manifest_sha256':sha(manifest),'manifest_group_order':[g['name']for g in m['groups']],'required_collection_group_order':[g['name']for g in ordered],'commits':commits,'candidates':candidates,'receipts':receipts,'raw_paths':rawpaths,'planned_source_images':images,'raw_changed_CPP':rawcpp,'raw_changed_headers':rawheaders,'net_changed_paths':netpaths,'net_changed_CPP':netcpp,'net_changed_headers':netheaders,'already_final_unchanged_paths':unchanged,'already_identical_video_deltas':already,'added_paths':added,'new_provider_CPP':newcpp,'projected_native_inventory':3617,'projected_text_inputs':6307,'baseline_native_inventory':3616,'baseline_text_inputs':6303,'baseline_passed':3408,'baseline_failed':208,'unexpected_preimage_issues':issues,'already_landed_R16_parent_context':parentproof,'required_preparation_excluded':'3a25b4dd2ac7a80796718f555d29a83360d7a13e','required_last_NFSMix_successor':'58c6db4876809b8e7bea3813e0f78352db3f4913','projection':str(H/'projected-inventory-and-header-closure.json'),'projection_sha256':sha(H/'projected-inventory-and-header-closure.json'),'native_provider_route':str(H/'native-provider-build-route.json'),'native_provider_route_sha256':sha(H/'native-provider-build-route.json'),'recommended_gate':recommend,'gate_reason':reason,'resource_proposal':{'full_native_jobs_max':8,'private_jobs_max':4,'private_jobs_per_worker_max':1,'aggregate_logical_native_compiler_jobs_max':12,'actual_launch_requires_Root_release':True},'required_execution':['Wait sealed exact metadata preflight,one-batch review and explicit Root release;no collection/build under this preparation.','Sequentially collect exact23 original commits in dependency-corrected order. Preserve both already-identical Video paths without reverting or duplicating their content. Never collect prep3a25,a363or9265again.','Verify raw allowed per-commit paths/preimages or explicit already-final exceptions;stop unexpected conflicts/images;verify every final postimage.','Recompute actual tracking-based native3617/text6307/new4paths and compare net final source scope;include NFSLiveLink.cpp despite its include-directory location. Do not usePC CMake mounts or mutateproduction identity/ledger.','Recommended onefreshfullnative3617 gate,all9canonicalfallbacks/showIncludes,D-only outputs/TEMP,max8jobs under Rootresourceallocation;no author objects as products. Reconcile projected header scope with preflight/actual frozen tree first.','Pin every6307 source/header input plus external/tool/submodule context before/after and all actual include files. Bind every native result/log/PPC product;old failures remain actual failures.','Compareall3408R16priorpasses,newfailed-to-pass gains and genuinely new provider row separately. Newinventory1 is not a repaired oldfailure. Pin exact regressions and await narrow fixes without bypasses/fakes/drops.','Identify affected named safeguards from actual changed-header traces;rerun only those. Keep inherited numeric/ABI/runtime limits explicit.','No R17link/network/audio/GPU/runtime/launch/overall byte-match claim. Prepared strictlink routing is future separately authorized work;weakE/G accepted as verified.'], 'limits':m['limits'],'parent_gitlink_index':index,'no_source_collection_build_patch_apply_or_dry_run':True,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R17_FINITE_INTAKE_GATE_PLAN.json',plan)
config={'base':BASE,'workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R17_FINITE_INTAKE_GATE_PLAN.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R16.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native139-command.json'),'baseline_named_gates':str(B/'named-safeguard-results.json')}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:config[k+'_sha256']=sha(config[k])
save(H/'campaign-preparation.json',config)
summary={'state':plan['state'],'commits':23,'groups':14,'raw_paths':len(rawpaths),'raw_CPP':len(rawcpp),'raw_headers':len(rawheaders),'net_paths':len(netpaths),'net_CPP':len(netcpp),'net_headers':len(netheaders),'already_final_paths':unchanged,'added_paths':added,'projected_native':3617,'projected_text_inputs':6307,'projected_header_native_consumers':len(hcons),'projected_finite_union':len(selected),'projected_retained':len(retained),'recommended_gate':recommend,'unexpected_preimage_issues':issues,'plan_sha256':sha(H/'R17_FINITE_INTAKE_GATE_PLAN.json'),'projection_sha256':sha(H/'projected-inventory-and-header-closure.json'),'provider_route_sha256':sha(H/'native-provider-build-route.json'),'source_mutated':False,'compiler_started':False};save(H/'preparation-summary.json',summary)
print(json.dumps(summary,indent=2))
