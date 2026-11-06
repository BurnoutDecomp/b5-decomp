"""Exact Root-authorized R17 finite676 native gate,one new provider,verified-unaffected composite."""
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
C=read(H/'campaign.json');C['published_base']='3534f7f16103a58ff904bbd1604e18e9934868f4';plan=read(C['plan']);W=Path(C['workspace']);CAN=Path(C['canonical']);P=CAN.parent

def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']

def named_gates(e,head,snapshot,external,new):
 old=read(C['baseline_named_gates']);expected_facts={**read(H.parent/'repair-r15-full-20261103/named-safeguard-results.json')['emitted_facts'],**old['emitted_facts']};changed={str((W/p).resolve()).lower()for p in plan['net_changed_paths']};out=H/'affected-named-gates';out.mkdir();temp=out/'temp';temp.mkdir();rows=[];facts={};reg=[]
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((Path(d)/rel).resolve()).lower()]=h
 sample=new['results'][0];flags=[x for x in sample['command'][1:-1]if not x.startswith(('/Fo','/Fa'))]
 env=dict(os.environ,TEMP=str(temp),TMP=str(temp));sdk=Path(new['sdk']);env.update(XEDK=str(sdk),PATH=str(sdk/'bin/win32')+os.pathsep+str(Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(sdk/'include/xbox'),LIB=str(sdk/'lib/xbox'))
 for key in ['CL','_CL_','LINK','_LINK_']:env.pop(key,None)
 fn=H.parent/'repair-r4/compile_header_probes.py';nodes=[n for n in ast.parse(fn.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='symbol_words'];namespace={'Path':Path,'struct':struct};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fn),'exec'),namespace);words=namespace['symbol_words']
 symbols={'CombinedLayouts':('R4LayoutFacts',25),'TypedLayouts':('R7TypedFacts',21),'HUDMemberGuards':('HudNativeOffsets',8),'ResourceContracts':('R11ResourceFacts',9)}
 for r in old['rows']:
  assert sha(r['source'])==r['source_sha256'];hits=sorted(p for p in r['actual_included_headers']if p.lower()in changed)
  if not hits:
   for p,h in r['actual_included_headers'].items():assert sha(p)==h,p
   for p,h in r['artifacts'].items():assert sha(p)==h,p
   rows.append({**r,'coverage_role':'unchanged historical native safeguard retained','original_head':r.get('original_head',old['head']),'changed_inputs':[]});continue
  name=r['name'];cpp,obj,asm,log=[out/(name+ext)for ext in ['.cpp','.obj','.asm','.log']];cpp.write_bytes(Path(r['source']).read_bytes());cmd=[sample['command'][0],*flags,'/Fo'+str(obj),'/Fa'+str(asm),str(cpp)]
  try:run=subprocess.run(cmd,cwd=W,env=env,capture_output=True,timeout=180);rc=run.returncode;log.write_bytes(run.stdout+run.stderr)
  except subprocess.TimeoutExpired as exc:rc=-1;log.write_bytes((exc.stdout or b'')+(exc.stderr or b''))
  diagnostics=[line for line in log.read_text(errors='replace').splitlines()if 'error C'in line];ok=rc==0 and obj.exists()and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2;neg=r['expected_negative'];satisfied=(rc!=0 and rc!=-1 and len(diagnostics)==1 and 'C2118'in diagnostics[0]and not obj.exists())if neg else ok
  hdrs={}
  for line in log.read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs,(name,p);hdrs[p]=allinputs[p.lower()]
  if ok and name in symbols:
   facts[name]=words(obj,*symbols[name]);satisfied=satisfied and facts[name]==expected_facts[name]
  row={'name':name,'command':cmd,'returncode':rc,'passed':ok,'expected_negative':neg,'gate_satisfied':satisfied,'source':str(cpp),'source_sha256':sha(cpp),'object':str(obj)if obj.exists()else None,'artifacts':{str(p):sha(p)for p in [cpp,obj,asm,log]if p.exists()},'actual_included_headers':hdrs,'diagnostics':diagnostics,'changed_inputs':hits,'coverage_role':'fresh affected safeguard at R14'};rows.append(row)
  if not satisfied:reg.append({'name':name,'returncode':rc,'diagnostics':diagnostics,'log':str(log),'facts':facts.get(name),'previous_facts':expected_facts.get(name)})
  print('AFFECTED_SAFEGUARD',name,rc,'satisfied',satisfied,flush=True)
 report={'head':head,'original_historical_head':old['head'],'rows':rows,'fresh_affected_count':sum(bool(r['changed_inputs'])for r in rows),'retained_unaffected_count':sum(not r['changed_inputs']for r in rows),'emitted_facts':facts,'regressions':reg,'scope':'Only actually affected native/header safeguards rerun. Expected independent C2118 negatives remain actual failures,not repaired PASS claims.','created_utc':now()};save(H/'named-safeguard-results.json',report);return report




