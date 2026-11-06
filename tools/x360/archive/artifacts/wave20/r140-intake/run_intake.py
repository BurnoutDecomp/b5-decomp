import json,hashlib,subprocess,pathlib,os,sys,importlib.util,re,struct,ast
sys.dont_write_bytecode=True
H=pathlib.Path(__file__).resolve().parent; B=H.parent/'r140-batch'; R=H.parent/'r139-intake'; L=H.parent.parent/'wave8/repair-r21-layout-intake-20261103'; W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); CAN=pathlib.Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp'); P=CAN.parent
read=lambda p:json.loads(pathlib.Path(p).read_text());sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def save(p,x):
 with open(p,'x',encoding='utf-8',newline='\n') as f:json.dump(x,f,indent=2);f.write('\n')
def git(*args,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=cwd)
def module(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip() and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4' and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==read(read(L/'campaign.json')['plan'])['parent_gitlink_index']
def main():
 auth=read(H/'ROOT_RELEASE.json');assert auth['approved'] is True and auth['manifest_sha256']==sha(B/'MANIFEST.json') and auth['patch_sha256']==sha(B/'r140.patch') and auth['projection_sha256']==sha(H/'projection.json')
 assert sha(auth['review_path'])==auth['review_sha256']; projection=read(H/'projection.json');base=projection['base'];binding=read(H/'commit-binding.json');head=binding['head'];guards(head)
 assert binding['base']==base and binding['manifest_sha256']==auth['manifest_sha256'] and binding['patch_sha256']==auth['patch_sha256'] and binding['projection_sha256']==auth['projection_sha256']
 manifest=read(B/'MANIFEST.json');entries={x['path']:x for x in manifest['final_paths']};assert len(entries)==7 and len(entries)==len(manifest['final_paths']) and all(x['new'] for x in entries.values()) and {p for p,x in entries.items() if x['old'] is None}==set() and set(git('diff','--name-only',base,head).decode().splitlines())==set(entries)
 for p,x in entries.items():
  expected=x['new'].split()[-1];raw=git('hash-object','--no-filters','--',p).decode().strip();filtered=git('hash-object','--path='+p,'--',p).decode().strip();assert expected in (raw,filtered),p
 loading=read(H/'loading-check-binding.json');assert sha(H/'loading-check-binding.json')==auth['loading_binding_sha256'] and sha(loading['stage']['path'])==loading['stage']['sha256'] and sha(loading['contract']['path'])==loading['contract']['sha256'] and sha(loading['coff_reference']['path'])==loading['coff_reference']['sha256'] and sha(loading['reference_verifier_path'])==loading['reference_verifier_sha256'] and sha(H/'verify_loading_coff.py')==loading['adapted_verifier_sha256']; subprocess.run([sys.executable,'-X','utf8','-B',loading['stage']['path'],'--worktree',str(W),'--head',head,'--receipt',str(H/'loading-input.json')],check=True);generated_pins=read(H/'loading-input.json')['generated'];assert generated_pins and all(sha(p)==h for p,h in generated_pins.items())
 e=module('r140_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=base
 prior=read(R/'composite-active-native.json');audit=read(R/'composite-artifact-audit.json'); old={r['source']:r for r in prior['results']}; assert len(old)==3710 and prior['failed']==0
 historically_retired={'src/SDKs/EATech/rwcollision/volumelinequery.cpp','src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp'};assert not (historically_retired & set(old))
 snapshot=e.common.source_snapshot(); projected=projection['projected_snapshot'];assert snapshot['dependencies']==projected['dependencies'] and set(snapshot['hashes'])==set(projected['hashes']);assert all(snapshot['hashes'][p]==projected['hashes'][p] for p in snapshot['hashes'] if p not in entries)
 if not (H/'line-ending-projection-proof.json').exists():save(H/'line-ending-projection-proof.json',{'head':head,'approved_index_and_filter_blob_ids_verified':True,'changed_worktree_hashes':{p:snapshot['hashes'][p] for p in entries if p in snapshot['hashes']},'unchanged_hashes_equal':True,'dependency_graphs_identical':True,'reason':'Git checkout line ending conversion; compiler not yet invoked','projection_sha256':sha(H/'projection.json')})
 native=sorted(p for p in e.common.inventory() if pathlib.Path(p).suffix.lower() in ('.c','.cpp')); assert len(native)==projection['native_count'] and set(native)==set(old)|set(projection['new_native'])
 assert not (historically_retired & set(native));selected=projection['selected_sources'];assert selected and set(selected)<=set(native) and set(projection['new_native'])<=set(selected)
 external=e.external_inputs(prior)
 for k in ('include_trees','submodules','compiler_and_runtime'):assert external[k]==audit['external_inputs'][k]
 # External trees can introduce indirect consumers; conservatively reject changed basenames in their includes.
 names={pathlib.PurePosixPath(p).name.lower() for p in entries}; edges=[]
 for d,t in external['include_trees'].items():
  for rel in t['files']:
   p=pathlib.Path(d)/rel
   if p.suffix.lower() not in e.common.TEXT_EXT:continue
   for hit in re.findall(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
    if pathlib.PurePosixPath(hit.replace('\\','/')).name.lower() in names:edges.append((str(p),hit))
 save(H/'external-wrapper-edges.json',{'edges':edges,'prior_actual_traces_checked':projection['prior_include_trace_origins'],'prior_actual_consumers_in_selected':projection['actual_prior_consumers_count'],'actual_only_not_in_lexical':projection['actual_only'],'reason':'External include trees and prior compiler flags unchanged; every prior origin actual include trace checked for changed worktree paths, and all old/new lexical consumers are selected.'})
 sdk=read(L/'SDK-reference-retention-before.json'); assert len(sdk['SDK_reference_records'])==2
 for k in ('supporting_inputs','SDK_files'):
  assert {p:sha(p) for p in sdk[k]}==sdk[k]
 sdkroot=pathlib.Path(prior['sdk']).resolve()/'include'; sdkheaders={str(p.resolve()).lower():sha(p) for p in sdkroot.rglob('*') if p.is_file()}
 retained=[r for p,r in old.items() if p not in set(selected)];e.verify_manifest({'results':retained},audit['artifact_manifest'])
 save(H/'retention-before.json',{'head':head,'retained_rows':len(retained),'retained_artifact_entries':sum(len(audit['artifact_manifest'][r['source']]) for r in retained),'external_inputs':external,'SDK_records':sdk['SDK_reference_records'],'sdk_include_hashes':sdkheaders,'old_report_sha256':sha(R/'composite-active-native.json'),'old_audit_sha256':sha(R/'composite-artifact-audit.json')})
 out=H/'native-selected';temp=H/'temp';assert not out.exists() and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 named=read(H/'guard-baseline.json');assert sha(H/'guard-baseline.json')==auth['guard_baseline_sha256']; fn=H.parent.parent/'wave8/repair-r4/compile_header_probes.py'; nodes=[n for n in ast.parse(fn.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='symbol_words']; ns={'Path':pathlib.Path,'struct':struct};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fn),'exec'),ns); words=ns['symbol_words']; symbols={'CombinedLayouts':('R4LayoutFacts',25),'TypedLayouts':('R7TypedFacts',21),'HUDMemberGuards':('HudNativeOffsets',8),'ResourceContracts':('R11ResourceFacts',9)}; impacts={r['name']:r for r in projection['named_guard_impacts']}; guardout=H/'named-guards';guardout.mkdir();guardrows=[]
 for r in named['rows']:
  assert sha(r['source'])==r['source_sha256']; impact=impacts[r['name']]
  if not impact['changed_source'] and not impact['changed_actual_headers']:
   for p,h in r['actual_included_headers'].items():assert sha(p)==h
   for p,h in r['artifacts'].items():assert sha(p)==h
   assert sha(r['origin'])==r['origin_sha256'];guardrows.append({'name':r['name'],'kind':'retained','gate_satisfied':r['gate_satisfied'],'origin':r['origin'],'origin_sha256':r['origin_sha256'],'guard_baseline_sha256':sha(H/'guard-baseline.json'),'artifacts':r['artifacts']});continue
  cmd=[('/Fo'+str(guardout/(r['name']+'.obj'))) if a.startswith('/Fo') else ('/Fa'+str(guardout/(r['name']+'.asm'))) if a.startswith('/Fa') else a for a in r['command']]
  env=dict(os.environ,XEDK=prior['sdk'],PATH=str(pathlib.Path(prior['sdk'])/'bin/win32')+os.pathsep+str(pathlib.Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(pathlib.Path(prior['sdk'])/'include/xbox'),LIB=str(pathlib.Path(prior['sdk'])/'lib/xbox')); [env.pop(k,None) for k in ('CL','_CL_','LINK','_LINK_')]
  run=subprocess.run(cmd,cwd=W,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180); log=guardout/(r['name']+'.log');log.write_bytes(run.stdout); obj=guardout/(r['name']+'.obj'); diagnostics=[x for x in run.stdout.decode(errors='replace').splitlines() if 'error C' in x]; passed=(run.returncode==0 and obj.exists() and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2) if not r['expected_negative'] else (run.returncode!=0 and len(diagnostics)==1 and 'C2118' in diagnostics[0] and not obj.exists()); facts=words(obj,*symbols[r['name']]) if passed and r['name'] in symbols else None
  if facts is not None:passed=passed and facts==named['emitted_facts'][r['name']]
  guardrows.append({'name':r['name'],'kind':'fresh','command':cmd,'returncode':run.returncode,'gate_satisfied':passed,'emitted_facts':facts,'log_sha256':sha(log),'object_sha256':sha(obj) if obj.exists() else None,'assembly_sha256':sha(guardout/(r['name']+'.asm')) if (guardout/(r['name']+'.asm')).exists() else None})
 save(H/'named-guards.json',guardrows);assert all(r['gate_satisfied'] for r in guardrows)
 driver=W/'tools/build_x360.py';assert sha(driver)==read(L/'native179-command.json')['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[a for a in next(iter(old.values()))['command'] if a.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',prior['sdk'],'--out',str(out),'--jobs','8','--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in selected:argv+=['--source',p]
 save(H/'command.json',{'head':head,'argv':argv,'driver_sha256':sha(driver),'runner_sha256':sha(__file__),'private_temp':str(temp),'selected_count':len(selected)})
 print('R140_NATIVE_START',head,len(selected),flush=True);d=module('r140_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(selected)
 save(H/'native-exit.json',{'exit_code':code,'report_sha256':sha(out/'compile-results.json')})
 binding_net=read(H/'netconn-check-binding.json');assert sha(H/'netconn-check-binding.json')==auth['netconn_binding_sha256'] and sha(H/'verify_netconn.py')==binding_net['verify_sha256']; crt=pathlib.Path(prior['sdk'])/'lib/xbox/libcMT.lib'; assert sha(crt)==binding_net['expected_crt_sha256']
 fs='vendor/dirtysdk/src/x360/netconn_friends_x360.cpp'; osrc='vendor/dirtysdk/src/x360/netconn_owner_x360.cpp'; origin=read(H/'netconn-retention-origin.json'); assert sha(H/'netconn-retention-origin.json')==binding_net['r26_origin_sha256'] and sha(origin['report'])==origin['report_sha256'] and sha(R/'completion.json')==origin['completion_sha256']; frow=idx[fs] if fs in idx else old[fs]; orow=idx[osrc] if osrc in idx else old[osrc]; assert frow['passed'] and orow['passed']; av=[sys.executable,str(H/'verify_netconn.py'),'--friends',frow['object'],'--owner',orow['object'],'--crt',str(crt),'--out',str(H/'netconn-codegen.json')]; rr=subprocess.run(av,cwd=W,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180); (H/'netconn-codegen.log').write_bytes(rr.stdout); save(H/'netconn-codegen-invocation.json',{'command':av,'exit_code':rr.returncode,'log_sha256':sha(H/'netconn-codegen.log'),'verify_sha256':sha(H/'verify_netconn.py')});assert rr.returncode==0 and read(H/'netconn-codegen.json')['ok'], 'NetConn PPC/COFF/CRT check failed'
 entropy=read(H/'entropy-check-binding.json'); entropy_origin=read(H/'entropy-retention-origin.json');assert sha(H/'entropy-check-binding.json')==auth['entropy_binding_sha256'] and sha(H/'entropy-retention-origin.json')==entropy['r28_origin_sha256'] and sha(entropy_origin['report'])==entropy_origin['report_sha256'] and sha(R/'completion.json')==entropy_origin['completion_sha256'] and sha(H/'verify_entropy_frame.py')==entropy['adapted_verifier_sha256'] and sha(entropy['original_verifier'])==entropy['original_verifier_sha256'] and sha(entropy['original_receipt'])==entropy['original_receipt_sha256']; esrc=entropy_origin['source'];erow=idx[esrc] if esrc in idx else old[esrc];assert erow['passed'];save(H/'entropy-input-report.json',{'results':[erow]});eargv=[sys.executable,str(H/'verify_entropy_frame.py'),'--report',str(H/'entropy-input-report.json'),'--out',str(H/'entropy-frame-verification.json')];erun=subprocess.run(eargv,cwd=W,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);(H/'entropy-frame-verification.log').write_bytes(erun.stdout);save(H/'entropy-frame-invocation.json',{'command':eargv,'returncode':erun.returncode,'log_sha256':sha(H/'entropy-frame-verification.log'),'binding_sha256':sha(H/'entropy-check-binding.json')});assert erun.returncode==0 and read(H/'entropy-frame-verification.json')['status']=='pass' and read(H/'entropy-frame-verification.json')['object_sha256']==sha(erow['object']), 'Entropy frame check failed'
 loading_origin=read(H/'loading-retention-origin.json');assert sha(H/'loading-retention-origin.json')==loading['r29_origin_sha256'] and sha(loading_origin['report'])==loading_origin['report_sha256'] and sha(R/'completion.json')==loading_origin['completion_sha256'];loading_source=loading_origin['source'];lrow=idx[loading_source] if loading_source in idx else old[loading_source];assert lrow['passed'];save(H/'loading-input-report.json',{'results':[lrow]});largv=[sys.executable,str(H/'verify_loading_coff.py'),'--report',str(H/'loading-input-report.json'),'--generated',str(W/'build/x360/generated'),'--reference',loading['coff_reference']['path'],'--out',str(H/'loading-coff-verification.json')]; lrun=subprocess.run(largv,cwd=W,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);(H/'loading-coff-verification.log').write_bytes(lrun.stdout);save(H/'loading-coff-invocation.json',{'command':largv,'returncode':lrun.returncode,'log_sha256':sha(H/'loading-coff-verification.log'),'binding_sha256':sha(H/'loading-check-binding.json')});assert lrun.returncode==0 and read(H/'loading-coff-verification.json')['obj_sha256']==sha(lrow['object']), 'Loading COFF verification failed'
 after=e.external_inputs(new)
 for k in ('include_trees','submodules','compiler_and_runtime'):assert after[k]==external[k]
 assert {p:sha(W/p) for p in snapshot['hashes']}==snapshot['hashes'] and all(sha(p)==h for p,h in generated_pins.items());guards(head)
 for k in ('supporting_inputs','SDK_files'):assert {p:sha(p) for p in sdk[k]}==sdk[k]
 assert {str(p.resolve()).lower():sha(p) for p in sdkroot.rglob('*') if p.is_file()}==sdkheaders
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()};allinputs.update({str(pathlib.Path(p).resolve()).lower():h for p,h in generated_pins.items()})
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((pathlib.Path(d)/rel).resolve()).lower()]=h
 # SDK headers are pinned by named-guard input inventory or source driver SDK; record hashes as observed.
 actual={}; include_hash_cache={}
 for p,r in idx.items():
  found={}
  for line in pathlib.Path(r['log']).read_text(errors='replace').splitlines():
   if 'including file:' in line:
    raw_path=pathlib.Path(line.split('including file:',1)[1].strip());path=str((raw_path if raw_path.is_absolute() else W/raw_path).resolve());key=path.lower()
    if key not in include_hash_cache:include_hash_cache[key]=sha(path)
    observed=include_hash_cache[key]
    if key in allinputs:assert observed==allinputs[key]
    else:assert key in sdkheaders and observed==sdkheaders[key],('unfrozen include',p,path)
    found[path]=observed
  actual[p]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual})
 flags=lambda r:[a for a in r['command'][:-1] if not a.startswith(('/Fo','/Fa')) and a.lower()!='/showincludes']
 for p in set(idx)&set(old):assert flags(idx[p])==flags(old[p]),p
 rows=[];newsha=sha(out/'compile-results.json'); priorsha=sha(R/'composite-active-native.json')
 for p in native:
  if p in idx:row={**idx[p],'build_head':head,'origin_kind':'fresh R140 native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:row={**old[p],'retained_via_report':str(R/'composite-active-native.json'),'retained_via_report_sha256':priorsha}
  rows.append(row)
 composite={k:v for k,v in prior.items() if k!='results'};composite.update(state='complete',requested=len(rows),results=rows,passed=sum(r['passed'] for r in rows));composite['failed']=len(rows)-composite['passed'];composite['provenance']={'source_head':head,'fresh':len(idx),'retained':len(retained),'SDK_reference_units':2,'real_prior_origins_preserved':True};save(H/'composite-active-native.json',composite)
 summary=e.compare(composite,prior);art={**audit['artifact_manifest'],**newart};save(H/'composite-artifact-audit.json',{'head':head,'base':base,'summary':summary,'artifact_manifest':art,'source_snapshot':snapshot,'generated_inputs':generated_pins,'external_inputs':external,'SDK_reference_records':sdk['SDK_reference_records']})
 status='SEALED_NO_REGRESSIONS' if not summary['regressions'] and not summary['timeouts'] and not composite['failed'] else 'PENDING_SOURCE_OR_COMPILE_DEFECT'
 if status=='SEALED_NO_REGRESSIONS':
  checkpoint='codex/experimental-x360-native-r140-checkpoint'; existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+checkpoint).decode().strip(); assert not existing or existing==head
  if not existing:subprocess.run(['git','branch',checkpoint,head],cwd=W,check=True)
 guards(head);save(H/'completion.json',{'head':head,'status':status,'requested':len(rows),'passed':composite['passed'],'failed':composite['failed'],'fresh':len(idx),'retained':len(retained),'SDK_refs':2,'netconn_codegen_report_sha256':sha(H/'netconn-codegen.json'),'entropy_frame_report_sha256':sha(H/'entropy-frame-verification.json'),'loading_coff_report_sha256':sha(H/'loading-coff-verification.json'),'loading_input_sha256':sha(H/'loading-input.json'),'summary':summary,'named_guards':guardrows,'compiler_exit':code,'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json')});print('R140_TERMINAL',status,composite['passed'],composite['failed'],flush=True)
if __name__=='__main__':
 try:main()
 except BaseException as exc:
  import traceback
  failure=H/'runner-failure.json'
  if not failure.exists():save(failure,{'type':type(exc).__name__,'error':str(exc),'traceback':traceback.format_exc()})
  raise
