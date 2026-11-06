"""Root-gated exact R15 collection and one fresh full3616 native sweep."""
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
   facts[name]=words(obj,*symbols[name]);satisfied=satisfied and facts[name]==old['emitted_facts'][name]
  row={'name':name,'command':cmd,'returncode':rc,'passed':ok,'expected_negative':neg,'gate_satisfied':satisfied,'source':str(cpp),'source_sha256':sha(cpp),'object':str(obj)if obj.exists()else None,'artifacts':{str(p):sha(p)for p in [cpp,obj,asm,log]if p.exists()},'actual_included_headers':hdrs,'diagnostics':diagnostics,'changed_inputs':hits,'coverage_role':'fresh affected safeguard at R14'};rows.append(row)
  if not satisfied:reg.append({'name':name,'returncode':rc,'diagnostics':diagnostics,'log':str(log),'facts':facts.get(name),'previous_facts':old['emitted_facts'].get(name)})
  print('AFFECTED_SAFEGUARD',name,rc,'satisfied',satisfied,flush=True)
 report={'head':head,'original_historical_head':old['head'],'rows':rows,'fresh_affected_count':sum(bool(r['changed_inputs'])for r in rows),'retained_unaffected_count':sum(not r['changed_inputs']for r in rows),'emitted_facts':facts,'regressions':reg,'scope':'Only actually affected native/header safeguards rerun. Expected independent C2118 negatives remain actual failures,not repaired PASS claims.','created_utc':now()};save(H/'named-safeguard-results.json',report);return report


def authorize():
 a=read(H/'R15_ROOT_AUTHORIZATION.json');assert a['root_explicit_authorization']and a['base']==C['base']and a['commits']==plan['commits']and a['plan_sha256']==sha(C['plan'])==C['plan_sha256']
 assert sha(C['preflight'])==a['preflight_sha256'];pf=read(C['preflight']);assert pf['manifest_sha256']==plan['manifest_sha256']==sha(plan['manifest'])and pf['baseline_commit']==C['base']and not pf['issues']and pf['all_first_preimages_match_baseline']and pf['successor_chains_valid']
 for r in plan['receipts']:assert sha(r['path'])==r['sha256']
 assert sha(plan['review'])==plan['review_sha256']
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:assert sha(C[k])==C[k+'_sha256']
 return a,pf

def collect():
 guards(C['base']);path=H/'intake-receipts.json';state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy()};save(path,state)
 for c in plan['candidates']:
  guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent']
  paths=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(paths)==sorted(d['path']for d in c['paths'])
  for d in c['paths']:
   if d['preimage']is None:
    assert d['status']=='A'and d['path']in plan['new_headers'];r=subprocess.run(['git','cat-file','-e','HEAD:'+d['path']],cwd=W,capture_output=True);assert r.returncode!=0
   else:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage']
  r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
  if r.returncode:
   state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected collection conflict;no automatic resolution: '+r.stderr)
  state['head']=git('rev-parse','HEAD').decode().strip()
  for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage']
  state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
 guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['paths']
 for p,v in plan['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).decode().strip()==v['final']
 state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'exact15commits':plan['commits'],'paths':plan['paths'],'final52postimages_verified':True,'new_header':plan['new_headers'],'patch_sha256':hashlib.sha256(patch).hexdigest()})
 with(H/'integrated.diff').open('xb')as f:f.write(patch)
 return state