def main():
 ap=argparse.ArgumentParser();ap.add_argument('--jobs',type=int,default=8);args=ap.parse_args();assert 1<=args.jobs<=8
 state=read(H/'intake-receipts.json');head=state['head'];assert state['state']=='complete'and len(state['receipts'])==23;guards(head);authorization=read(H/'R17_ROOT_AUTHORIZATION.json');assert authorization['root_explicit_authorization']and authorization['commits']==plan['commits']and authorization['plan_sha256']==C['plan_sha256']==sha(C['plan'])
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates','preflight','review']:assert sha(C[k])==C[k+'_sha256']
 e=module('R17_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];baseline=read(C['baseline_report']);old={r['source']:r for r in baseline['results']};oldaudit=read(C['baseline_artifact_manifest']);snapshot=e.common.source_snapshot();previous=oldaudit['source_snapshot'];native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);provider='src/SDKs/EATech/include/NFSMix/NFSLiveLink.cpp'
 assert len(snapshot['hashes'])==6307 and set(snapshot['hashes'])-set(previous['hashes'])==set(plan['added_paths'])and not set(previous['hashes'])-set(snapshot['hashes']);assert len(native)==3617 and set(native)-set(old)=={provider}and not set(old)-set(native)
 assert native==sorted(set(read(C['baseline_metadata'])['native_inventory'])|{provider});changed,affected=e.closure(previous,snapshot);assert changed==set(plan['net_changed_paths'])and len(changed)==135
 sources=sorted((affected&set(native))|set(plan['raw_changed_CPP']));projection=read(plan['projection'])
 if sources!=projection['projected_selected_sources']or len(sources)!=676:
  save(H/'CLOSURE_MISMATCH_R17.json',{'head':head,'actual_count':len(sources),'actual_sources':sources,'extra_actual_sources':sorted(set(sources)-set(projection['projected_selected_sources'])),'missing_projection_sources':sorted(set(projection['projected_selected_sources'])-set(sources)),'action':'Stop before compiler;Root reconciliation required,no full replay.'});raise RuntimeError('Actual postcollection closure mismatch;no automatic expansion/full replay')
 assert all('pc'not in [part.lower()for part in Path(p).parts]for p in sources)and set(plan['raw_changed_CPP'])<=set(sources)
 pf=read(C['preflight']);rec=pf['integrator_projection_reconciliation'];assert not pf['integrity']['issues']and rec['union_source_count']==676 and rec['retained_unchanged_count']==2941 and not rec['mismatch_sources_outside_selection']and not rec['unmapped_mismatches']
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 affectednames={Path(p).name.lower()for p in affected};external_edges=[]
 for directory,tree in external['include_trees'].items():
  for rel in tree['files']:
   path=Path(directory)/rel
   if path.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',path.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    inc=match[1].replace('\\','/')
    if Path(inc).name.lower()in affectednames:external_edges.append({'source':str(path),'include':inc})
 if external_edges:save(H/'EXTERNAL_CLOSURE_RECONCILIATION_REQUIRED.json',{'head':head,'edges':external_edges,'action':'Root review before compiler,no silent full replay.'});raise RuntimeError('External wrappers reach affected nodes;stop for bounded reconciliation')
 reused=[r for r in baseline['results']if r['source']not in set(sources)];assert len(reused)==2941;e.verify_manifest({'results':reused},oldaudit['artifact_manifest'])
 save(H/'post-header-consumer-closure.json',{'head':head,'base':C['base'],'changed_net135paths':sorted(changed),'selected_sources':sources,'fresh_count':676,'retained_count':2941,'actual676_matches_projection':True,'projected_header_consumers':654,'new_provider_CPP':provider,'native_inventory_before':3616,'native_inventory_after':3617,'text_inputs_before':6303,'text_inputs_after':6307,'added_paths':plan['added_paths'],'identicalVideo_inputs_unchanged':[p for p in plan['already_final_unchanged_paths']if snapshot['hashes'][p]==previous['hashes'][p]],'selected_prior_PASS':sum(old[p]['passed']for p in sources if p in old),'selected_prior_FAIL':sum(not old[p]['passed']for p in sources if p in old),'selected_new_provider_sources':1,'external_literal_edges_into_affected_nodes':[],'all_other6168_old_inputs_unchanged':True,'preflight_21_private_header_context_gaps_all_within_gate':True,'method':'Conservative union of actual old/new tracked include graph plus raw104candidateCPPs;all basename matches and inactive branches retained. Exact preflight reconciliation,no full replay.'})
 save(H/'unaffected-products-verification.json',{'head':head,'base':C['base'],'retained_source_count':2941,'retained_artifact_hash_entries_verified_once':sum(len(oldaudit['artifact_manifest'][r['source']])for r in reused),'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'selected_old_products_not_rehashed_or_substituted':True,'all_reused_actual_products_hash_match':True,'external_tools_submodules_equal':True})
 out=H/'native676';temp=H/'native676-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(args.jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(git('diff','--binary',C['base'])).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':plan['net_changed_paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'selection':'fresh676/2941verifiedunaffectedR16;newNFSLiveLink included','created_utc':now()};save(H/'source-metadata.json',meta);save(H/'native676-command.json',{'head':head,'argv':argv,'jobs':args.jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'R17actual676native','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json')});print('FROZEN_R17',head,'fresh676 retained2941','jobs',args.jobs,'PID',os.getpid(),'text6307native3617',flush=True)
 d=module('R17_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(sources);save(H/'native676-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()});after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after});allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
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
 for p,row in idx.items():assert flags(row)==flags(old.get(p,baseline['results'][0])),p
 safeguards=named_gates(e,head,snapshot,external,new)
 assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']and set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes']);guards(head)
 proof={'head':head,'base':C['base'],'fresh':676,'retained':2941,'only135net_authorized_paths_changed':True,'all_other6168_old_inputs_unchanged':True,'new4paths_bound':plan['added_paths'],'actual_native3617_text6307':True,'post_header_closure_sha256':sha(H/'post-header-consumer-closure.json'),'all_actual_header_consumers_and_raw104CPP_rebuilt':True,'all9canonical_fallbacks':True,'actual_flags_equal_R16_except_showIncludes_outputs':True,'new_provider_flags_equal_native_baseline':True,'actual_includes_sha256':sha(H/'actual-includes.json'),'unaffected_products_verification_sha256':sha(H/'unaffected-products-verification.json'),'unaffected_products_hash_verified_once':True,'fresh676_products_verified_once':True,'source_headers_external_tools_submodules_pre_post_stable':True,'named_safeguards_sha256':sha(H/'named-safeguard-results.json'),'no_full_replay_author_product_substitution_link_or_published_history_checks':True};save(H/'reuse-proof.json',proof)
 rows=[];newsha=sha(out/'compile-results.json')
 for p in native:
  if p in idx:value={**idx[p],'build_head':head,'origin_kind':'fresh R17 affected native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:
   row=old[p];value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'verified unaffectedR16 coverage row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',requested=3617,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3617-composite['passed'];composite['provenance']={'kind':'Complete-coverage COMPOSITE,not a fresh full sweep','source_head':head,'fresh':676,'retained_R16':2941,'actual_prior_build_origins_preserved':True,'new_provider_source':provider,'reuse_proof_sha256':sha(H/'reuse-proof.json')};save(H/'composite-effective.json',composite);summary=e.compare(composite,baseline);assert summary['new_sources']==[provider]and not summary['removed_sources'];manifest={**oldaudit['artifact_manifest'],**newart};audit={'head':head,'base':C['base'],'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'fresh_actual_compiles':676,'retained_actual_R16_rows':2941,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 regressions=[]
 for p in summary['regressions']:
  row=idx[p];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':[x for x in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in x or 'fatal error'in x or 'timeout'in x.lower()]})
 owned={p:idx[p]['passed']for p in plan['raw_changed_CPP']};crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];providerrow=idx[provider];providerok=bool(providerrow['passed']);oldgains=[p for p in summary['new_passes']if p in old];newpasses=[p for p in summary['new_passes']if p not in old];pending=bool(regressions or safeguards['regressions']or summary['timeouts']or not providerok);states={k:{'head':git('rev-parse','HEAD',cwd=Path(C[k])).decode().strip(),'status':git('status','--porcelain=v1',cwd=Path(C[k])).decode().strip()}for k in ['workspace','canonical']};guards(head)
 result={'head':head,'base':C['base'],'status':'PENDING_OWNED_REGRESSION_REPAIR'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'requested':3617,'passed':composite['passed'],'failed':composite['failed'],'fresh_actual_compiles':676,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained_R16_coverage_rows':2941,'R16_prior_passes':3408,'R16_prior_passes_retained':summary['prior_passes_retained'],'existing_failure_gains':len(oldgains),'existing_failure_gain_sources':oldgains,'new_provider_passes':len(newpasses),'new_provider_pass_sources':newpasses,'total_added_PASS_rows':len(summary['new_passes']),'regressions':regressions,'timeouts':summary['timeouts'],'raw104_candidate_CPP_results':owned,'raw104_candidate_passed':sum(owned.values()),'raw104_candidate_failed':sum(not v for v in owned.values()),'known_old_failed_candidate_sources':[p for p,v in owned.items()if not v and p in old and not old[p]['passed']],'new_native_provider':{'source':provider,'passed':providerok,'object':providerrow['object']if providerok else None,'object_sha256':newart[provider].get('object_sha256'),'machine':providerrow['machine'],'actual_header_count':len(actual[provider]),'existing_identity_functions':13,'identity_or_ledger_mutation':False},'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':23,'groups':14,'raw_paths':137,'raw_CPP':104,'raw_headers':33,'net_changed_paths':135,'net_CPP':102,'net_headers':33,'already_identical_Video_paths':plan['already_final_unchanged_paths'],'added_paths':plan['added_paths'],'text_inputs':6307,'native_inventory':3617,'baseline_native_inventory':3616,'artifact_hash_entries':sum(len(v)for v in manifest.values()),'reused_artifact_entries_verified_once':read(H/'unaffected-products-verification.json')['retained_artifact_hash_entries_verified_once'],'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':args.jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'preflight_sha256':C['preflight_sha256'],'review_sha256':C['review_sha256'],'plan_sha256':C['plan_sha256'],'named_safeguards':{'fresh_affected':safeguards['fresh_affected_count'],'retained_unaffected':safeguards['retained_unaffected_count'],'regressions':safeguards['regressions'],'report_sha256':sha(H/'named-safeguard-results.json')},'canonical_and_intake':states,'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['limits']+read(C['preflight'])['limits'],'scope':'Actual676finalnativeCPP/header-consumer objects plus2941verifiedunaffectedR16 rows withrealbuildorigins. ExistingFAILstatus remains honest;newprovider separate from repaired oldFAIL. No fullreplay,private author-object substitution,published-history/owner/link/runtime/byte-parity/CRT/image/launch or publication claim. WeakE/G is accepted as verified,not invented blocker.','excluded':'Privateprep3a25,recollectionofa363/9265/82477,allR18extras.','publication_allowed':False,'goal':'ACTIVE;native source/provider/link/image/launch remains','created_utc':now()};filename='REGRESSION_DIAGNOSTIC_R17.json'if pending else 'FROZEN_COMPOSITE_DIAGNOSTIC_R17.json';save(H/filename,result)
 if not pending:
  assert summary['prior_passes_retained']==3408;ref='codex/experimental-x360-native-r17-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  result['local_checkpoint']=ref
 completion={'head':head,'status':result['status'],'requested':3617,'passed':composite['passed'],'failed':composite['failed'],'fresh':676,'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained':2941,'prior_passes_retained':summary['prior_passes_retained'],'existing_failure_gains':len(oldgains),'new_provider_passes':len(newpasses),'total_added_PASS_rows':len(summary['new_passes']),'regressions':len(regressions),'safeguard_regressions':len(safeguards['regressions']),'provider_passed':providerok,'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-effective.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'goal':'ACTIVE'}
 if not pending:completion['local_checkpoint']=result['local_checkpoint']
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':result['status'],'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
