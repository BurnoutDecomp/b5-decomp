"""Exact Root-authorized post-full R15 one-CPP regression repair;never a full replay."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;R=json.loads((H/'repair-plan.json').read_text());F=Path(R['phase1_root']);C=json.loads((F/'campaign.json').read_text());W=Path(C['workspace']);CAN=Path(C['canonical']);P=CAN.parent
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
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==read(C['plan'])['parent_gitlink_index']
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--run',action='store_true');args=ap.parse_args()
 if not args.run:ap.error('Explicit --run required;phase1 must be terminal and diagnostic pinned.')
 completion=read(F/'completion.json');diagnostic=Path(completion['diagnostic_file']);assert sha(diagnostic)==completion['diagnostic_sha256'];phase1=read(diagnostic);assert phase1['head']==R['base']and phase1['fresh_actual_compiles']==3616 and phase1['reused_rows']==0
 a=read(F/'full-native-artifact-audit.json');assert sha(F/'full-native-artifact-audit.json')==completion['audit_sha256']and sha(a['report'])==completion['report_sha256'];full=read(a['report']);assert full['state']=='complete'and full['requested']==3616 and len(full['results'])==3616
 assert sha(R['receipt'])==R['receipt_sha256']and sha(R['author_binding'])==R['author_binding_sha256'];guards(R['base']);assert git('rev-parse',R['commit']+'^').decode().strip()==R['base']
 raw=git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',R['commit']).decode().splitlines();assert len(raw)==1;info,path=raw[0].split('\t',1);_,_,oldblob,newblob,status=info.split();assert path==R['source']and status=='M'and path.endswith('.cpp')and newblob==R['author_postimage_blob']and git('rev-parse','HEAD:'+path).decode().strip()==oldblob
 run=subprocess.run(['git','cherry-pick',R['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:save(H/'collection-conflict.json',{'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr});raise RuntimeError('Repair conflict;no automatic resolution')
 head=git('rev-parse','HEAD').decode().strip();guards(head);assert git('rev-parse','HEAD:'+path).decode().strip()==newblob and git('diff','--name-only',R['base']).decode().splitlines()==[path]
 save(H/'intake-source-binding.json',{'head':head,'base':R['base'],'source_commit':R['commit'],'source':path,'preimage_blob':oldblob,'postimage_blob':newblob,'patch_sha256':hashlib.sha256(git('diff','--binary',R['base'])).hexdigest(),'phase1_diagnostic_sha256':sha(diagnostic)})
 e=module('R15_repair_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=R['base'];snapshot=e.common.source_snapshot();previous=a['source_snapshot'];changed,affected=e.closure(previous,snapshot);assert changed=={path}and len(snapshot['hashes'])==6303 and set(snapshot['hashes'])==set(previous['hashes'])
 native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);assert native==read(F/'source-metadata.json')['native_inventory']and len(native)==3616
 oldincludes=read(F/'actual-includes.json');target=str((W/path).resolve()).lower();actualconsumers={p for p,hdrs in oldincludes['rows'].items()if any(h.lower()==target for h in hdrs)};sources=sorted((affected&set(native))|actualconsumers|{path});assert len(sources)<3616
 external=e.external_inputs(full)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==a['external_inputs'][k]
 incoming=[]
 for d,t in external['include_trees'].items():
  for rel in t['files']:
   p=Path(d)/rel
   if p.suffix.lower()not in e.common.TEXT_EXT:continue
   for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if Path(match[1].replace('\\','/')).name.lower()==Path(path).name.lower():incoming.append({'source':str(p),'include':match[1]})
 assert not incoming,('Unexpected external CPP inclusion requires expanded evidence',incoming)
 save(H/'affected-repair-closure.json',{'head':head,'base':R['base'],'changed_CPP':[path],'static_affected_native':sorted(affected&set(native)),'phase1_actual_CPP_include_consumers':sorted(actualconsumers),'selected_sources':sources,'external_CPP_include_edges':incoming,'all_other6302_text_inputs_and_all_headers_unchanged':True,'native_inventory_equal':True,'phase1_actual_include_trace_sha256':sha(F/'actual-includes.json')})
 out=H/'native-repair';temp=H/'native-repair-temp';temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(F/'full-native-command.json')['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[arg for arg in full['results'][0]['command']if arg.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',full['sdk'],'--out',str(out),'--jobs','1','--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 save(H/'source-metadata.json',{'head':head,'base':R['base'],'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':[path],'sha256':hashlib.sha256(git('diff','--binary',R['base'])).hexdigest(),'source_sha256':{path:sha(W/path)}});save(H/'command.json',{'head':head,'argv':argv,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'TEMP_TMP':str(temp),'canonical_fallbacks':9});save(H/'external-before.json',external);save(H/'operation.json',{'phase':'targeted successor repair','head':head,'owned_pid':os.getpid(),'sources':sources});print('FROZEN_R15_REPAIR',head,'sources',len(sources),'PID',os.getpid(),flush=True)
 d=module('R15_repair_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');save(H/'native-repair-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json')});assert set(idx)==set(sources)and code==0 and new['failed']==0
 old={r['source']:r for r in full['results']}
 def flags(r):return [x for x in r['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes'];guards(head);after=e.external_inputs(new)
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
 save(H/'actual-includes.json',{'head':head,'rows':actual,'all_actual_headers_bound':True,'unique_headers':len({p for h in actual.values()for p in h})})
 safeguards=read(F/'named-safeguard-results.json');assert not safeguards['regressions']
 for r in safeguards['rows']:assert not any(p.lower()==target for p in r['actual_included_headers'])
 save(H/'reuse-proof.json',{'head':head,'base':R['base'],'fresh_repair_sources':sources,'fresh_repair_count':len(sources),'retained_phase1_rows':3616-len(sources),'phase1_full_actual_artifacts_already_verified_once':True,'phase1_full_audit_sha256':sha(F/'full-native-artifact-audit.json'),'phase1_raw_report_sha256':sha(a['report']),'raw_phase1_failure_preserved':True,'only_one_CPP_changed_no_header_change':True,'all_other6302_inputs_unchanged':True,'actual_CPP_include_trace_and_static_graph_checked':True,'external_tools_flags_submodules_stable':True,'named_safeguards_unaffected_by_CPP_delta':True,'named_safeguards_receipt_sha256':sha(F/'named-safeguard-results.json'),'no_full_replay_or_historical_products_rehash':True})
 rows=[];newsha=sha(out/'compile-results.json')
 for row in full['results']:
  if row['source']in idx:value={**idx[row['source']],'build_head':head,'origin_kind':'fresh R15 authorized successor repair','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:value={**row,'build_head':R['base'],'origin_kind':'retained verified R15 phase1 full-sweep row','origin_report':a['report'],'origin_report_sha256':sha(a['report'])}
  rows.append(value)
 composite={k:v for k,v in full.items()if k!='results'};composite.update(state='complete',requested=3616,results=rows,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'R15 phase1 freshfull3616 plus fresh targeted successor repair;current tree is COMPOSITE','source_head':head,'phase1_attempts':3616,'fresh_successor_attempts':len(sources),'current_reused_phase1_rows':3616-len(sources),'current_fresh_successor_rows':len(sources),'raw_phase1_failure_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')};save(H/'composite-effective.json',composite)
 baseline=read(C['baseline_report']);summary=e.compare(composite,baseline);reg=summary['regressions'];pending=bool(reg or summary['timeouts']);manifest={**a['artifact_manifest'],**newart};audit={'head':head,'base':R['base'],'R14_base':C['base'],'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'summary_versus_R14':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'phase1_full_audit_sha256':sha(F/'full-native-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 candidate=read(C['plan']);currentidx={r['source']:r for r in rows};owned={p:currentidx[p]['passed']for p in candidate['changed_CPP']};crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];scope=git('diff','--name-only',C['base']).decode().splitlines();assert set(scope)==set(candidate['paths'])|{path}
 seal={'head':head,'base':C['base'],'phase1_head':R['base'],'status':'PENDING_ADDITIONAL_REGRESSION_FIXES'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'requested':3616,'passed':composite['passed'],'failed':composite['failed'],'actual_phase1_full_attempts':3616,'actual_fresh_successor_attempts':len(sources),'current_phase1_rows_retained':3616-len(sources),'current_fresh_successor_rows':len(sources),'R14_prior_passes':3342,'R14_prior_passes_retained':summary['prior_passes_retained'],'gains':len(summary['new_passes']),'gain_sources':summary['new_passes'],'regressions':reg,'timeouts':summary['timeouts'],'all40_original_candidate_CPP_results':owned,'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':15,'authorized_caller_repair_commits':1,'cumulative_unique_paths':len(scope),'cumulative_changed_CPP':41,'cumulative_changed_headers':12,'new_headers':candidate['new_headers'],'text_inputs':6303,'native_inventory':3616,'artifact_hash_entries':sum(len(x)for x in manifest.values()),'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'phase1_diagnostic':str(diagnostic),'phase1_diagnostic_sha256':sha(diagnostic),'phase1_full_raw_report_sha256':sha(a['report']),'repair_report_sha256':newsha,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'named_safeguards_sha256':sha(F/'named-safeguard-results.json'),'phase1_compiler_exit':phase1['compiler_exit'],'repair_compiler_exit':0,'binder_exit':0,'limits':phase1['limits'],'scope':'One full3616 phase1 sweep plus freshly compiled successor affected source(s). This repaired tree is explicitly composite. All prior full failed artifacts/diagnostic retained. No optional full replay,link/CRT/runtime/semantic parity/publication claim.','canonical_head':C['published_base'],'parent_gitlink_index':candidate['parent_gitlink_index'],'publication_allowed':False,'goal':'ACTIVE','created_utc':now()};filename='REGRESSION_DIAGNOSTIC_R15_REPAIR.json'if pending else 'FROZEN_REPAIRED_COMPOSITE_DIAGNOSTIC_R15.json';save(H/filename,seal)
 if not pending:
  assert summary['prior_passes_retained']==3342;ref='codex/experimental-x360-native-r15-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  seal['local_checkpoint']=ref
 result={'head':head,'status':seal['status'],'requested':3616,'passed':composite['passed'],'failed':composite['failed'],'phase1_full_attempts':3616,'successor_fresh_attempts':len(sources),'current_fresh_successor_rows':len(sources),'current_retained_phase1_rows':3616-len(sources),'R14_prior_passes_retained':summary['prior_passes_retained'],'gains':len(summary['new_passes']),'regressions':len(reg),'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-effective.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'compiler_exit':0,'binder_exit':0,'goal':'ACTIVE'}
 if not pending:result['local_checkpoint']=seal['local_checkpoint']
 save(H/'completion.json',result);(H/'operation.json').write_text(json.dumps({'phase':seal['status'],'head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':result},indent=2)+'\n',encoding='utf-8');guards(head);print(json.dumps(result,indent=2),flush=True)
if __name__=='__main__':main()
