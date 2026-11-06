"""Root-gated exact R13 collection,48 native compiles,and single-pass baseline artifact binding."""
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

def closure(snapshot,previous,external,native,baseline):
 nodes={};byname=defaultdict(set);absolute={};graph=defaultdict(set);dynamic=[]
 for p in snapshot['hashes']:
  key='T:'+p;path=W/p;nodes[key]=path;byname[path.name.lower()].add(key);absolute[str(path.resolve()).lower()]=key
 for directory,tree in external['include_trees'].items():
  for rel in tree['files']:
   path=Path(directory)/rel
   if path.suffix.lower()not in ['.cpp','.c','.h','.hpp','.inl','.inc','.ipp']:continue
   key='E:'+str(path.resolve());nodes[key]=path;byname[path.name.lower()].add(key);absolute[str(path.resolve()).lower()]=key
 for g in [snapshot['dependencies'],previous['dependencies']]:
  for parent,children in g.items():graph['T:'+parent].update('T:'+p for p in children)
 texts={}
 for key,path in nodes.items():
  text=path.read_text(encoding='utf-8-sig',errors='replace');texts[key]=text
  for directive in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',text,re.M):
   m=re.match(r'[<"]([^>"\r\n]+)[>"]',directive)
   if not m:dynamic.append({'node':key,'directive':directive});continue
   inc=m[1].replace('\\','/');graph[key].update(byname.get(Path(inc).name.lower(),[]));local=str((path.parent/inc).resolve()).lower()
   if local in absolute:graph[key].add(absolute[local])
 unknown=[]
 for d in dynamic:
  token=d['directive'].strip()
  if token not in ['LUA_USER_H','EABASE_USER_HAVE_HEADER','EASTL_USER_CONFIG_HEADER']:unknown.append(d);continue
  defs=[k for k,text in texts.items()if re.search(r'^\s*#\s*define\s+'+token+r'\b',text,re.M)]
  cmds=[r['source']for r in baseline['results']if any(re.search(r'/D'+token+r'(?:[= ]|$)',arg,re.I)for arg in r['command'])]
  if defs or cmds:unknown.append({**d,'definitions':defs,'command_definitions':cmds})
 reverse=defaultdict(set)
 for parent,children in graph.items():
  for child in children:reverse[child].add(parent)
 reached={'T:'+p for p in plan['paths']};q=deque(reached)
 while q:
  for p in reverse[q.popleft()]:
   if p not in reached:reached.add(p);q.append(p)
 computed={p for p in native if 'T:'+p in reached};pre=set(plan['expected_preflight_sources']);selected=sorted(set(native)if unknown else computed|pre)
 assert pre<=set(selected)and set(plan['changed_CPP'])<=set(selected)
 return selected,{'method':'Union old/new tracked conservative graph plus post-header tracked/external literal basename graph;all inactive branches and basename candidates retained. Preflight1040 retained conservatively.','computed_native_consumers':len(computed),'preflight_native_consumers':len(pre),'selected_native_consumers':len(selected),'extra_computed_over_preflight':sorted(computed-pre),'preflight_retained_over_computed':sorted(pre-computed),'affected_external_nodes':[k[2:]for k in reached if k.startswith('E:')],'nonliteral_sites':dynamic,'unbounded_dynamic_sites':unknown,'fallback_to_full':bool(unknown),'no_new_or_removed_sources':True}