def run(state,a,pf,jobs):
 head=state['head'];e=module('R15_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];guards(head)
 oldaudit=read(C['baseline_artifact_manifest']);baseline=read(C['baseline_report']);old={r['source']:r for r in baseline['results']};assert baseline['passed']==3342 and baseline['failed']==274 and len(old)==3616
 # Every current native source will compile fresh. Historical product rehash is unnecessary.
 snapshot=e.common.source_snapshot();native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp']);previous=oldaudit['source_snapshot'];changed={p for p in set(snapshot['hashes'])|set(previous['hashes'])if snapshot['hashes'].get(p)!=previous['hashes'].get(p)}
 assert len(snapshot['hashes'])==6303 and changed==set(plan['paths'])and set(snapshot['hashes'])-set(previous['hashes'])==set(plan['new_headers'])and not set(previous['hashes'])-set(snapshot['hashes'])
 assert len(native)==3616 and native==read(C['baseline_metadata'])['native_inventory']and set(native)==set(old)
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==oldaudit['external_inputs'][k]
 out=H/'full-native';temp=H/'full-native-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in native:argv+=['--source',p]
 patch=git('diff','--binary',C['base']);meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(patch).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':native,'paths':plan['paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'selection':'ONE fresh full3616 sweep;no composite/reuse','created_utc':now()};save(H/'source-metadata.json',meta);save(H/'full-native-command.json',{'head':head,'argv':argv,'jobs':jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9});save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'one full native sweep','head':head,'owned_pid':os.getpid(),'jobs':jobs,'live_report':str(out/'compile-results.json'),'fresh_count':3616})
 print('FROZEN_R15',head,'ONE FULL3616','jobs',jobs,'PID',os.getpid(),'text6303',flush=True)
 d=module('R15_actual_native_driver',driver);sys.argv=argv;code=d.main();new,idx,art=e.common.report_check(out/'compile-results.json');assert set(idx)==set(native)
 save(H/'full-native-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()})
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
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_actual_includes_frozen_before_and_unchanged_after':True})
 def flags(row):return [x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 safeguards=named_gates(e,head,snapshot,external,new)
 assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes'];assert set(p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT)==set(snapshot['hashes']);guards(head)
 summary=e.compare(new,baseline);assert not summary['new_sources']and not summary['removed_sources'];regressions=[]
 for p in summary['regressions']:
  row=idx[p];diagnostics=[line for line in Path(row['log']).read_text(errors='replace').splitlines()if 'error C'in line or 'fatal error'in line or 'timeout'in line.lower()];regressions.append({'source':p,'baseline_passed':True,'returncode':row['returncode'],'log':row['log'],'diagnostics':diagnostics})
 audit={'head':head,'base':C['base'],'coverage_kind':'ONE fresh full3616 native sweep;no reused compile rows','report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'summary':summary,'artifact_manifest':art,'source_snapshot':snapshot,'external_inputs':external,'source_metadata_sha256':sha(H/'source-metadata.json'),'full_command_sha256':sha(H/'full-native-command.json'),'actual_include_trace_sha256':sha(H/'actual-includes.json'),'named_safeguards_sha256':sha(H/'named-safeguard-results.json'),'baseline_report_sha256':C['baseline_report_sha256'],'historical_products_rehashed':False,'historical_published_comparison':False,'goal':'ACTIVE','created_utc':now()};save(H/'full-native-artifact-audit.json',audit)
 states={k:{'head':git('rev-parse','HEAD',cwd=Path(C[k])).decode().strip(),'status':git('status','--porcelain=v1',cwd=Path(C[k])).decode().strip()}for k in ['workspace','canonical']};crows=[r for r in new['results']if Path(r['source']).suffix.lower()=='.c'];owned={p:idx[p]['passed']for p in plan['changed_CPP']};pending=bool(regressions or safeguards['regressions']or summary['timeouts']);guards(head)
 result={'head':head,'base':C['base'],'status':'PENDING_CALLER_FIXES'if pending else 'SEALED_NO_REGRESSIONS','coverage_kind':audit['coverage_kind'],'requested':3616,'passed':new['passed'],'failed':new['failed'],'fresh_actual_compiles':3616,'reused_rows':0,'R14_prior_passes':3342,'R14_prior_passes_retained':summary['prior_passes_retained'],'gains':len(summary['new_passes']),'gain_sources':summary['new_passes'],'regressions':regressions,'timeouts':summary['timeouts'],'all40_candidate_CPP_results':owned,'candidate_CPP_passed':sum(owned.values()),'candidate_CPP_failed':sum(not v for v in owned.values()),'native_C_requested':len(crows),'native_C_passed':sum(r['passed']for r in crows),'original_commits':15,'groups':10,'changed_paths':52,'changed_CPP':40,'changed_headers':12,'new_headers':plan['new_headers'],'text_inputs':6303,'native_inventory':3616,'native_inventory_change':False,'artifact_hash_entries':sum(len(v)for v in art.values()),'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':jobs,'compiler_exit':code,'binder_exit':0,'operation_pid':os.getpid(),'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'artifact_audit':str(H/'full-native-artifact-audit.json'),'artifact_audit_sha256':sha(H/'full-native-artifact-audit.json'),'metadata_sha256':sha(H/'source-metadata.json'),'preflight_sha256':a['preflight_sha256'],'plan_sha256':C['plan_sha256'],'bounded_review_sha256':plan['review_sha256'],'named_safeguards':{'fresh_affected':safeguards['fresh_affected_count'],'retained_unaffected':safeguards['retained_unaffected_count'],'regressions':safeguards['regressions'],'report_sha256':sha(H/'named-safeguard-results.json')},'canonical_and_intake':states,'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['limits']+pf['limits'],'scope':'All3616 actual native products/logs bound to final6303-source/header inputs. Existing failures remain actual FAIL. No published-history artifact rehash,optional owner/link suite,full semantic/numerical parity,runtime/GPU/image/launch or publication claim.','excluded':'All nonmanifest candidates and private preparation stream copies.','publication_allowed':False,'formal_review':'Bounded changed-source review passed;no exhaustive parity verdict.','goal':'ACTIVE;native source/provider/link/image/launch remains','created_utc':now()}
 filename='REGRESSION_DIAGNOSTIC_R15.json'if pending else 'FROZEN_FULL_DIAGNOSTIC_R15.json';save(H/filename,result)
 if not pending:
  assert summary['prior_passes_retained']==3342;ref='codex/experimental-x360-native-r15-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
  if existing:assert existing==head
  else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
  result['local_checkpoint']=ref
 completion={'head':head,'status':result['status'],'requested':3616,'passed':new['passed'],'failed':new['failed'],'fresh':3616,'reused':0,'prior_passes_retained':summary['prior_passes_retained'],'gains':len(summary['new_passes']),'regressions':len(regressions),'safeguard_regressions':len(safeguards['regressions']),'compiler_exit':code,'binder_exit':0,'diagnostic_file':str(H/filename),'diagnostic_sha256':sha(H/filename),'report_sha256':sha(out/'compile-results.json'),'audit_sha256':sha(H/'full-native-artifact-audit.json'),'goal':'ACTIVE'}
 if not pending:completion['local_checkpoint']=result['local_checkpoint']
 save(H/'completion.json',completion);(H/'operation.json').write_text(json.dumps({'phase':'R15 pending necessary caller fixes'if pending else 'R15 full diagnostic sealed','head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':completion},indent=2)+'\n',encoding='utf-8');print(json.dumps(completion,indent=2),flush=True)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--collect-build',action='store_true');ap.add_argument('--jobs',type=int,default=8);args=ap.parse_args()
 if not args.collect_build:ap.error('Separate Root authorization and explicit --collect-build required.')
 assert 1<=args.jobs<=8;a,pf=authorize();state=collect();run(state,a,pf,args.jobs)
if __name__=='__main__':main()
