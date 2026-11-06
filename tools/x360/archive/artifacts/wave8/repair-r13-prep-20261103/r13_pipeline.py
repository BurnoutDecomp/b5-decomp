"""Root-gated exact R13 collection,48 native compiles,and single-pass baseline artifact binding."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,re,subprocess,sys
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
def auth():
 a=read(C['authorization_required']);assert a['root_explicit_authorization']and a['base']==C['base']and a['plan_sha256']==C['plan_sha256']==sha(C['plan'])and a['commits']==plan['collect_now']
 assert sha(a['preflight'])==a['preflight_sha256']=='5d9f1a02aa6ba643a898bacf2c1f9f8d43010fc960343c85c09287f11a465b4f'
 pf=read(a['preflight']);assert pf['intake']==C['base']and pf['unique_cpp_count']==48 and len(pf['groups'])==11
 assert set().union(*(set(g['paths'])for g in pf['groups'].values()))==set(plan['unique_paths'])
 for g in pf['groups'].values():
  last=next(c for c in plan['candidates']if c['commit']==g['commit']);assert last['receipt_sha256']==g['receipt_sha256']
 for c in plan['candidates']:assert sha(c['receipt'])==c['receipt_sha256']
 for k in ['baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_safeguard_receipt']:assert sha(C[k])==C[k+'_sha256'],k
 return a,pf

def collect(a):
 guards(C['base']);state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['collect_now'].copy(),'authorization_sha256':sha(C['authorization_required'])};path=H/'intake-receipts.json';save(path,state)
 for c in plan['candidates']:
  guards(state['head'])
  assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent']
  actualpaths=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(actualpaths)==sorted(d['path']for d in c['paths'])
  for d in c['paths']:assert d['status']=='M'and d['path'].endswith('.cpp')and git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage']
  r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
  if r.returncode:
   state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Source collection conflict;no automatic resolution: '+r.stderr)
  state['head']=git('rev-parse','HEAD').decode().strip()
  for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage']
  state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']],'receipt_sha256':c['receipt_sha256']});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
 guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['unique_paths']
 for p,d in plan['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).decode().strip()==d['final']
 state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base'])
 save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'commits':14,'CPP':48,'paths':plan['unique_paths'],'all_final_postimages_verified':True,'patch_sha256':hashlib.sha256(patch).hexdigest(),'authorization_sha256':sha(C['authorization_required'])})
 with(H/'integrated.diff').open('xb')as f:f.write(patch)
 return state

def run(a,pf,state,jobs):
 e=module('R13_native_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];head=state['head'];guards(head)
 baseaudit=read(C['baseline_artifact_manifest']);basemeta=read(C['baseline_metadata']);snapshot=e.common.source_snapshot();native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in ['.c','.cpp'])
 assert len(snapshot['hashes'])==6302 and native==basemeta['native_inventory']and len(native)==3616
 previous=baseaudit['source_snapshot'];changed={p for p in set(snapshot['hashes'])|set(previous['hashes'])if snapshot['hashes'].get(p)!=previous['hashes'].get(p)};assert changed==set(plan['unique_paths'])and set(snapshot['hashes'])==set(previous['hashes'])
 incoming={}
 for graph in [snapshot['dependencies'],previous['dependencies']]:
  for p,children in graph.items():
   edge=set(children)&changed
   if edge:incoming[p]=sorted(edge)
 assert not incoming,('Changed CPP included by another input',incoming)
 assert snapshot['nonliteral_includes']==previous['nonliteral_includes']==[{'path':'vendor/lua/src/lua.h','directive':'LUA_USER_H'}]
 # Read/hash each current baseline product exactly once; no historical Wave7 manifest is involved.
 baseline,old,oldart=e.common.report_check(Path(C['baseline_report']));assert oldart==baseaudit['artifact_manifest'];assert baseline['passed']==3285 and baseline['failed']==331
 assert set(old)==set(native)and all(not old[p]['passed']for p in changed)
 assert not any(re.search(r'/[DU]LUA_USER_H(?:[= ]|$)',arg,re.I)for row in baseline['results']for arg in row['command'])
 assert not any(re.search(r'^\s*#\s*define\s+LUA_USER_H\b',(W/p).read_text(encoding='utf-8-sig',errors='replace'),re.M)for p in snapshot['hashes'])
 external=e.external_inputs(baseline)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==baseaudit['external_inputs'][k]
 names={Path(p).name.lower()for p in changed};cppincludes=[]
 scan=[(W/p)for p in snapshot['hashes']]+[Path(d)/p for d,t in external['include_trees'].items()for p in t['files']if Path(p).suffix.lower()in e.common.TEXT_EXT]
 for p in scan:
  for m in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
   inc=m[1].replace('\\','/');assert Path(inc).name.lower()not in names,(str(p),inc)
   if Path(inc).suffix.lower()in ['.c','.cpp']:cppincludes.append({'source':str(p),'include':inc})
 save(H/'baseline-input-artifact-verification.json',{'head':head,'base':C['base'],'baseline_report_sha256':C['baseline_report_sha256'],'baseline_audit_sha256':C['baseline_artifact_manifest_sha256'],'baseline_actual_artifacts_verified_once':True,'artifact_hash_entries':sum(len(x)for x in oldart.values()),'only48_CPP_changed':sorted(changed),'all_other6254_text_inputs_unchanged':True,'incoming_changed_CPP_include_edges':incoming,'unrelated_literal_C_CPP_includes':cppincludes,'nonliteral_include_scope':'Only LUA_USER_H,not defined in tracked sources or baseline command flags.','external_include_compiler_runtime_submodules_equal':True,'no_historical_published_manifest_read':True})
 sources=plan['selected_sources'];out=H/'native48';temp=H/'native48-temp';assert not out.exists()and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 driver=W/'tools/build_x360.py';assert sha(driver)==read(C['baseline_driver_command'])['driver_sha256']
 canonical=str(CAN).replace('\\','/').lower();fallback=[x for x in baseline['results'][0]['command']if x.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(out),'--jobs',str(jobs),'--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in sources:argv+=['--source',p]
 patch=git('diff','--binary',C['base']);meta={'head':head,'base':C['base'],'sha256':hashlib.sha256(patch).hexdigest(),'source_snapshot':snapshot,'native_inventory':native,'selected_sources':sources,'paths':plan['unique_paths'],'source_sha256':{p:snapshot['hashes'][p]for p in changed},'selection':'actual48 CPP;no full replay','created_utc':now()};save(H/'source-metadata.json',meta)
 save(H/'native48-command.json',{'head':head,'argv':argv,'jobs':jobs,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'TEMP_TMP':str(temp),'canonical_vendor_fallbacks':9})
 save(H/'external-inputs-before.json',{'head':head,'inputs':external});save(H/'operation.json',{'phase':'native48 compile','head':head,'owned_pid':os.getpid(),'jobs':jobs,'live_report':str(out/'compile-results.json')})
 print('FROZEN_R13',head,'native48','jobs',jobs,'PID',os.getpid(),'baseline artifacts verified once',flush=True)
 d=module('R13_native_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json')
 save(H/'native48-exit.json',{'head':head,'exit_code':code,'report':str(out/'compile-results.json'),'report_sha256':sha(out/'compile-results.json'),'owned_pid':os.getpid()})
 # Postcompile identity is necessary stability evidence; no repeated graph/global artifact pass.
 assert {p:sha(W/p)for p in snapshot['hashes']}==snapshot['hashes']
 postfiles=[p for p in e.common.inventory()if Path(p).suffix.lower()in e.common.TEXT_EXT];assert set(postfiles)==set(snapshot['hashes']);guards(head)
 after=e.external_inputs(new)
 for k in ['include_trees','submodules','compiler_and_runtime']:assert after[k]==external[k]
 save(H/'external-inputs-after.json',{'head':head,'inputs':after})
 allheaders={str((W/p).resolve()):h for p,h in snapshot['hashes'].items()}
 for directory,tree in external['include_trees'].items():
  for rel,h in tree['files'].items():allheaders[str((Path(directory)/rel).resolve())]=h
 actual={}
 for row in new['results']:
  found={}
  for line in Path(row['log']).read_text(errors='replace').splitlines():
   if 'including file:'in line:
    p=str(Path(line.split('including file:',1)[1].strip()).resolve());assert p in allheaders,('Unfrozen actual include',row['source'],p);found[p]=allheaders[p]
  actual[row['source']]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual,'unique_headers':len({p for h in actual.values()for p in h}),'all_headers_bound_before_and_unchanged_after':True})
 assert set(idx)==set(sources)and new['requested']==48
 if code or new['failed']:raise RuntimeError('Native48 has actual failures;stop without source fitting or composite seal.')
 def flags(row):return [x for x in row['command'][:-1]if not x.startswith(('/Fo','/Fa'))and x.lower()!='/showincludes']
 for p,row in idx.items():assert flags(row)==flags(old[p]),p
 proof={'head':head,'base':C['base'],'fresh':48,'retained':3568,'only_changed_CPP':sorted(changed),'all_other6254_inputs_unchanged':True,'no_incoming_changed_CPP_include_edges':True,'nonliteral_include_checked':True,'full9_canonical_fallbacks':True,'actual_flags_equal_baseline_except_showIncludes_outputs':True,'actual_includes_sha256':sha(H/'actual-includes.json'),'baseline_artifact_verification_sha256':sha(H/'baseline-input-artifact-verification.json'),'baseline_actual_artifacts_rehashed_once':True,'native48_artifacts_verified_once':True,'no_effective_manifest_rehash_or_historical_published_comparison':True,'source_tool_external_submodule_pre_post_stable':True,'created_utc':now()};save(H/'reuse-proof.json',proof)
 composite={k:v for k,v in baseline.items()if k!='results'};rows=[]
 for row in baseline['results']:
  if row['source']in idx:value={**idx[row['source']],'build_head':head,'origin_kind':'fresh R13 actual48 compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':sha(out/'compile-results.json')}
  else:
   value={**row,'retained_via_report':C['baseline_report'],'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'unchanged verified R12 coverage row retained'}
   for k in ['build_head','origin_report','origin_report_sha256']:assert value[k]==row[k]
  rows.append(value)
 composite.update(state='complete',results=rows,requested=3616,passed=sum(r['passed']for r in rows));composite['failed']=3616-composite['passed'];composite['provenance']={'kind':'Complete-coverage COMPOSITE,not a fresh full native sweep','source_head':head,'fresh':48,'retained':3568,'prior_actual_origins_preserved':True,'reuse_proof_sha256':sha(H/'reuse-proof.json')}
 summary=e.compare(composite,baseline);assert composite['passed']==3333 and composite['failed']==283 and summary['prior_passes_retained']==3285 and len(summary['new_passes'])==48 and not summary['regressions']and not summary['timeouts']and not summary['new_sources']and not summary['removed_sources']
 compositepath=H/'composite-effective.json';save(compositepath,composite);manifest={**oldart,**newart};assert len(manifest)==3616
 audit={'head':head,'base':C['base'],'report':str(compositepath),'report_sha256':sha(compositepath),'summary':summary,'artifact_manifest':manifest,'source_snapshot':snapshot,'external_inputs':external,'actual48_report_sha256':sha(out/'compile-results.json'),'fresh_actual_compiles':48,'retained_actual_R12_rows':3568,'baseline_artifacts_verified_once':True,'reuse_proof_sha256':sha(H/'reuse-proof.json'),'source_metadata_sha256':sha(H/'source-metadata.json'),'historical_published_comparison':'Not performed,unrelated to current coverage.','goal':'ACTIVE','created_utc':now()};save(H/'composite-artifact-audit.json',audit)
 crows=[r for r in rows if Path(r['source']).suffix.lower()=='.c'];assert len(crows)==42 and all(r['passed']for r in crows)
 states={k:{'head':git('rev-parse','HEAD',cwd=Path(C[k])).decode().strip(),'status':git('status','--porcelain=v1',cwd=Path(C[k])).decode().strip()}for k in ['workspace','canonical']};guards(head)
 seal={'head':head,'base':C['base'],'coverage_kind':composite['provenance']['kind'],'requested':3616,'passed':3333,'failed':283,'fresh_actual_compiles':48,'retained_R12_coverage_rows':3568,'all3285_R12passes_retained':True,'gains':48,'regressions':0,'timeouts':0,'native_C_requested':42,'native_C_passed':42,'groups':11,'original_commits':14,'changed_CPP':48,'changed_headers':0,'new_files':[],'inventory_change':False,'text_inputs':6302,'unchanged_inputs':6254,'artifact_hash_entries':sum(len(x)for x in manifest.values()),'baseline_artifact_entries_verified_once':sum(len(x)for x in oldart.values()),'actual_unique_headers':len({p for h in actual.values()for p in h}),'canonical_vendor_fallbacks':9,'jobs':jobs,'compiler_exit':0,'binder_exit':0,'operation_pid':os.getpid(),'report':str(compositepath),'report_sha256':sha(compositepath),'artifact_audit':str(H/'composite-artifact-audit.json'),'artifact_audit_sha256':sha(H/'composite-artifact-audit.json'),'reuse_proof_sha256':sha(H/'reuse-proof.json'),'plan_sha256':C['plan_sha256'],'root_authorization_sha256':sha(C['authorization_required']),'preflight_sha256':a['preflight_sha256'],'baseline_safeguard_receipt_sha256':C['baseline_safeguard_receipt_sha256'],'baseline_safeguard_scope':'Historical immutable evidence retained through unchanged headers/source context;no optional probe rerun.','limitations':plan['limitations']+pf['limitations'],'canonical_and_intake':states,'parent_gitlink_index':plan['parent_gitlink_index'],'scope':'Actual48 integrated final native CPP products plus3568 verified unaffected R12 coverage rows;actual R11/R12 build origins preserved. No full replay,historical published comparison,optional link/owner suite,whole-parity/byte-match,image/launch/runtime or publication claim.','excluded':'Entire Physics17path3commit cohort and all later source.','publication_allowed':False,'formal_review':False,'goal':'ACTIVE;native source/provider/link/image/launch work remains','created_utc':now()};save(H/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.json',seal)
 md=f"# Frozen R13 native composite\n\nClean intake `{head}`. **3333 PASS / 283 FAIL / 3616**: **48 fresh native CPP compiles + 3568 verified unaffected R12 rows**. All3285 prior passes retained;48 gains;zero regressions/timeouts;42/42 C pass.\n\nExact14 commits across11 groups change48 CPPs. All three successors applied before compilation. No header,new file or inventory change. Native48 uses4 jobs,showIncludes and all9 canonical vendor fallback roots.\n\n6302 source inputs bound;6254 unchanged. No include edge reaches changed CPPs;compiler flags,external files,tools and EA pins match R12 and stay stable. Each baseline product was hash-verified once;fresh48 products verified once. Historical R11/R12 origins remain per row. No old published manifest rehash/comparison.\n\nSource review and author-receipt limits remain in the JSON seal,including World/GUI authorflag omissions and bounded geometry/gamestate include evidence. Fresh integrated48 supersedes author compile coverage only.\n\nReport SHA256 `{sha(compositepath)}`. Audit SHA256 `{sha(H/'composite-artifact-audit.json')}`. Canonical3534 and parentindex209 unchanged. Physics cohort excluded. No optional full/link/owner suite,publication,image or launch. Overall native goal remains active.\n"
 with(H/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.md').open('x',encoding='utf-8')as f:f.write(md)
 ref='codex/experimental-x360-native-r13-checkpoint';existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+ref).decode().strip()
 if existing:assert existing==head
 else:subprocess.run(['git','branch',ref,head],cwd=W,check=True)
 assert git('rev-parse',ref).decode().strip()==head;guards(head)
 result={'head':head,'local_checkpoint':ref,'requested':3616,'passed':3333,'failed':283,'fresh':48,'retained':3568,'prior_passes_retained':3285,'gains':48,'regressions':0,'timeouts':0,'compiler_exit':0,'binder_exit':0,'report_sha256':sha(compositepath),'audit_sha256':sha(H/'composite-artifact-audit.json'),'seal_sha256':sha(H/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.json'),'seal_md_sha256':sha(H/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.md'),'scope':'No publication or full-link/image/launch claim.','goal':'ACTIVE'};save(H/'completion.json',result);(H/'operation.json').write_text(json.dumps({'phase':'R13 composite sealed','head':head,'owned_pid':None,'last_operation_pid':os.getpid(),'result':result},indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2),flush=True)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--run',action='store_true');ap.add_argument('--jobs',type=int,default=4);args=ap.parse_args()
 if not args.run:ap.error('Explicit --run and Root authorization record required;preparation alone cannot mutate intake.')
 assert 1<=args.jobs<=4;a,pf=auth();state=collect(a);run(a,pf,state,args.jobs)
if __name__=='__main__':main()