def named_gates(e,head,snapshot,external,new):
 old=read(C['baseline_named_gates']);changed={str((W/p).resolve()).lower()for p in plan['paths']};out=H/'affected-named-gates';out.mkdir();temp=out/'temp';temp.mkdir();rows=[];facts={};reg=[]
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
   rows.append({**r,'coverage_role':'unchanged historical native safeguard retained','original_head':old['head'],'changed_inputs':[]});continue
  name=r['name'];cpp,obj,asm,log=[out/(name+ext)for ext in ['.cpp','.obj','.asm','.log']];cpp.write_bytes(Path(r['source']).read_bytes());cmd=[sample['command'][0],*flags,'/Fo'+str(obj),'/Fa'+str(asm),str(cpp)]
  try:run=subprocess.run(cmd,cwd=W,env=env,capture_output=True,timeout=180);rc=run.returncode;log.write_bytes(run.stdout+run.stderr)
  except subprocess.TimeoutExpired as exc:rc=-1;log.write_bytes((exc.stdout or b'')+(exc.stderr or b''))
  diagnostics=[line for line in log.read_text(errors='replace').splitlines()if 'error C'in line];ok=rc==0 and obj.exists()and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2;neg=r['expected_negative'];satisfied=(rc!=0 and rc!=-1 and len(diagnostics)==1 and 'C2118'in diagnostics[0]and not obj.exists())if neg else ok
  hdrs={}
  for line in log.read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p.lower()in allinputs,(name,p);hdrs[p]=allinputs[p.lower()]
  if ok and name in symbols:
   facts[name]=words(obj,*symbols[name]);satisfied=satisfied and facts[name]==old['emitted_facts'][name]
  row={'name':name,'command':cmd,'returncode':rc,'passed':ok,'expected_negative':neg,'gate_satisfied':satisfied,'source':str(cpp),'source_sha256':sha(cpp),'object':str(obj)if obj.exists()else None,'artifacts':{str(p):sha(p)for p in [cpp,obj,asm,log]if p.exists()},'actual_included_headers':hdrs,'diagnostics':diagnostics,'changed_inputs':hits,'coverage_role':'fresh affected safeguard at R14'};rows.append(row)
  if not satisfied:reg.append({'name':name,'returncode':rc,'diagnostics':diagnostics,'log':str(log),'facts':facts.get(name),'previous_facts':old['emitted_facts'].get(name)})
  print('AFFECTED_SAFEGUARD',name,rc,'satisfied',satisfied,flush=True)
 report={'head':head,'original_historical_head':old['head'],'rows':rows,'fresh_affected_count':sum(bool(r['changed_inputs'])for r in rows),'retained_unaffected_count':sum(not r['changed_inputs']for r in rows),'emitted_facts':facts,'regressions':reg,'scope':'Only actually affected native/header safeguards rerun. Expected independent C2118 negatives remain actual failures,not repaired PASS claims.','created_utc':now()};save(H/'named-safeguard-results.json',report);return report

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--jobs',type=int,default=8);args=ap.parse_args();assert 1<=args.jobs<=8
 state=read(H/'intake-receipts.json');head=state['head'];assert state['state']=='complete'and len(state['receipts'])==4;guards(head)
 authorization=read(H/'R14_ROOT_AUTHORIZATION.json');assert authorization['root_explicit_authorization']and authorization['commits']==plan['commits']and authorization['plan_sha256']==sha(C['plan'])==C['plan_sha256']
 assert sha(plan['preflight'])==plan['preflight_sha256']=='b63a7a66e134ae3d2f563a49342e3b7e32456204204b767c7ccc95624c437987'
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:assert sha(C[k])==C[k+'_sha256']
 e=module('R14_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];audit=read(C['baseline_artifact_manifest']);snapshot=e.common.source_snapshot();previous=audit['source_snapshot'];native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp'])
 assert len(snapshot['hashes'])==6302 and len(native)==3616 and native==read(C['baseline_metadata'])['native_inventory']
 changed={p for p in set(snapshot['hashes'])|set(previous['hashes'])if snapshot['hashes'].get(p)!=previous['hashes'].get(p)};assert changed==set(plan['paths'])and set(snapshot['hashes'])==set(previous['hashes'])
 baseline,old,oldart=e.common.report_check(Path(C['baseline_report']));assert oldart==audit['artifact_manifest']and baseline['passed']==3333 and baseline['failed']==283
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==audit['external_inputs'][k]
 sources,closureproof=closure(snapshot,previous,external,native,baseline);save(H/'post-header-consumer-closure.json',{'head':head,'base':C['base'],'sources':sources,'fresh_count':len(sources),'retained_count':3616-len(sources),'changed_paths':sorted(changed),**closureproof})
 save(H/'baseline-input-artifact-verification.json',{'head':head,'base':C['base'],'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'baseline_actual_artifacts_verified_once':True,'artifact_hash_entries':sum(len(x)for x in oldart.values()),'changed17paths':sorted(changed),'all_other6285_text_inputs_unchanged':True,'external_include_compiler_runtime_submodules_equal':True,'historical_published_comparison_performed':False})
 out=H/'native-consumers';temp=H/'native-consumers-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(args.jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 patch=git('diff','--binary',C['base']);meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(patch).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':plan['paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'selection':'fresh Physics affected-header closure,no optional full replay','created_utc':now()};save(H/'source-metadata.json',meta);save(H/'native-consumers-command.json',{'head':head,'argv':argv,'jobs':args.jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'native affected consumers compile','head':head,'owned_pid':os.getpid(),'jobs':args.jobs,'live_report':str(out/'compile-results.json'),'fresh_count':len(sources)})
 print('FROZEN_R14',head,'fresh',len(sources),'retained',3616-len(sources),'jobs',args.jobs,'PID',os.getpid(),'baseline artifacts verified once',flush=True)
 d=module('R14_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(sources)
 save(H/'native-consumers-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()})
 after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after})
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
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_actual_includes_frozen':True})
 def flags(row):return [x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 safeguards=named_gates(e,head,snapshot,external,new)
 # Necessary post-operation stability check; no repeated global graph or product rehash.
 assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes'];assert set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes']);guards(head)
 proof={'head':head,'base':C['base'],'fresh':len(sources),'retained':3616-len(sources),'changed_paths':sorted(changed),'unchanged_text_inputs':6302-len(changed),'consumer_closure_sha256':sha(H/'post-header-consumer-closure.json'),'all_affected_native_consumers_recompiled':True,'outside_closure_inputs_unchanged':True,'all9canonical_fallbacks':True,'actual_flags_equal_baseline_except_showIncludes_outputs':True,'actual_includes_sha256':sha(H/'actual-includes.json'),'baseline_artifact_verification_sha256':sha(H/'baseline-input-artifact-verification.json'),'baseline_artifacts_hashed_once':True,'fresh_products_hashed_once':True,'source_tool_external_submodule_pre_post_stable':True,'named_safeguard_report_sha256':sha(H/'named-safeguard-results.json'),'no_historical_published_comparison':True,'created_utc':now()};save(H/'reuse-proof.json',proof)
 rows=[];newhash=sha(out/'compile-results.json')
 for row in baseline['results']:
  if row['source']in idx:value={**idx[row['source']],'build_head':head,'origin_kind':'fresh R14 affected-header native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newhash}
  else:
   value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'verified unaffected R13 coverage row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite={k:v for k,v in baseline.items()if k!='results'};composite.update(state='complete',results=rows,requested=3616,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'Complete-coverage COMPOSITE,not a fresh full sweep','source_head':head,'fresh':len(sources),'retained':3616-len(sources),'prior_R11_R12_R13_actual_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')};summary=e.compare(composite,baseline);assert not summary['new_sources']and not summary['removed_sources'];save(H/'composite-effective.json',composite);manifest={**oldart,**newart}
 terminalaudit={'head':head,'base':C['base'],'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'actual_native_report_sha256':newhash,'fresh_actual_compiles':len(sources),'retained_actual_R13_rows':3616-len(sources),'baseline_actual_artifacts_verified_once':True,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',terminalaudit)
 regressions=[]
 for p in summary['regressions']:
  row=idx[p];diagnostics=[line for line in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in line or 'fatal error'in line or 'timeout'in line.lower()];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':diagnostics})
 freshfail=[p for p,r in idx.items()if not r['passed']];crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];states={k:{'head':git('rev-parse','HEAD',cwd=Path(C[k])).decode().strip(),'status':git('status','--porcelain=v1',cwd=Path(C[k])).decode().strip()}for k in ['workspace','canonical']};pending=bool(regressions or safeguards['regressions']or summary['timeouts']);guards(head)
 result={'head':head,'base':C['base'],'status':'PENDING_SOURCE_FIXES'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':composite['provenance']['kind'],'requested':3616,'passed':composite['passed'],'failed':composite['failed'],'fresh_actual_compiles':len(sources),'fresh_passed':new['passed'],'fresh_failed':new['failed'],'fresh_failed_sources':freshfail,'retained_R13_coverage_rows':3616-len(sources),'R13_prior_passes_retained':summary['prior_passes_retained'],'R13_prior_passes':3333,'gains':len(summary['new_passes']),'gain_sources':summary['new_passes'],'regressions':regressions,'timeouts':summary['timeouts'],'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':4,'changed_paths':17,'changed_CPP':10,'changed_headers':7,'inventory_change':False,'text_inputs':6302,'unchanged_inputs':6285,'artifact_hash_entries':sum(len(x)for x in manifest.values()),'baseline_artifact_entries_verified_once':sum(len(x)for x in oldart.values()),'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':args.jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(H/'composite-effective.json'),'report_sha256':sha(H/'composite-effective.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'post_header_closure_sha256':sha(H/'post-header-consumer-closure.json'),'preflight_sha256':plan['preflight_sha256'],'named_safeguards':{'fresh_affected':safeguards['fresh_affected_count'],'retained_unaffected':safeguards['retained_unaffected_count'],'regressions':safeguards['regressions'],'report_sha256':sha(H/'named-safeguard-results.json')},'canonical_and_intake':states,'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['limits'],'scope':'Actual header-consumer native gate and verified unaffected composite. Pre-existing selected failures remain actual FAIL;expected independent safeguard negatives remain separately labelled. No optional full/link/published-history/owner suite,runtime/byte-parity/image/launch or publication claim.','excluded':'Streams97ba20 and all sound/GUI/AI/later candidates.','publication_allowed':False,'formal_review':'Bounded successor review pass only;no full parity review claim.','goal':'ACTIVE;native source/provider/link/image/launch remains','created_utc':now()}
 filename='REGRESSION_DIAGNOSTIC_R14.json'if pending else 'FROZEN_COMPOSITE_DIAGNOSTIC_R14.json';save(H/filename,result)
 if not pending:
  assert summary['prior_passes_retained']==3333
  ref='codex/experimental-x360-native-r14-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  result['local_checkpoint']=ref
 completion={'head':head,'status':result['status'],'requested':3616,'passed':composite['passed'],'failed':composite['failed'],'fresh':len(sources),'fresh_passed':new['passed'],'fresh_failed':new['failed'],'retained':3616-len(sources),'prior_passes_retained':summary['prior_passes_retained'],'gains':len(summary['new_passes']),'regressions':len(regressions),'safeguard_regressions':len(safeguards['regressions']),'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(H/'composite-effective.json'),'audit_sha256':sha(H/'composite-artifact-audit.json'),'goal':'ACTIVE'}
 if not pending:completion['local_checkpoint']=result['local_checkpoint']
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':'R14 pending regression repairs'if pending else 'R14 composite sealed','head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)
if __name__=='__main__':main()
