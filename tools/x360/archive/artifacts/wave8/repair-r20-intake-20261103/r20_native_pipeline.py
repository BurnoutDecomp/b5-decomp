"""Exact Root-authorized R20 finite header-consumer gate and verified-unaffected composite."""
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
 state=read(H/'intake-receipts.json');head=state['head'];assert state['state']=='complete'and len(state['receipts'])==2;guards(head);auth=read(H/'R20_ROOT_AUTHORIZATION.json');assert auth['root_explicit_authorization']and auth['commits']==plan['commits']and auth['plan_sha256']==C['plan_sha256']==sha(C['plan'])
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_guard_retention','baseline_named_gates','baseline_SDK_provider_pins','preflight','review','preflight_closure']:assert sha(C[k])==C[k+'_sha256']
 e=module('R20_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];baseline=read(C['baseline_report']);old={r['source']:r for r in baseline['results']};oldaudit=read(C['baseline_artifact_manifest']);priorseal=read(C['baseline_seal']);snapshot=e.common.source_snapshot();previous=oldaudit['source_snapshot'];native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);provider='src/SDKs/RenderEngineClub/MAIN/components/src/states/vertexprogramstate.cpp';shadow='src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp';entry='src/GameSource/Main/BrnMainX360.cpp'
 assert len(snapshot['hashes'])==6305 and set(snapshot['hashes'])-set(previous['hashes'])==set(plan['added_paths'])and not set(previous['hashes'])-set(snapshot['hashes']);assert len(native)==3616 and set(native)-set(old)=={provider}and not set(old)-set(native);assert not old[shadow]['passed']and old[entry]['passed']
 changed,affected=e.closure(previous,snapshot);assert changed==set(plan['paths'])and len(changed)==5;sources=sorted((affected&set(native))|set(plan['CPP']));projection=read(plan['projection']);pfclosure=read(C['preflight_closure']);assert sources==projection['selected_sources']==sorted(pfclosure['final_native_gate'])and len(sources)==36
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 affectednames={Path(p).name.lower()for p in affected};external_edges=[]
 for d,t in external['include_trees'].items():
  for rel in t['files']:
   p=Path(d)/rel
   if p.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if Path(match[1].replace('\\','/')).name.lower()in affectednames:external_edges.append({'source':str(p),'include':match[1]})
 assert not external_edges
 # Existing R19 SDK coverage remains two references,not new native passes. Its inventory delta is historical.
 SDKafter=read(C['baseline_SDK_provider_pins']);support=SDKafter['supporting_inputs'];SDKfiles=SDKafter['SDK_files'];proofpins=SDKafter['approved_proof_files'];assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles and {p:sha(p)for p in proofpins}==proofpins;SDKrecords=priorseal['SDK_reference_records'];assert len(SDKrecords)==2 and all(r['status']=='SDK_PROVIDED_REFERENCE'and not r['compiler_gain']for r in SDKrecords)
 save(H/'SDK-reference-retention-before.json',{'head':head,'R19_pin_file':C['baseline_SDK_provider_pins'],'R19_pin_sha256':C['baseline_SDK_provider_pins_sha256'],'SDK_reference_records':SDKrecords,'supporting_inputs':support,'SDK_files':SDKfiles,'approved_proof_files':proofpins,'SDK_refs':2,'current_native_before':3615,'current_native_after':3616,'current_broad_dispositions':3618,'R19_policy_inventory_delta_is_historical':True,'no_newlink_or_policy_change':True})
 reused=[r for r in baseline['results']if r['source']not in set(sources)];assert len(reused)==3580;e.verify_manifest({'results':reused},oldaudit['artifact_manifest'])
 save(H/'postcollection-native-closure.json',{'head':head,'base':C['base'],'changed5paths':sorted(changed),'selected_sources':sources,'fresh_count':36,'retained_native_count':3580,'native_before':3615,'native_after':3616,'native_text_before':6303,'native_text_after':6305,'SDK_refs_unchanged':2,'broad_dispositions':3618,'new_provider_CPP':provider,'selected_prior_PASS':sum(old[p]['passed']for p in sources if p in old),'selected_prior_FAIL':sum(not old[p]['passed']for p in sources if p in old),'selected_new_provider':1,'actual36_matches_both_independent_and_preflight':True,'new_header_name_resolution_included':True,'external_edges':external_edges,'all_other6300_prior_inputs_unchanged':True})
 save(H/'unaffected-products-verification.json',{'head':head,'base':C['base'],'reused_native_rows':3580,'artifact_entries_hash_verified_once':sum(len(oldaudit['artifact_manifest'][r['source']])for r in reused),'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'newprovider_and_selected_old_products_not_substituted':True})
 named=read(C['baseline_named_gates']);target_abs={str((W/p).resolve()).lower()for p in changed};namedrows=[]
 for r in named['rows']:
  assert r['gate_satisfied']and not any(p.lower()in target_abs for p in r['actual_included_headers']);assert sha(r['source'])==r['source_sha256']
  for p,h in r['actual_included_headers'].items():assert sha(p)==h
  namedrows.append({'name':r['name'],'original_head':r.get('original_head',named['head']),'source_sha256':r['source_sha256'],'role':'unaffectednativeguardretained,no newcompile'})
 save(H/'native-safeguard-current-reuse.json',{'head':head,'report':C['baseline_named_gates'],'report_sha256':C['baseline_named_gates_sha256'],'R19_retention_sha256':C['baseline_guard_retention_sha256'],'fresh_audits':0,'retained_records':7,'all_actual_inputs_unchanged':True,'rows':namedrows})
 out=H/'native36';temp=H/'native36-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(args.jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(git('diff','--binary',C['base'])).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':plan['paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'new_provider':provider,'selection':'36fresh+3580verifiedR19native+2separateSDKrefs','created_utc':now()};save(H/'source-metadata.json',meta);save(H/'native36-command.json',{'head':head,'argv':argv,'jobs':args.jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'R20actual36native','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json')});print('FROZEN_R20',head,'fresh36 retained3580 SDKrefs2','jobs',args.jobs,'PID',os.getpid(),'activeText6305native3616',flush=True)
 d=module('R20_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(sources);save(H/'native36-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()});after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after});assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']and set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes']);assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles and {p:sha(p)for p in proofpins}==proofpins;guards(head);save(H/'SDK-reference-retention-after.json',{'head':head,'same_policy_references_archive_dependencies_proof_files':True,'before_sha256':sha(H/'SDK-reference-retention-before.json'),'SDK_refs':2,'no_newlink':True})
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((Path(d)/rel).resolve()).lower()]=h
 actual={}
 for row in new['results']:
  found={}
  for line in Path(row['log']).read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs,('Unfrozen actual include',row['source'],p);found[p]=allinputs[p.lower()]
  actual[row['source']]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_actual_headers_bound_before_after':True})
 def flags(row):return [x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old.get(p,baseline['results'][0])),p
 save(H/'reuse-proof.json',{'head':head,'base':C['base'],'fresh_native':36,'retained_native':3580,'SDK_refs':2,'active_native3616_broad3618_explicit':True,'text6305_add2_explicit':True,'all_other6300_prior_inputs_unchanged':True,'newprovider_cpp_and_header_bound':plan['added_paths'],'allheaderconsumers_and3CPPfresh':True,'all9_and_actualflags_equal_R19':True,'actual_include_sha256':sha(H/'actual-includes.json'),'unaffected_product_verification_sha256':sha(H/'unaffected-products-verification.json'),'SDK_before_after_pins_sha256':sha(H/'SDK-reference-retention-after.json'),'seven_guards_retained_no_newaudit':True,'no_fullreplay_link_or_partial_layout_intake':True})
 rows=[];newsha=sha(out/'compile-results.json')
 for p in native:
  if p in idx:value={**idx[p],'build_head':head,'origin_kind':'fresh R20 finalnativecompile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:
   row=old[p];value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'verified untouched activeR19native row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',requested=3616,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'Active-native COMPOSITE36fresh+3580verifiedR19;2SDKrefsseparate','source_head':head,'fresh':36,'retained_active_R19':3580,'new_provider':provider,'SDK_reference_units':2,'broader_unit_dispositions':3618,'real_prior_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')};save(H/'composite-active-native.json',composite);summary=e.compare(composite,baseline);assert summary['new_sources']==[provider]and not summary['removed_sources'];existinggains=[p for p in summary['new_passes']if p in old];newproviderpasses=[p for p in summary['new_passes']if p not in old];manifest={**oldaudit['artifact_manifest'],**newart};assert len(manifest)==3616;audit={'head':head,'base':C['base'],'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'SDK_reference_records':SDKrecords,'SDK_retention_sha256':sha(H/'SDK-reference-retention-after.json'),'fresh_actual_compiles':36,'retained_actual_active_R19_rows':3580,'new_provider':provider,'active_native_denominator':3616,'broader_unit_dispositions':3618,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 regressions=[]
 for p in summary['regressions']:
  row=idx[p];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':[x for x in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in x or 'fatal error'in x or 'timeout'in x.lower()]})
 providerok=idx[provider]['passed'];pending=bool(regressions or summary['timeouts']or not providerok);crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];current={r['source']:r for r in rows};assert composite['passed']+composite['failed']+len(SDKrecords)==3618;guards(head)
 result={'head':head,'base':C['base'],'status':'PENDING_OWNED_REGRESSION_REPAIR'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_PROVIDED_REFERENCE_units':2,'SDK_reference_records':SDKrecords,'broader_unit_dispositions':3618,'baseline_native_requested':3615,'native_denominator_delta':1,'baseline_text':6303,'active_text':6305,'text_delta':2,'fresh_actual_compiles':36,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_active_R19_rows':3580,'R19_prior_PASS':3613,'R19_prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(existinggains),'old_failure_repair_sources':existinggains,'new_provider_PASS_rows':len(newproviderpasses),'new_provider_PASS_sources':newproviderpasses,'total_added_native_PASS_rows':len(summary['new_passes']),'Entry_controlflow_correction':{'source':entry,'baseline_PASS':True,'fresh_PASS':idx[entry]['passed'],'old_failure_gain':False},'Shadowing_oldfailure_candidate':{'source':shadow,'baseline_PASS':False,'fresh_PASS':idx[shadow]['passed']},'new_provider':{'source':provider,'passed':providerok,'object':idx[provider]['object']if providerok else None,'object_sha256':newart[provider].get('object_sha256'),'machine':idx[provider]['machine'],'actual_header_count':len(actual[provider]),'original_full_size_alignment_claimed':False,'minimum_closed_NoBind_payload':0x28},'regressions':regressions,'timeouts':summary['timeouts'],'three_candidate_CPP_results':{p:idx[p]['passed']for p in plan['CPP']},'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':2,'groups':2,'changed_paths':5,'CPP':3,'headers':2,'added_paths':plan['added_paths'],'artifact_hash_entries':sum(len(v)for v in manifest.values()),'reused_artifact_entries_verified_once':read(H/'unaffected-products-verification.json')['artifact_entries_hash_verified_once'],'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':args.jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'preflight_sha256':C['preflight_sha256'],'review_sha256':C['review_sha256'],'plan_sha256':C['plan_sha256'],'named_guards':{'fresh':0,'retained':7,'receipt_sha256':sha(H/'native-safeguard-current-reuse.json')},'canonical_head':C['published_base'],'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['limits']+read(C['preflight'])['limits'],'scope':'36freshfinalnativeunits+3580verifiedR19rows+2SDKrefs;oldShadowrepair andnewprovider andEntrybehaviorcorrectnesscountedseparately. Originalfullstate sizeof/alignmentunclaimed;0x28 minimumclosedNoBindview only. Author24/12/48andDeviceproofsremainbounded,noglobalbyteclaim. No ICE/Main/Replaytypedpartials/layout/gameLink/CRT/runtime/launch/publication.','publication_allowed':False,'goal':'ACTIVE','created_utc':now()};filename='REGRESSION_DIAGNOSTIC_R20.json'if pending else 'FROZEN_COMPOSITE_DIAGNOSTIC_R20.json';save(H/filename,result)
 if not pending:
  assert summary['prior_passes_retained']==3613;ref='codex/experimental-x360-native-r20-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  result['local_checkpoint']=ref
 completion={'head':head,'status':result['status'],'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_reference_units':2,'broader_dispositions':3618,'fresh':36,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_native':3580,'prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(existinggains),'new_provider_PASS_rows':len(newproviderpasses),'Entry_oldfailure_gain':0,'regressions':len(regressions),'provider_passed':providerok,'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'goal':'ACTIVE'}
 if not pending:completion['local_checkpoint']=result['local_checkpoint']
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':result['status'],'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
