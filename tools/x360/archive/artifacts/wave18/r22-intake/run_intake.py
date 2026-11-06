import json,hashlib,subprocess,pathlib,os,sys,importlib.util,re,struct,ast
sys.dont_write_bytecode=True
H=pathlib.Path(__file__).resolve().parent; B=H.parent/'r22-batch-repaired'; R=H.parent.parent/'wave8/repair-r21-game-cxx03-repair-20261103'; L=H.parent.parent/'wave8/repair-r21-layout-intake-20261103'; W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); CAN=pathlib.Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp'); P=CAN.parent
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
 auth=read(H/'ROOT_RELEASE.json');assert auth['approved'] is True and auth['manifest_sha256']==sha(B/'MANIFEST.json') and auth['patch_sha256']==sha(B/'r22.patch') and auth['projection_sha256']==sha(H/'projection.json')
 assert sha(auth['review_path'])==auth['review_sha256']; projection=read(H/'projection.json'); base=projection['base'];guards(base)
 manifest=read(B/'MANIFEST.json'); entries={x['path']:x for g in manifest['groups'] for x in g['paths']}; assert len(entries)==102
 for g in manifest['groups']:assert sha(B/(g['name']+'.patch'))==g['patch_sha256']
 for p,x in entries.items():
  staged=git('ls-files','--stage','--',p).decode().strip(); oldentry=x['old']; assert (not staged if not oldentry else staged.split()[1]==oldentry.split()[-1]),p
 priorcheck=read(R/'composite-active-native.json'); origins={}
 for r in priorcheck['results']:
  path=r['origin_report'];digest=r['origin_report_sha256']; assert path not in origins or origins[path]==digest;origins[path]=digest
 for path,digest in origins.items():assert sha(path)==digest,path
 for trace in projection['prior_include_trace_origins']:assert sha(trace['path'])==trace['sha256']
 subprocess.run(['git','apply','--check','--index',str(B/'r22.patch')],cwd=W,check=True); subprocess.run(['git','apply','--index',str(B/'r22.patch')],cwd=W,check=True)
 assert set(git('diff','--cached','--name-only').decode().splitlines())==set(entries)
 for p,x in entries.items(): assert git('ls-files','--stage','--',p).decode().split()[1]==x['new'].split()[-1]
 trailer=auth['co_author_trailer']; assert trailer.startswith('Co-Authored-By: ') and '\n' not in trailer
 message=H/'COMMIT_MESSAGE.txt';message.write_text('Integrate reviewed R22 native reconstruction batch\n\n'+trailer+'\n',encoding='utf-8');subprocess.run(['git','commit','-F',str(message)],cwd=W,check=True)
 head=git('rev-parse','HEAD').decode().strip(); guards(head);save(H/'commit-binding.json',{'base':base,'head':head,'paths':sorted(entries),'manifest_sha256':sha(B/'MANIFEST.json'),'patch_sha256':sha(B/'r22.patch'),'projection_sha256':sha(H/'projection.json')})
 e=module('r22_evidence',H.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=base
 prior=read(R/'composite-active-native.json');audit=read(R/'composite-artifact-audit.json'); seal=read(R/'FROZEN_COMPOSITE_DIAGNOSTIC_R21_REPAIRED.json'); old={r['source']:r for r in prior['results']}; assert len(old)==3616 and prior['failed']==0
 snapshot=e.common.source_snapshot(); projected=projection['projected_snapshot'];assert snapshot['hashes']==projected['hashes'] and snapshot['dependencies']==projected['dependencies']
 native=sorted(p for p in e.common.inventory() if pathlib.Path(p).suffix.lower() in ('.c','.cpp')); assert len(native)==3642 and set(native)==set(old)|set(projection['new_native'])
 selected=projection['selected_sources'];assert selected and set(selected)<=set(native) and set(projection['new_native'])<=set(selected)
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
 assert not edges,edges
 sdk=read(L/'SDK-reference-retention-before.json'); assert len(sdk['SDK_reference_records'])==2
 for k in ('supporting_inputs','SDK_files'):
  assert {p:sha(p) for p in sdk[k]}==sdk[k]
 sdkroot=pathlib.Path(prior['sdk']).resolve()/'include'; sdkheaders={str(p.resolve()).lower():sha(p) for p in sdkroot.rglob('*') if p.is_file()}
 retained=[r for p,r in old.items() if p not in set(selected)];e.verify_manifest({'results':retained},audit['artifact_manifest'])
 save(H/'retention-before.json',{'head':head,'retained_rows':len(retained),'retained_artifact_entries':sum(len(audit['artifact_manifest'][r['source']]) for r in retained),'external_inputs':external,'SDK_records':sdk['SDK_reference_records'],'sdk_include_hashes':sdkheaders,'old_report_sha256':sha(R/'composite-active-native.json'),'old_audit_sha256':sha(R/'composite-artifact-audit.json')})
 out=H/'native498';temp=H/'temp';assert not out.exists() and not temp.exists();temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp)
 named=read(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r18-intake-20261103/named-safeguard-results.json')); fn=H.parent.parent/'wave8/repair-r4/compile_header_probes.py'; nodes=[n for n in ast.parse(fn.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='symbol_words']; ns={'Path':pathlib.Path,'struct':struct};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fn),'exec'),ns); words=ns['symbol_words']; symbols={'CombinedLayouts':('R4LayoutFacts',25),'TypedLayouts':('R7TypedFacts',21),'HUDMemberGuards':('HudNativeOffsets',8),'ResourceContracts':('R11ResourceFacts',9)}; impacts={r['name']:r for r in projection['named_guard_impacts']}; guardout=H/'named-guards';guardout.mkdir();guardrows=[]
 for r in named['rows']:
  assert sha(r['source'])==r['source_sha256']; impact=impacts[r['name']]
  if not impact['changed_source'] and not impact['changed_actual_headers']:
   for p,h in r['actual_included_headers'].items():assert sha(p)==h
   for p,h in r['artifacts'].items():assert sha(p)==h
   guardrows.append({'name':r['name'],'kind':'retained','gate_satisfied':r['gate_satisfied']});continue
  cmd=[('/Fo'+str(guardout/(r['name']+'.obj'))) if a.startswith('/Fo') else ('/Fa'+str(guardout/(r['name']+'.asm'))) if a.startswith('/Fa') else a for a in r['command']]
  env=dict(os.environ,XEDK=prior['sdk'],PATH=str(pathlib.Path(prior['sdk'])/'bin/win32')+os.pathsep+str(pathlib.Path(os.environ['SystemRoot'])/'System32'),INCLUDE=str(pathlib.Path(prior['sdk'])/'include/xbox'),LIB=str(pathlib.Path(prior['sdk'])/'lib/xbox')); [env.pop(k,None) for k in ('CL','_CL_','LINK','_LINK_')]
  run=subprocess.run(cmd,cwd=W,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180); log=guardout/(r['name']+'.log');log.write_bytes(run.stdout); obj=guardout/(r['name']+'.obj'); diagnostics=[x for x in run.stdout.decode(errors='replace').splitlines() if 'error C' in x]; passed=(run.returncode==0 and obj.exists() and struct.unpack_from('<H',obj.read_bytes())[0]==0x1f2) if not r['expected_negative'] else (run.returncode!=0 and len(diagnostics)==1 and 'C2118' in diagnostics[0] and not obj.exists()); facts=words(obj,*symbols[r['name']]) if passed and r['name'] in symbols else None
  if facts is not None:passed=passed and facts==named['emitted_facts'][r['name']]
  guardrows.append({'name':r['name'],'kind':'fresh','command':cmd,'returncode':run.returncode,'gate_satisfied':passed,'emitted_facts':facts,'log_sha256':sha(log)})
 save(H/'named-guards.json',guardrows);assert all(r['gate_satisfied'] for r in guardrows)
 driver=W/'tools/build_x360.py';assert sha(driver)==read(L/'native179-command.json')['driver_sha256'];canonical=str(CAN).replace('\\','/').lower();fallback=[a for a in next(iter(old.values()))['command'] if a.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')];assert len(fallback)==9
 argv=[str(driver),'--sdk',prior['sdk'],'--out',str(out),'--jobs','8','--timeout','180','--quiet']+['--compiler-arg='+x for x in fallback]+['--compiler-arg=/showIncludes']
 for p in selected:argv+=['--source',p]
 save(H/'command.json',{'head':head,'argv':argv,'driver_sha256':sha(driver),'runner_sha256':sha(__file__),'private_temp':str(temp),'selected_count':len(selected)})
 print('R22_NATIVE_START',head,len(selected),flush=True);d=module('r22_driver',driver);sys.argv=argv;code=d.main();new,idx,newart=e.common.report_check(out/'compile-results.json');assert set(idx)==set(selected)
 save(H/'native-exit.json',{'exit_code':code,'report_sha256':sha(out/'compile-results.json')});after=e.external_inputs(new)
 for k in ('include_trees','submodules','compiler_and_runtime'):assert after[k]==external[k]
 assert {p:sha(W/p) for p in snapshot['hashes']}==snapshot['hashes'];guards(head)
 for k in ('supporting_inputs','SDK_files'):assert {p:sha(p) for p in sdk[k]}==sdk[k]
 assert {str(p.resolve()).lower():sha(p) for p in sdkroot.rglob('*') if p.is_file()}==sdkheaders
 allinputs={str((W/p).resolve()).lower():h for p,h in snapshot['hashes'].items()}
 for d,t in external['include_trees'].items():
  for rel,h in t['files'].items():allinputs[str((pathlib.Path(d)/rel).resolve()).lower()]=h
 # SDK headers are pinned by named-guard input inventory or source driver SDK; record hashes as observed.
 actual={}
 for p,r in idx.items():
  found={}
  for line in pathlib.Path(r['log']).read_text(errors='replace').splitlines():
   if 'including file:' in line:
    path=str(pathlib.Path(line.split('including file:',1)[1].strip()).resolve());key=path.lower()
    if key in allinputs:assert sha(path)==allinputs[key]
    else:assert key in sdkheaders and sha(path)==sdkheaders[key],('unfrozen include',p,path)
    found[path]=sha(path)
  actual[p]=found
 save(H/'actual-includes.json',{'head':head,'rows':actual})
 flags=lambda r:[a for a in r['command'][:-1] if not a.startswith(('/Fo','/Fa')) and a.lower()!='/showincludes']
 for p in set(idx)&set(old):assert flags(idx[p])==flags(old[p]),p
 rows=[];newsha=sha(out/'compile-results.json')
 for p in native:
  if p in idx:row={**idx[p],'build_head':head,'origin_kind':'fresh R22 native compile','origin_report':str(out/'compile-results.json'),'origin_report_sha256':newsha}
  else:row={**old[p],'retained_via_report':str(R/'composite-active-native.json'),'retained_via_report_sha256':sha(R/'composite-active-native.json')}
  rows.append(row)
 composite={k:v for k,v in prior.items() if k!='results'};composite.update(state='complete',requested=len(rows),results=rows,passed=sum(r['passed'] for r in rows));composite['failed']=len(rows)-composite['passed'];composite['provenance']={'source_head':head,'fresh':len(idx),'retained':len(retained),'SDK_reference_units':2,'real_prior_origins_preserved':True};save(H/'composite-active-native.json',composite)
 summary=e.compare(composite,prior);art={**audit['artifact_manifest'],**newart};save(H/'composite-artifact-audit.json',{'head':head,'base':base,'summary':summary,'artifact_manifest':art,'source_snapshot':snapshot,'external_inputs':external,'SDK_reference_records':sdk['SDK_reference_records']})
 status='SEALED_NO_REGRESSIONS' if not summary['regressions'] and not summary['timeouts'] and not composite['failed'] else 'PENDING_SOURCE_OR_COMPILE_DEFECT'
 if status=='SEALED_NO_REGRESSIONS':
  checkpoint='codex/experimental-x360-native-r22-checkpoint'; existing=git('for-each-ref','--format=%(objectname)','refs/heads/'+checkpoint).decode().strip(); assert not existing or existing==head
  if not existing:subprocess.run(['git','branch',checkpoint,head],cwd=W,check=True)
 guards(head);save(H/'completion.json',{'head':head,'status':status,'requested':len(rows),'passed':composite['passed'],'failed':composite['failed'],'fresh':len(idx),'retained':len(retained),'SDK_refs':2,'summary':summary,'named_guards':guardrows,'compiler_exit':code,'report_sha256':sha(H/'composite-active-native.json'),'audit_sha256':sha(H/'composite-artifact-audit.json')});print('R22_TERMINAL',status,composite['passed'],composite['failed'],flush=True)
if __name__=='__main__':main()
