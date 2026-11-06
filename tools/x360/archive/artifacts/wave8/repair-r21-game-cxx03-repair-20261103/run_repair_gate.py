"""Single-CPP successor gate; retain original179 diagnostic and verified products."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,os,subprocess,sys
from collections import defaultdict
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;B=H.parent/'repair-r21-layout-intake-20261103';C=json.loads((B/'campaign.json').read_text());W=Path(C['workspace']);CAN=Path(C['canonical']);P=CAN.parent;SOURCE='src/GameSource/Game/BrnGameModule.cpp'
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def module(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==read(C['plan'])['parent_gitlink_index']
def now():return datetime.now(timezone.utc).isoformat()
def main():
 bind=read(H/'repair-commit-binding.json');head=bind['head'];base=bind['base'];guards(head);assert git('diff','--name-only',base).decode().splitlines()==[SOURCE]
 root=read(H/'ROOT_AUTHORIZATION_AND_REPAIR_PLAN.json');assert root['root_explicit_authorization']and root['base']==base and sha(W/SOURCE)==bind['postimage_sha256']
 priorcompletion=read(B/'completion.json');assert sha(priorcompletion['diagnostic_file'])==priorcompletion['diagnostic_sha256']=='ccb632adacb50baf41e1081181caa4efa60740e4cea62ec082630d312032a250'
 assert sha(B/'composite-active-native.json')==priorcompletion['report_sha256']=='11de4dd9820ad08c19e05394bdd5d4ebfc8f66365b4efd650af5fc9d37e90efe'
 assert sha(B/'composite-artifact-audit.json')==priorcompletion['audit_sha256']=='678d1a8b1daca49e9847469cafe459fde0f583cec29210195452c10fbf9984dd'
 baseline=read(B/'composite-active-native.json');oldaudit=read(B/'composite-artifact-audit.json');priorseal=read(priorcompletion['diagnostic_file']);old={r['source']:r for r in baseline['results']};assert len(old)==3616 and baseline['passed']==3615 and baseline['failed']==1 and not old[SOURCE]['passed']
 out=H/'native1';temp=H/'native1-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 save(H/'operation.json',{'phase':'R21_Game_source_and_context_freeze','head':head,'owned_pid':os.getpid(),'jobs':1,'live_report':str(out/'compile-results.json')});print('R21_GAME_PREPARING',head,'PID',os.getpid(),flush=True)
 e=module('R21_repair_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=base;snapshot=e.common.source_snapshot();previous=oldaudit['source_snapshot'];native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp'])
 assert len(snapshot['hashes'])==6305 and len(native)==3616 and set(native)==set(old)and set(snapshot['hashes'])==set(previous['hashes'])
 changed,affected=e.closure(previous,snapshot);assert changed=={SOURCE}and affected&set(native)=={SOURCE};assert {p:h for p,h in snapshot['hashes'].items()if p!=SOURCE}=={p:h for p,h in previous['hashes'].items()if p!=SOURCE}
 incoming=sorted(p for p,ch in previous['dependencies'].items()if SOURCE in ch);assert not incoming
 target=str((W/SOURCE).resolve()).lower();groups=defaultdict(list)
 for row in baseline['results']:groups[str(Path(row['origin_report']).parent.parent/'actual-includes.json')].append(row['source'])
 actual_consumers=[];origins=[]
 for p,sources in groups.items():
  trace=read(p)['rows'];assert set(sources)<=set(trace)
  for s in sources:
   if any(path.lower()==target for path in trace[s]):actual_consumers.append(s)
  origins.append({'path':p,'current_rows_checked':len(sources)})
 assert not actual_consumers
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 SDKbefore=read(B/'SDK-reference-retention-before.json');support=SDKbefore['supporting_inputs'];SDKfiles=SDKbefore['SDK_files'];assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles
 # All3615 retained PASS products were already hashed in phase1:3437baseline verification +178phase1 report_check.
 assert sha(B/'unaffected-products-verification.json')==read(B/'reuse-proof.json')['unaffected_product_verification_sha256']
 retained=[r for r in baseline['results']if r['source']!=SOURCE];assert len(retained)==3615 and all(r['passed']for r in retained)
 save(H/'unchanged-and-retained-proof.json',{'head':head,'base':base,'changed_CPP':SOURCE,'all_other6304_inputs_and_allheaders_unchanged':True,'native3616_text6305_unchanged':True,'static_incoming_CPP_edges':incoming,'current_origin_actual_CPP_include_consumers':actual_consumers,'actual_trace_origins':origins,'selected_native_sources':[SOURCE],'retained_PASS_rows':3615,'unaffected_products_NOT_rehashed':True,'verified_product_chain':{'3437priorrows_proof':str(B/'unaffected-products-verification.json'),'proof_sha256':sha(B/'unaffected-products-verification.json'),'178fresh_phase1_rows_report':str(B/'native179/compile-results.json'),'report_sha256':sha(B/'native179/compile-results.json'),'phase1_audit_sha256':sha(B/'composite-artifact-audit.json')},'external_context_equal_phase1':True,'phase1_external_no_wrapper_CPP_edges_retained':True,'seven_named_guards_retention_sha256':sha(B/'native-safeguard-current-reuse.json'),'no_repeat179_or_guard_audit':True})
 for p,h in root['actual_header_sha256'].items():assert sha(W/p)==h
 named=read(C['baseline_named_gates'])
 for r in named['rows']:
  assert sha(r['source'])==r['source_sha256']
  for p,h in r['actual_included_headers'].items():assert sha(p)==h
 save(H/'native-safeguard-current-reuse.json',{'head':head,'report':C['baseline_named_gates'],'report_sha256':C['baseline_named_gates_sha256'],'phase1_retention_sha256':sha(B/'native-safeguard-current-reuse.json'),'fresh_audits':0,'retained_records':7,'all_actual_inputs_unchanged':True})
 driver=W/'tools/build_x360.py';assert sha(driver)==read(B/'native179-command.json')['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in old[SOURCE]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs','1','--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes','--source',SOURCE]
 save(H/'source-metadata.json',{'head':head,'base':base,'sha256':hashlib.sha256(git('diff','--binary',base)).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':[SOURCE],'paths':[SOURCE],'source_sha256':{SOURCE:snapshot['hashes'][SOURCE]},'selection':'1freshGame+178verifiedphase1R21+3437verifiedR20+2SDKrefs','created_utc':now()})
 save(H/'native1-command.json',{'head':head,'argv':argv,'jobs':1,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'canonical_vendor_fallbacks':9,'private_TEMP_TMP':str(temp)});save(H/'external-inputs-before.json',{'head':head,'inputs':external})
 (H/'operation.json').write_text(json.dumps({'phase':'R21_Game_actual1native','head':head,'owned_pid':os.getpid(),'jobs':1,'live_report':str(out/'compile-results.json')},indent=2)+'\n')
 print('FROZEN_R21_GAME',head,'actual1 retained3615 jobs1 PID',os.getpid(),flush=True)
 d=module('R21_Game_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)=={SOURCE};row=idx[SOURCE]
 save(H/'native1-exit.json',{'head':head,'exit_code':code,'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()});print('R21_GAME_NATIVE_TERMINAL',new['passed'],'PASS',new['failed'],'FAIL',flush=True)
 after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after});assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']and set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes'])
 assert {p:sha(p)for p in support}==support and {p:sha(p)for p in SDKfiles}==SDKfiles;guards(head)
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((Path(d)/rel).resolve()).lower()]=h
 found={}
 for line in Path(row['log']).read_text(errors='replace').splitlines():
  if 'including file:'in line:
   p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs;found[p]=allinputs[p.lower()]
 save(H/'actual-includes.json',{'head':head,'rows':{SOURCE:found},'unique_headers':len(found),'all_actual_headers_bound_before_after':True})
 flags=lambda r:[x for x in r['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes'];assert flags(row)==flags(old[SOURCE])
 text=(W/SOURCE).read_text(encoding='utf-8-sig');assert '/D_XBOX'in row['command']and 'NativeGameModuleSize[sizeof(BrnGameModule) == 0x9A1300 ? 1 : -1]'in text and 'NativeNetworkOffset[offsetof(BrnGameModule, mNetworkModule) == 0x8C3600 ? 1 : -1]'in text
 save(H/'original-Game-guards-proof.json',{'head':head,'source':SOURCE,'source_sha256':snapshot['hashes'][SOURCE],'actual_XBOX_defined':True,'original_size':'0x9A1300','original_Network_offset':'0x8C3600','all_original_assertion_lines_unchanged':True,'fresh_source_PASS':row['passed'],'log_sha256':sha(row['log']),'author_source_difference_sha256':bind['author_difference_sha256']})
 save(H/'SDK-reference-retention-after.json',{'head':head,'source_policy_archive_dependencies_unchanged':True,'SDK_refs':2,'phase1_before_sha256':sha(B/'SDK-reference-retention-before.json'),'phase1_after_sha256':sha(B/'SDK-reference-retention-after.json'),'historical_proof_bindings_retained_without_rehash':True,'no_link':True})
 save(H/'reuse-proof.json',{'head':head,'base':base,'fresh_native':1,'retained_active_native':3615,'origin_layers':{'fresh_repairedGame':1,'retained_R21_phase1':178,'retained_verified_R20':3437},'SDK_refs':2,'active3616_broad3618':True,'unchanged_and_retained_proof_sha256':sha(H/'unchanged-and-retained-proof.json'),'all9_actualflags_equal_phase1':True,'all_actual_includes_bound_sha256':sha(H/'actual-includes.json'),'named_guard_retention_sha256':sha(H/'native-safeguard-current-reuse.json'),'original_Game_guards_proof_sha256':sha(H/'original-Game-guards-proof.json'),'no_repeat179_fullsweep_link_or_header_change':True})
 freshsha=sha(out/'compile-results.json');rows=[]
 for p in native:
  if p==SOURCE:value={**row,'build_head':head,'origin_kind':'fresh R21 five-line Game C++03 repair compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':freshsha}
  else:
   value={**old[p],'retained_via_report':str(B/'composite-active-native.json'),'retained_via_report_sha256':sha(B/'composite-active-native.json'),'coverage_layer_role':'unchanged PASS retained after oneCPP repair;phase1 product proof reused'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==old[p][k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',requested=3616,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'Active-native COMPOSITE1freshRepairedGame+178verifiedR21phase1+3437verifiedR20;2SDKrefsseparate','source_head':head,'fresh':1,'retained_active_R21_phase1':178,'retained_verified_R20':3437,'SDK_reference_units':2,'broader_unit_dispositions':3618,'initial179_phase1_report_preserved':str(B/'native179/compile-results.json'),'initial_failure_receipt_preserved':str(B/'REMAINING_FAILURE_R21.json'),'real_prior_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')}
 save(H/'composite-active-native.json',composite);summary=e.compare(composite,baseline);assert not summary['new_sources']and not summary['removed_sources'];manifest={**oldaudit['artifact_manifest'],**newart};assert len(manifest)==3616
 audit={'head':head,'base':base,'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'SDK_reference_records':priorseal['SDK_reference_records'],'fresh_actual_compiles':1,'retained_active_R21_phase1_rows':178,'retained_verified_R20_rows':3437,'active_native_denominator':3616,'broader_unit_dispositions':3618,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'unaffected_products_not_rehashed':True,'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 pending=bool(not row['passed']or summary['regressions']or summary['timeouts']);status='PENDING_GAME_SOURCE_REPAIR'if pending else'SEALED_NO_REGRESSIONS_ALL_ACTIVE_NATIVE_PASS';checkpoint=None
 if not pending:
  assert composite['passed']==3616 and composite['failed']==0 and summary['prior_passes_retained']==3615 and summary['new_passes']==[SOURCE]
  checkpoint='codex/experimental-x360-native-r21-game-repaired-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+checkpoint).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',checkpoint,head],cwd=W,check=True)
 guards(head);crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c']
 result={'head':head,'base':base,'status':status,'local_checkpoint':checkpoint,'prior_frozen_checkpoint':'codex/experimental-x360-native-r21-checkpoint','prior_frozen_head':base,'coverage_kind':composite['provenance']['kind'],'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_PROVIDED_REFERENCE_units':2,'SDK_reference_records':priorseal['SDK_reference_records'],'broader_unit_dispositions':3618,'active_text':6305,'text_delta':0,'native_denominator_delta':0,'fresh_actual_compiles':1,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'phase1_actual179_passed':178,'phase1_actual179_failed':1,'retained_R21_phase1_rows':178,'retained_verified_R20_rows':3437,'prior_PASS':3615,'prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(summary['new_passes']),'old_failure_repair_sources':summary['new_passes'],'new_provider_PASS_rows':0,'regressions':summary['regressions'],'timeouts':summary['timeouts'],'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'repair_changed_CPP':SOURCE,'exact_five_line_cpp_only':True,'no_header_change':True,'constructor_all4_lanes':'VectorIntrinsic=__vector4;Vector3(raw)body mV=v;W not cleared','constructor_proof_sha256':sha(H/'ROOT_AUTHORIZATION_AND_REPAIR_PLAN.json'),'original_Game_guards_proof_sha256':sha(H/'original-Game-guards-proof.json'),'initial_failure_receipt':str(B/'REMAINING_FAILURE_R21.json'),'initial_failure_receipt_sha256':sha(B/'REMAINING_FAILURE_R21.json'),'initial179_seal_sha256':priorcompletion['diagnostic_sha256'],'artifact_hash_entries':sum(len(v)for v in manifest.values()),'unaffected_products_not_rehashed':True,'canonical_vendor_fallbacks':9,'jobs':1,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-active-native.json'),'report_sha256':sha(H/'composite-active-native.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'named_guards':{'fresh':0,'retained':7,'receipt_sha256':sha(H/'native-safeguard-current-reuse.json')},'canonical_head':C['published_base'],'parent_gitlink_index':read(C['plan'])['parent_gitlink_index'],'limits':priorseal['limits'],'scope':'Exact5constructorlines/oneCPP repair only. SinglefreshGame and retained178phase1/3437R20 rows keep trueorigins;notafreshfullsweep. Prior179failure/contextgappreserved. Noheader/guard/stub/lanezero/counterchange,link/R22/canonical/index/main/dev/ledger/origin/publication.','publication_allowed':False,'goal':'ACTIVE','created_utc':now()}
 filename='PENDING_GAME_REPAIR_DIAGNOSTIC.json'if pending else'FROZEN_COMPOSITE_DIAGNOSTIC_R21_REPAIRED.json';save(H/filename,result)
 completion={'head':head,'status':status,'native_requested':3616,'native_passed':composite['passed'],'native_failed':composite['failed'],'SDK_reference_units':2,'broader_dispositions':3618,'fresh':1,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_native':3615,'prior_PASS_retained':summary['prior_passes_retained'],'old_failure_repairs':len(summary['new_passes']),'regressions':len(summary['regressions']),'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'local_checkpoint':checkpoint,'goal':'ACTIVE'}
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':status,'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
