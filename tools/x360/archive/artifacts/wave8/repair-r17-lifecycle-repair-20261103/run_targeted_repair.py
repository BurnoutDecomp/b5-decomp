"""Exact Root-authorized one-line Lifecycle type repair after pinned R17 gate."""
from pathlib import Path
from datetime import datetime,timezone
from collections import defaultdict
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;R=json.loads((H/'repair-plan.json').read_text());F=Path(R['phase1_root']);C=json.loads((F/'campaign.json').read_text());C['published_base']='3534f7f16103a58ff904bbd1604e18e9934868f4';W=Path(C['workspace']);CAN=Path(C['canonical']);P=CAN.parent;originalplan=json.loads(Path(C['plan']).read_text())
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def now():return datetime.now(timezone.utc).isoformat()
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def module(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==originalplan['parent_gitlink_index']
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--run',action='store_true');args=ap.parse_args()
 if not args.run:ap.error('ExactRootrelease and explicit --run required.')
 completion=read(F/'completion.json');diagnostic=Path(completion['diagnostic_file']);assert sha(diagnostic)==completion['diagnostic_sha256'];phase1=read(diagnostic);assert phase1['head']==R['base']and phase1['fresh_actual_compiles']==676 and phase1['retained_R16_coverage_rows']==2941
 assert sha(F/'composite-artifact-audit.json')==completion['audit_sha256'];audit=read(F/'composite-artifact-audit.json');assert sha(audit['report'])==completion['report_sha256'];prior=read(audit['report']);assert prior['state']=='complete'and prior['requested']==3617
 assert sha(R['receipt'])==R['receipt_sha256'];guards(R['base']);assert git('rev-parse',R['commit']+'^').decode().strip()==R['base'];raw=git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',R['commit']).decode().splitlines();assert len(raw)==1;info,path=raw[0].split('\t',1);_,_,oldblob,newblob,status=info.split();assert path==R['source']and status=='M'and newblob==R['postimage_blob']and git('rev-parse','HEAD:'+path).decode().strip()==oldblob
 run=subprocess.run(['git','cherry-pick',R['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:save(H/'collection-conflict.json',{'code':run.returncode,'stdout':run.stdout,'stderr':run.stderr});raise RuntimeError('Unexpected repair conflict;no automatic resolution')
 head=git('rev-parse','HEAD').decode().strip();guards(head);assert git('rev-parse','HEAD:'+path).decode().strip()==newblob and git('diff','--name-only',R['base']).decode().splitlines()==[path];save(H/'intake-source-binding.json',{'head':head,'base':R['base'],'source_commit':R['commit'],'source':path,'preimage_blob':oldblob,'postimage_blob':newblob,'patch_sha256':hashlib.sha256(git('diff','--binary',R['base'])).hexdigest(),'phase1_diagnostic_sha256':sha(diagnostic)})
 e=module('R17_lifecycle_repair_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=R['base'];snapshot=e.common.source_snapshot();previous=audit['source_snapshot'];changed,affected=e.closure(previous,snapshot);assert changed=={path}and len(snapshot['hashes'])==6307 and set(snapshot['hashes'])==set(previous['hashes']);native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);assert len(native)==3617 and native==read(F/'source-metadata.json')['native_inventory']
 target=str((W/path).resolve()).lower();origin_groups=defaultdict(list)
 for row in prior['results']:origin_groups[str(Path(row['origin_report']).parent.parent/'actual-includes.json')].append(row['source'])
 actual_consumers=set();trace_receipts=[]
 for tracepath,sourcegroup in origin_groups.items():
  trace=read(tracepath)['rows'];missing=set(sourcegroup)-set(trace);assert not missing,(tracepath,missing)
  for p in sourcegroup:
   if any(h.lower()==target for h in trace[p]):actual_consumers.add(p)
  trace_receipts.append({'path':tracepath,'current_rows_checked':len(sourcegroup),'actual_changed_CPP_include_hits':[p for p in sourcegroup if any(h.lower()==target for h in trace[p])]});del trace
 sources=sorted((affected&set(native))|actual_consumers|{path});assert len(sources)==1,('Concrete consumers require expanded authorized gate',sources)
 external=e.external_inputs(prior)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==audit['external_inputs'][k]
 external_edges=[]
 for d,t in external['include_trees'].items():
  for rel in t['files']:
   p=Path(d)/rel
   if p.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if Path(match[1].replace('\\','/')).name.lower()==Path(path).name.lower():external_edges.append({'source':str(p),'include':match[1]})
 assert not external_edges
 save(H/'affected-repair-closure.json',{'head':head,'base':R['base'],'changed_CPP':[path],'static_affected_native':sorted(affected&set(native)),'actual_current_rows_checked':3617,'actual_include_trace_groups':trace_receipts,'actual_CPP_include_consumers':sorted(actual_consumers),'external_literal_CPP_include_edges':external_edges,'selected_sources':sources,'all_other6306_inputs_and_all_headers_unchanged':True,'native3617_text6307_stable':True})
 out=H/'native-repair';temp=H/'native-repair-temp';temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(F/'native676-command.json')['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in prior['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',prior['sdk'],'--out',str(out),'--jobs','1','--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes','--source',path]
 save(H/'source-metadata.json',{'head':head,'base':R['base'],'sha256':hashlib.sha256(git('diff','--binary',R['base'])).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':[path],'source_sha256':{path:sha(W/path)}});save(H/'command.json',{'head':head,'argv':argv,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'TEMP_TMP':str(temp),'canonical_fallbacks':9});save(H/'external-before.json',external);save(H/'operation.json',{'phase':'R17targetedLifecycleRepair','head':head,'owned_pid':os.getpid(),'sources':sources});print('FROZEN_R17_REPAIR',head,'fresh1','PID',os.getpid(),flush=True)
 d=module('R17_repair_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');save(H/'native-repair-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json')});assert set(idx)=={path}and code==0 and new['passed']==1 and new['failed']==0
 old={r['source']:r for r in prior['results']}
 def flags(r):return [x for x in r['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 assert flags(idx[path])==flags(old[path]);assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes'];guards(head);after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-after.json',after);allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((Path(d)/rel).resolve()).lower()]=h
 actual={}
 for row in new['results']:
  found={}
  for line in Path(row['log']).read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs;found[p]=allinputs[p.lower()]
  actual[row['source']]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_actual_headers_bound':True});safeguards=read(F/'named-safeguard-results.json');assert not safeguards['regressions']and safeguards['fresh_affected_count']==2 and safeguards['retained_unaffected_count']==5
 for row in safeguards['rows']:assert not any(p.lower()==target for p in row['actual_included_headers'])
 save(H/'reuse-proof.json',{'head':head,'base':R['base'],'fresh_repair_count':1,'retained_R17phase1_rows':675,'retained_prior_R16_rows':2941,'current_reused_rows':3616,'only_one_CPP_changed_all_headers_unchanged':True,'all_other6306_inputs_unchanged':True,'actual3617current_origin_trace_rows_checked':True,'no_incoming_CPP_consumers':True,'external_tools_flags_submodules_stable':True,'phase1_artifact_and_unaffected_product_checks_reused_without_rehash':True,'phase1_audit_sha256':sha(F/'composite-artifact-audit.json'),'phase1_rawfailedgate_preserved':True,'phase1_named_guard_report_sha256':sha(F/'named-safeguard-results.json'),'two_fresh_guard_passes_five_retained_unchanged_no_rerun':True,'no_full_replay_link_or_R18extras':True})
 rows=[];newsha=sha(out/'compile-results.json')
 for row in prior['results']:
  if row['source']==path:value={**idx[path],'build_head':head,'origin_kind':'fresh exact R17 Lifecycle successor repair','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:
   value={**row,'retained_via_R17_phase1_report':audit['report'],'retained_via_R17_phase1_report_sha256':sha(audit['report'])}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in prior.items()if k!='results'};composite.update(state='complete',requested=3617,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3617-composite['passed'];composite['provenance']={'kind':'R17 phase1affected676 plus exact fresh1callerrepair;current coverage COMPOSITE','source_head':head,'fresh_repair_rows':1,'retained_R17phase1_rows':675,'retained_prior_R16_rows':2941,'raw_phase1_failure_preserved':True,'all_retained_real_build_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')};save(H/'composite-effective.json',composite)
 baseline=read(C['baseline_report']);baseidx={r['source']:r for r in baseline['results']};summary=e.compare(composite,baseline);regressions=summary['regressions'];provider='src/SDKs/EATech/include/NFSMix/NFSLiveLink.cpp';current={r['source']:r for r in rows};assert summary['new_sources']==[provider]and not summary['removed_sources'];oldgains=[p for p in summary['new_passes']if p in baseidx];newproviders=[p for p in summary['new_passes']if p not in baseidx];pending=bool(regressions or summary['timeouts']or not current[provider]['passed']);manifest={**audit['artifact_manifest'],**newart};a={'head':head,'base':R['base'],'R16_base':C['base'],'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'summary_versus_R16':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'phase1_audit_sha256':sha(F/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',a)
 crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];owned={p:current[p]['passed']for p in originalplan['raw_changed_CPP']};assert all(owned.values());net=git('diff','--name-only',C['base']).decode().splitlines();assert set(net)==set(originalplan['net_changed_paths'])|{path}
 seal={'head':head,'base':C['base'],'phase1_head':R['base'],'status':'PENDING_RESIDUAL_REGRESSION'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'requested':3617,'passed':composite['passed'],'failed':composite['failed'],'actual_R17phase1_compile_attempts':676,'actual_repair_compile_attempts':1,'current_fresh_repair_rows':1,'current_retained_R17phase1_rows':675,'current_retained_R16_rows':2941,'R16_prior_passes':3408,'R16_prior_passes_retained':summary['prior_passes_retained'],'existing_failure_gains':len(oldgains),'existing_failure_gain_sources':oldgains,'new_provider_passes':len(newproviders),'new_provider_pass_sources':newproviders,'total_added_PASS_rows':len(summary['new_passes']),'regressions':regressions,'timeouts':summary['timeouts'],'all104_original_candidate_CPP_results':owned,'caller_repair_pass':True,'new_native_provider':phase1['new_native_provider'],'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':23,'authorized_caller_repair_commits':1,'cumulative_net_changed_paths':len(net),'cumulative_net_CPP':103,'cumulative_headers':33,'added_paths':originalplan['added_paths'],'text_inputs':6307,'native_inventory':3617,'artifact_hash_entries':sum(len(v)for v in manifest.values()),'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'phase1_regression_diagnostic':str(diagnostic),'phase1_regression_diagnostic_sha256':sha(diagnostic),'phase1_raw676_report_sha256':read(F/'native676-exit.json')['report_sha256'],'repair_report_sha256':newsha,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'named_safeguards_sha256':sha(F/'named-safeguard-results.json'),'phase1_compiler_exit':phase1['compiler_exit'],'repair_compiler_exit':0,'binder_exit':0,'limits':phase1['limits'],'scope':'Explicit676phase1affectedcompiles+1freshrepair;currentcoverage675phase1+2941priorR16+1repair. Originalfailedgatepreserved. Realretainedbuildoriginsunchanged;all6306otherinputs/header/tool/externalcontextstable. No fullreplay,link/CRT/network/audio/runtime/byte-parity/image/launch/publication claim. WeakE/G accepted asverified.','canonical_head':C['published_base'],'parent_gitlink_index':originalplan['parent_gitlink_index'],'publication_allowed':False,'goal':'ACTIVE','created_utc':now()};filename='REGRESSION_DIAGNOSTIC_R17_REPAIR.json'if pending else 'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R17.json';save(H/filename,seal)
 if not pending:
  assert summary['prior_passes_retained']==3408 and composite['passed']==3464 and composite['failed']==153 and len(oldgains)==55 and len(newproviders)==1;ref='codex/experimental-x360-native-r17-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  seal['local_checkpoint']=ref
 result={'head':head,'status':seal['status'],'requested':3617,'passed':composite['passed'],'failed':composite['failed'],'phase1_fresh_attempts':676,'repair_fresh_attempts':1,'current_retained_phase1_rows':675,'current_retained_R16_rows':2941,'current_fresh_repair_rows':1,'R16_prior_passes_retained':summary['prior_passes_retained'],'existing_failure_gains':len(oldgains),'new_provider_passes':len(newproviders),'regressions':len(regressions),'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-effective.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'compiler_exit':0,'binder_exit':0,'goal':'ACTIVE'}
 if not pending:result['local_checkpoint']=seal['local_checkpoint']
 save(H/'completion.json',result);(H/'operation.json').write_text(json.dumps({'phase':seal['status'],'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':result},indent=2)+'\n',encoding='utf-8');guards(head);print(json.dumps(result,indent=2),flush=True)
if __name__=='__main__':main()
