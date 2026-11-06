"""One authorized finite179 native gate and provenance-preserving R20 composite."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
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
 state=read(H/'intake-receipts.json');head=state['head'];assert state['state']=='complete'and len(state['receipts'])==4;guards(head)
 auth=read(H/'R21_ROOT_AUTHORIZATION.json');assert auth['root_explicit_authorization']and auth['commits']==plan['commits']and auth['plan_sha256']==C['plan_sha256']==sha(C['plan'])
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_guard_retention','baseline_named_gates','baseline_SDK_provider_pins','composition','manifest','preflight','review','preflight_closure']:assert sha(C[k])==C[k+'_sha256']
 out=H/'native179';temp=H/'native179-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 save(H/'operation.json',{'phase':'R21_postcollection_input_and_retained_verification','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json')})
 print('R21_PREPARING',head,'PID',os.getpid(),flush=True)
 e=module('R21_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base']
 baseline=read(C['baseline_report']);old={r['source']:r for r in baseline['results']};oldaudit=read(C['baseline_artifact_manifest']);priorseal=read(C['baseline_seal']);previous=oldaudit['source_snapshot'];snapshot=e.common.source_snapshot()
 native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);assert len(native)==3616 and set(native)==set(old)and len(snapshot['hashes'])==6305 and set(snapshot['hashes'])==set(previous['hashes'])
 changed,affected=e.closure(previous,snapshot);assert changed==set(plan['unique_paths'])and len(changed)==22
 sources=sorted((affected&set(native))|set(plan['CPP']));projection=read(plan['projection']);pfclosure=read(C['preflight_closure']);assert sources==projection['selected_sources']==sorted(pfclosure['finite_native_source_gate'])and len(sources)==179
 assert sum(old[p]['passed']for p in sources)==178 and [p for p in sources if not old[p]['passed']]==['src/GameSource/Game/BrnGameModule.cpp']
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 names={Path(p).name.lower()for p in affected};external_edges=[]
 for d,t in external['include_trees'].items():
  for rel in t['files']:
   p=Path(d)/rel
   if p.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if Path(match[1].replace('\\','/')).name.lower()in names:external_edges.append({'source':str(p),'include':match[1]})
 assert not external_edges
 SDKpins=read(C['baseline_SDK_provider_pins']);support=SDKpins['supporting_inputs'];SDKfiles=SDKpins['SDK_files'];proofpins=SDKpins['approved_proof_files'];SDKrecords=priorseal['SDK_reference_records']
 assert len(SDKrecords)==2 and all(r['status']=='SDK_PROVIDED_REFERENCE'and not r['compiler_gain']for r in SDKrecords)
 assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles
 save(H/'SDK-reference-retention-before.json',{'head':head,'original_provider_pins':C['baseline_SDK_provider_pins'],'original_provider_pins_sha256':C['baseline_SDK_provider_pins_sha256'],'supporting_inputs':support,'SDK_files':SDKfiles,'historical_approved_proof_pins_retained':proofpins,'historical_proofs_not_reexecuted_or_rehashed':True,'SDK_reference_records':SDKrecords,'active_native':3616,'broad_dispositions':3618,'no_link_or_policy_change':True})
 reused=[r for r in baseline['results']if r['source']not in set(sources)];assert len(reused)==3437
 print('R21_VERIFY_RETAINED',len(reused),'products once',flush=True);e.verify_manifest({'results':reused},oldaudit['artifact_manifest'])
 save(H/'unaffected-products-verification.json',{'head':head,'base':C['base'],'reused_native_rows':3437,'artifact_entries_hash_verified_once':sum(len(oldaudit['artifact_manifest'][r['source']])for r in reused),'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'selected_author_products_not_substituted':True})
 save(H/'postcollection-native-closure.json',{'head':head,'base':C['base'],'changed22paths':sorted(changed),'selected_sources':sources,'fresh_count':179,'retained_native_count':3437,'native_before_after':[3616,3616],'text_before_after':[6305,6305],'SDK_refs':2,'broad_dispositions':3618,'selected_prior_PASS':178,'selected_prior_FAIL':1,'new_provider_count':0,'actual179_matches_both_independent_and_preflight':True,'external_edges':external_edges,'all_other6283_text_inputs_unchanged':True})
 named=read(C['baseline_named_gates']);target_abs={str((W/p).resolve()).lower()for p in changed};namedrows=[]
 for r in named['rows']:
  assert r['gate_satisfied']and not any(p.lower()in target_abs for p in r['actual_included_headers'])and sha(r['source'])==r['source_sha256']
  for p,h in r['actual_included_headers'].items():assert sha(p)==h
  namedrows.append({'name':r['name'],'original_head':r.get('original_head',named['head']),'source_sha256':r['source_sha256'],'role':'unaffected named safeguard retained,no new compile'})
 assert len(namedrows)==7
 save(H/'native-safeguard-current-reuse.json',{'head':head,'report':C['baseline_named_gates'],'report_sha256':C['baseline_named_gates_sha256'],'R20_retention_sha256':C['baseline_guard_retention_sha256'],'fresh_audits':0,'retained_records':7,'all_actual_inputs_unchanged':True,'rows':namedrows})
 driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(args.jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 save(H/'source-metadata.json',{'head':head,'base':C['base'],'sha256':hashlib.sha256(git('diff','--binary',C['base'])).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':plan['unique_paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'selection':'179fresh+3437verifiedR20native+2separateSDKrefs','created_utc':now()})
 save(H/'native179-command.json',{'head':head,'argv':argv,'jobs':args.jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external})
 (H/'operation.json').write_text(json.dumps({'phase':'R21actual179native','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json')},indent=2)+'\n')
 print('FROZEN_R21',head,'fresh179 retained3437 SDKrefs2','jobs',args.jobs,'PID',os.getpid(),'text6305 native3616',flush=True)
 d=module('R21_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(sources)
 save(H/'native179-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()})
 print('R21_NATIVE_TERMINAL',new['passed'],'PASS',new['failed'],'FAIL;pinning after-context',flush=True)
 after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after});assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']and set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes'])
 assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles;guards(head)
 save(H/'SDK-reference-retention-after.json',{'head':head,'same_policy_references_archive_dependencies':True,'historical_proofs_retained_without_rehash':True,'before_sha256':sha(H/'SDK-reference-retention-before.json'),'SDK_refs':2,'no_newlink':True})
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
 def flags(row):return[x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 save(H/'reuse-proof.json',{'head':head,'base':C['base'],'fresh_native':179,'retained_native':3437,'SDK_refs':2,'active3616_broad3618_explicit':True,'all_other6283_text_inputs_unchanged':True,'allheaderconsumers_and10CPPfresh':True,'all9_actualflags_equal_R20':True,'actual_include_sha256':sha(H/'actual-includes.json'),'unaffected_product_verification_sha256':sha(H/'unaffected-products-verification.json'),'SDK_before_after_pins_sha256':sha(H/'SDK-reference-retention-after.json'),'seven_guards_retained_no_newaudit':True,'no_fullreplay_link':True})
 rows=[];newsha=sha(out/'compile-results.json')
 for p in native:
  if p in idx:value={**idx[p],'build_head':head,'origin_kind':'fresh R21 final-layout native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:
   row=old[p];value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'verified untouched activeR20 row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',requested=3616,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'Active-native COMPOSITE179fresh+3437verifiedR20;2SDKrefsseparate','source_head':head,'fresh':179,'retained_active_R20':3437,'SDK_reference_units':2,'broader_unit_dispositions':3618,'real_prior_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')}
 save(H/'composite-active-native.json',composite);summary=e.compare(composite,baseline);assert not summary['new_sources']and not summary['removed_sources'];manifest={**oldaudit['artifact_manifest'],**newart};assert len(manifest)==3616
 audit={'head':head,'base':C['base'],'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'SDK_reference_records':SDKrecords,'SDK_retention_sha256':sha(H/'SDK-reference-retention-after.json'),'fresh_actual_compiles':179,'retained_actual_active_R20_rows':3437,'active_native_denominator':3616,'broader_unit_dispositions':3618,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 regressions=[]
 for p in summary['regressions']:
  row=idx[p];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':[x for x in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in x or 'fatal error'in x or 'timeout'in x.lower()]})
 pending=bool(regressions or summary['timeouts']);status='PENDING_OWNED_REGRESSION_REPAIR'if pending else'SEALED_NO_REGRESSIONS';guards(head)
 checkpoint=None
 if not pending:
  assert summary['prior_passes_retained']==3615;checkpoint='codex/experimental-x360-native-r21-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+checkpoint).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',checkpoint,head],cwd=W,check=True)
 crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];review=read(C['review']);pf=read(C['preflight']);limits=plan['limits']+review['limits']
 result={'head':head,'base':C['base'],'status':status,'local_checkpoint':checkpoint,'coverage_kind':composite['provenance']['kind'],'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_PROVIDED_REFERENCE_units':2,'SDK_reference_records':SDKrecords,'broader_unit_dispositions':3618,'baseline_native_requested':3616,'native_denominator_delta':0,'baseline_text':6305,'active_text':6305,'text_delta':0,'fresh_actual_compiles':179,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_active_R20_rows':3437,'R20_prior_PASS':3615,'R20_prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(summary['new_passes']),'old_failure_repair_sources':summary['new_passes'],'new_provider_PASS_rows':0,'regressions':regressions,'timeouts':summary['timeouts'],'candidate_CPP_results':{p:idx[p]['passed']for p in plan['CPP']},'Game_original_outer_guards':{'source':'src/GameSource/Game/BrnGameModule.cpp','fresh_PASS':idx['src/GameSource/Game/BrnGameModule.cpp']['passed'],'size_guard':'0x9A1300','Network_offset_guard':'0x8C3600','assertion_values_unchanged':True},'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':4,'groups':4,'changed_paths':22,'raw_path_endpoints':23,'CPP':10,'headers':12,'added_or_removed_paths':[],'artifact_hash_entries':sum(len(v)for v in manifest.values()),'reused_artifact_entries_verified_once':read(H/'unaffected-products-verification.json')['artifact_entries_hash_verified_once'],'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':args.jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'preflight_sha256':C['preflight_sha256'],'review_sha256':C['review_sha256'],'plan_sha256':C['plan_sha256'],'manifest_sha256':C['manifest_sha256'],'named_guards':{'fresh':0,'retained':7,'receipt_sha256':sha(H/'native-safeguard-current-reuse.json')},'canonical_head':C['published_base'],'parent_gitlink_index':plan['parent_gitlink_index'],'limits':limits,'scope':'179fresh final-layout native units +3437 verified R20 rows with genuine origins +2SDKrefs. No added provider or denominator change. No wave17 linkrepair/R22 source,full diagnostic/link/runtime/launch/wholeclassparity/publication.','publication_allowed':False,'goal':'ACTIVE','created_utc':now()}
 filename='REGRESSION_DIAGNOSTIC_R21.json'if pending else'FROZEN_COMPOSITE_DIAGNOSTIC_R21.json';save(H/filename,result)
 completion={'head':head,'status':status,'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_reference_units':2,'broader_dispositions':3618,'fresh':179,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_native':3437,'prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(summary['new_passes']),'old_failure_repair_sources':summary['new_passes'],'new_provider_PASS_rows':0,'regressions':len(regressions),'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'local_checkpoint':checkpoint,'goal':'ACTIVE'}
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':status,'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
