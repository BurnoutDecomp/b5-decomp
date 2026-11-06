"""Exact Root-authorized R19 finite header-consumer gate and verified-unaffected composite."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys,struct,ast
from collections import defaultdict,deque
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent
read=lambda p:json.loads(Path(p).read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def module(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def now():return datetime.now(timezone.utc).isoformat()
C=read(H/'campaign.json');plan=read(C['plan']);W=Path(C['workspace']);CAN=Path(C['canonical']);P=CAN.parent

def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']


def main():
 ap=argparse.ArgumentParser();ap.add_argument('--jobs',type=int,default=8);args=ap.parse_args();assert 1<=args.jobs<=8
 state=read(H/'intake-receipts.json');head=state['head'];assert state['state']=='complete'and len(state['receipts'])==16 and len(state['move_proofs'])==4;guards(head)
 auth=read(H/'R19_ROOT_AUTHORIZATION.json');assert auth['root_explicit_authorization']and auth['commits']==plan['commits']and auth['plan_sha256']==C['plan_sha256']==sha(C['plan'])
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates','preflight','review']:assert sha(C[k])==C[k+'_sha256']
 e=module('R19_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];baseline=read(C['baseline_report']);old={r['source']:r for r in baseline['results']};oldaudit=read(C['baseline_artifact_manifest']);previous=oldaudit['source_snapshot'];snapshot=e.common.source_snapshot();native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);removed=set(plan['removed_source_paths']);removed_cpp={p for p in removed if p.endswith('.cpp')};assert len(removed)==4 and len(removed_cpp)==2 and all(not old[p]['passed']for p in removed_cpp)
 assert len(snapshot['hashes'])==6303 and set(previous['hashes'])-set(snapshot['hashes'])==removed and not set(snapshot['hashes'])-set(previous['hashes']);assert len(native)==3615 and set(old)-set(native)==removed_cpp and not set(native)-set(old)
 changed,affected=e.closure(previous,snapshot);expected_delta={p for p in plan['all_affected_path_endpoints']if p.startswith(('src/','vendor/'))and Path(p).suffix.lower()in e.common.TEXT_EXT};assert changed==expected_delta and len(changed)==85
 sources=sorted((affected&set(native))|set(plan['active_modified_CPP']));projection=read(plan['projection'])
 if sources!=projection['selected_sources']or len(sources)!=116:save(H/'CLOSURE_MISMATCH_R19.json',{'head':head,'actual_sources':sources,'actual_count':len(sources),'extra':sorted(set(sources)-set(projection['selected_sources'])),'action':'Stop before compiler;Root reconciliation,no fullreplay.'});raise RuntimeError('Postcollectionclosure mismatch')
 assert len(plan['active_modified_CPP'])==72 and not (removed_cpp&set(sources));external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 affectednames={Path(p).name.lower()for p in affected};external_edges=[]
 for directory,tree in external['include_trees'].items():
  for rel in tree['files']:
   path=Path(directory)/rel
   if path.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',path.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if Path(match[1].replace('\\','/')).name.lower()in affectednames:external_edges.append({'source':str(path),'include':match[1]})
 assert not external_edges
 # These six tracked support inputs are deliberately outside the native source/text denominator.
 support={str((W/p).resolve()):sha(W/p)for p in plan['added_reference_policy_paths']};assert len(support)==6
 policy=read(W/'tools/x360_sdk_providers.json');assert policy['inventory_change']['previous_eligible_sources']==3617 and policy['inventory_change']['new_eligible_sources']==3615 and policy['inventory_change']['compile_gains']==0
 assert {d['former_source']for d in policy['inventory_change']['dispositions']}==removed_cpp;assert len(policy['provider']['symbols'])==5;sdk=Path(baseline['sdk']);SDKfiles={}
 archive=sdk/policy['provider']['archive'];assert sha(archive)==policy['provider']['archive_sha256'];SDKfiles[str(archive)]=policy['provider']['archive_sha256']
 for lib in policy['sdk_only_link_libraries']:
  p=sdk/lib['path'];assert sha(p)==lib['sha256'];SDKfiles[str(p)]=lib['sha256']
 proofroot=H.parent.parent/'wave15/sdk_gpu_terra';proofkeys={'provider-byte-comparison.json':'comparison_sha256','compare-provider.py':'comparison_script_sha256','provider-functions.json':'function_relocations_sha256','provider3.map':'map_sha256','provider3.log':'link_log_sha256','provider3.rsp':'link_rsp_sha256'};proofpins={}
 for rel,key in proofkeys.items():p=proofroot/rel;assert sha(p)==policy['proof'][key];proofpins[str(p)]=policy['proof'][key]
 for rel,member in [('d3d9i-device.obj',policy['provider']['members'][0]),('d3d9i-draw.obj',policy['provider']['members'][1])]:p=proofroot/rel;assert sha(p)==member['sha256'];proofpins[str(p)]=member['sha256']
 SDKrecords=[]
 for d in policy['inventory_change']['dispositions']:
  p=d['former_source'];ref=d['reference'];assert not (W/p).exists()and (W/ref).is_file();SDKrecords.append({'former_source':p,'baseline_passed':False,'baseline_status':'FAIL','baseline_log':old[p]['log'],'baseline_log_sha256':oldaudit['artifact_manifest'][p]['log_sha256'],'status':'SDK_PROVIDED_REFERENCE','reference':ref,'reference_working_sha256':sha(W/ref),'reference_blob':plan['planned_source_images'][ref]['final'],'provider_archive':str(archive),'provider_archive_sha256':policy['provider']['archive_sha256'],'native_compile_PASS':False,'compiler_gain':False})
 assert len(SDKrecords)==2
 providerpin={'head':head,'policy_path':str(W/'tools/x360_sdk_providers.json'),'policy_working_sha256':sha(W/'tools/x360_sdk_providers.json'),'policy_git_blob':plan['planned_source_images']['tools/x360_sdk_providers.json']['final'],'supporting_reference_README_policy_files':support,'selected_SDK_library_files':SDKfiles,'approved_function_relocation_table_constant_link_proof_files':proofpins,'five_functions':policy['provider']['symbols'],'SDK_reference_records':SDKrecords,'reference_units':2,'native_before':3617,'native_after':3615,'broader_disposition_units':3617,'compiler_gains_from_reference_selection':0,'CAP_xbdm_requirements':policy['runtime_requirements'],'only_SDK_only_NOENTRY_link_claim_retained':True,'new_link_performed':False,'full_game_link_or_runtime_proved':False};save(H/'SDK-provider-reference-pins-before.json',providerpin)
 reused=[r for r in baseline['results']if r['source']in set(native)and r['source']not in set(sources)];assert len(reused)==3499;e.verify_manifest({'results':reused},oldaudit['artifact_manifest'])
 save(H/'postcollection-native-closure.json',{'head':head,'base':C['base'],'active_text_before':6307,'active_text_after':6303,'native_before':3617,'native_after':3615,'changed_active_input_endpoints85':sorted(changed),'selected_sources':sources,'fresh_count':116,'retained_active_native_count':3499,'SDK_reference_units':2,'broader_disposition_denominator':3617,'removed_native_sources':sorted(removed_cpp),'selected_prior_PASS':sum(old[p]['passed']for p in sources),'selected_prior_FAIL':sum(not old[p]['passed']for p in sources),'all_other6222_active_inputs_unchanged':True,'actual116_matches_projection':True,'external_literal_edges':external_edges})
 save(H/'unaffected-products-verification.json',{'head':head,'base':C['base'],'retained_active_native_rows':3499,'artifact_entries_hash_verified_once':sum(len(oldaudit['artifact_manifest'][r['source']])for r in reused),'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'selected_old_products_and_SDK_reference_failures_not_rehashed_as_native':True})
 # No existing native safeguard consumes changed or moved-out headers;retain current facts without new compile.
 named=read(C['baseline_named_gates']);changed_abs={str((W/p).resolve()).lower()for p in changed};namedpins=[]
 for row in named['rows']:
  assert row['gate_satisfied']and not any(p.lower()in changed_abs for p in row['actual_included_headers']);assert sha(row['source'])==row['source_sha256']
  for p,h in row['actual_included_headers'].items():assert sha(p)==h
  namedpins.append({'name':row['name'],'source_sha256':row['source_sha256'],'original_head':row.get('original_head',named['head']),'coverage_role':'unaffected historical native safeguard retained,no rerun'})
 save(H/'native-safeguard-current-reuse.json',{'head':head,'prior_report':C['baseline_named_gates'],'prior_report_sha256':C['baseline_named_gates_sha256'],'rows':namedpins,'fresh_guard_audits':0,'retained_guard_records':7,'all_actual_source_header_inputs_unchanged':True})
 out=H/'native116';temp=H/'native116-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(args.jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(git('diff','--binary','--no-renames',C['base'])).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'active_input_paths':sorted(changed),'source_sha256':{p:snapshot['hashes'].get(p)for p in changed},'supporting_inputs':support,'SDK_provider_pin_sha256':sha(H/'SDK-provider-reference-pins-before.json'),'selection':'fresh116+3499activeunaffected+2separateSDKreferenceunits','created_utc':now()};save(H/'source-metadata.json',meta);save(H/'native116-command.json',{'head':head,'argv':argv,'jobs':args.jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'R19actual116native','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json')});print('FROZEN_R19',head,'fresh116 retained3499 SDKrefs2','jobs',args.jobs,'PID',os.getpid(),'activeText6303native3615',flush=True)
 d=module('R19_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(sources);save(H/'native116-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()});after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after});assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']and set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes']);assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles and {p:sha(p)for p in proofpins}==proofpins;guards(head);save(H/'SDK-provider-reference-pins-after.json',{'head':head,'same_reference_policy_SDK_archive_libraries_and_approved_proofs':True,'before_sha256':sha(H/'SDK-provider-reference-pins-before.json'),'supporting_inputs':support,'SDK_files':SDKfiles,'approved_proof_files':proofpins})
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for directory,tree in external['include_trees'].items():
  for rel,h in tree['files'].items():allinputs[str((Path(directory)/rel).resolve()).lower()]=h
 actual={}
 for row in new['results']:
  found={}
  for line in Path(row['log']).read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs,('Unfrozen actual include',row['source'],p);found[p]=allinputs[p.lower()]
  actual[row['source']]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_actual_includes_frozen_before_and_unchanged_after':True})
 def flags(row):return [x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 save(H/'reuse-proof.json',{'head':head,'base':C['base'],'fresh_native':116,'retained_active_native':3499,'SDK_provided_reference_units':2,'native_denominator3615_explicit':True,'broader_dispositions3617_explicit':True,'native_text6303_explicit':True,'all_other6222_active_inputs_unchanged':True,'removed2oldFAIL_not_compiler_gains':True,'complete_old_new_header_CPP_closure116':True,'all9canonical_fallbacks_and_equal_baseline_flags':True,'actual_include_trace_sha256':sha(H/'actual-includes.json'),'unaffected_products_once_sha256':sha(H/'unaffected-products-verification.json'),'SDK_provider_support_before_sha256':sha(H/'SDK-provider-reference-pins-before.json'),'SDK_provider_support_after_sha256':sha(H/'SDK-provider-reference-pins-after.json'),'seven_unaffected_guard_records_retained_no_new_audit':True,'native_guard_reuse_sha256':sha(H/'native-safeguard-current-reuse.json'),'no_fullreplay_or_newlink':True})
 rows=[];newsha=sha(out/'compile-results.json')
 for p in native:
  if p in idx:value={**idx[p],'build_head':head,'origin_kind':'fresh R19 active native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:
   row=old[p];value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'verified unchanged active R18 native row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',requested=3615,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3615-composite['passed'];composite['provenance']={'kind':'Active-native COMPOSITE116fresh+3499verifiedR18;2SDKreferences separately accounted','source_head':head,'fresh':116,'retained_active_R18':3499,'SDK_reference_records':2,'broader_unit_dispositions':3617,'real_prior_build_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')};save(H/'composite-active-native.json',composite);summary=e.compare(composite,baseline);assert set(summary['removed_sources'])==removed_cpp and not summary['new_sources'];manifest={p:v for p,v in oldaudit['artifact_manifest'].items()if p in set(native)};manifest.update(newart);assert len(manifest)==3615
 compiler_gains=[p for p in summary['new_passes']if p not in removed_cpp];assert len(compiler_gains)==len(summary['new_passes']);audit={'head':head,'base':C['base'],'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'supporting_reference_policy_inputs':support,'SDK_provider_pins_sha256':sha(H/'SDK-provider-reference-pins-before.json'),'SDK_reference_records':SDKrecords,'native_denominator':3615,'broader_dispositions':3617,'fresh_actual_compiles':116,'retained_actual_active_R18_rows':3499,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 regressions=[]
 for p in summary['regressions']:
  row=idx[p];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':[x for x in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in x or 'fatal error'in x or 'timeout'in x.lower()]})
 owned={p:idx[p]['passed']for p in plan['active_modified_CPP']};crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];pending=bool(regressions or summary['timeouts']);assert composite['passed']+composite['failed']+len(SDKrecords)==3617;guards(head)
 result={'head':head,'base':C['base'],'status':'PENDING_OWNED_REGRESSION_REPAIR'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'native_requested':3615,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_PROVIDED_REFERENCE_units':2,'SDK_reference_records':SDKrecords,'broader_unit_dispositions':3617,'baseline_native_requested':3617,'native_denominator_delta':-2,'baseline_active_text_inputs':6307,'active_text_inputs':6303,'native_text_delta':-4,'fresh_actual_compiles':116,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_active_R18_rows':3499,'R18_prior_PASS':3544,'R18_prior_PASS_retained':summary['prior_passes_retained'],'genuine_compiler_gains':len(compiler_gains),'compiler_gain_sources':compiler_gains,'compiler_gains_from_SDK_reclassification':0,'oldFAIL_to_SDK_reference':2,'regressions':regressions,'timeouts':summary['timeouts'],'active72_candidate_CPP_results':owned,'active72_candidate_passed':sum(owned.values()),'active72_candidate_failed':sum(not v for v in owned.values()),'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':16,'groups':14,'manifest_paths87':87,'all_affected_path_endpoints':91,'active_modified_CPP':72,'active_modified_headers':9,'removed_source_CPP':2,'removed_source_headers':2,'byte_identical_reference_moves':state['move_proofs'],'provider_support_input_sha256':sha(H/'SDK-provider-reference-pins-before.json'),'provider_proof_scope':'Root-approved five SDK helper functions with code/reloc/data/table/constant proof and SDK-only strictNOENTRYlink. No new or fullgame link/runtime claim. CAP/xbdm explicit.','artifact_hash_entries':sum(len(v)for v in manifest.values()),'reused_artifact_entries_verified_once':read(H/'unaffected-products-verification.json')['artifact_entries_hash_verified_once'],'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':args.jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'preflight_sha256':C['preflight_sha256'],'review_sha256':C['review_sha256'],'plan_sha256':C['plan_sha256'],'named_safeguards':{'fresh_audits':0,'retained_records':7,'report_sha256':sha(H/'native-safeguard-current-reuse.json')},'canonical_head':C['published_base'],'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['limits']+read(C['preflight'])['limits'],'scope':'116fresh active native CPPs+3499verified activeR18 rows+2separateSDKreference dispositions. Compiler gains excludeSDKreclassification. Allsource/ref/policy/SDK/tool/header contexts pinned. No layoutrecoveries,fullreplay,newlink/CRT/runtime/globalbyteparity/image/launch/publication claim.','publication_allowed':False,'goal':'ACTIVE','created_utc':now()};filename='REGRESSION_DIAGNOSTIC_R19.json'if pending else 'FROZEN_COMPOSITE_DIAGNOSTIC_R19.json';save(H/filename,result)
 if not pending:
  assert summary['prior_passes_retained']==3544;ref='codex/experimental-x360-native-r19-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  result['local_checkpoint']=ref
 completion={'head':head,'status':result['status'],'native_requested':3615,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_reference_units':2,'broader_dispositions':3617,'fresh':116,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_active':3499,'prior_PASS_retained':summary['prior_passes_retained'],'genuine_compiler_gains':len(compiler_gains),'SDK_reclassification_compiler_gains':0,'regressions':len(regressions),'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'goal':'ACTIVE'}
 if not pending:completion['local_checkpoint']=result['local_checkpoint']
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':result['status'],'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
