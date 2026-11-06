import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent;B=H.parent/'r113-batch';R=H.parent/'r110-intake';W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest();read=lambda p:json.loads(pathlib.Path(p).read_text());git=lambda *x:subprocess.check_output(['git','--no-optional-locks',*x],cwd=W).decode().strip()
m=read(B/'MANIFEST.json');p=read(H/'projection.json'); prior=read(R/'composite-active-native.json');rows={x['source']:x for x in prior['results']};c=read(R/'completion.json')
assert sha(B/'MANIFEST.json')=='e4b1e8e2e86bb051c39fe2191ee9bde77944c946c8fd9b0369acad2d1000a3e9' and sha(B/'r113.patch')=='9d5894277697f424c543d9ad7509f7f16b676dc0d3c63fd9c5ab68f06862bfb6'
assert git('rev-parse','HEAD')==m['base']=='06f8bd729e0c0586d9dbfb47581aac33174f493f' and git('rev-parse','HEAD^{tree}')==m['base_tree'] and m['projected_tree']=='9ed23be4731468ce22e99dcc38a3a41319849053' and not git('status','--porcelain=v1') and not git('branch','--show-current')
assert len(m['final_paths'])==1 and all(e['new'] for e in m['final_paths']) and {e['path'] for e in m['final_paths'] if e['old'] is None}==set()
candidate='a53bcd14ec8eaa1999e1a875816cf3ca57d0ad56';assert git('rev-parse',candidate+'^{tree}')==m['projected_tree'];subprocess.run(['git','--no-optional-locks','-c','core.whitespace=cr-at-eol','diff','--check',m['base'],candidate],cwd=W,check=True)
for e in m['final_paths']:
 line=git('ls-tree','HEAD','--',e['path']); assert (line.split('\t')[0]==e['old'] if e['old'] else not line and not (W/e['path']).exists()) and git('cat-file','-t',e['new'].split()[-1])=='blob'
for k in ('base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff','original_launch_trace'):assert sha(m[k]['path'])==m[k]['sha256']
for g in m['groups']:
 assert sha(g['patch']['path'])==g['patch']['sha256']
 for x in g['receipts']:assert sha(x['path'])==x['sha256']
retired={'src/SDKs/EATech/rwcollision/volumelinequery.cpp','src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp'}
assert c['status']=='SEALED_NO_REGRESSIONS' and c['head']==m['base'] and c['passed']==c['requested']==3710 and c['failed']==0 and len(rows)==3710
assert p['base']==m['base'] and p['native_count']==3710 and p['previous_native_count']==3710 and not p['new_native'] and not (retired & set(rows)) and not (retired & set(p['projected_snapshot']['hashes'])) and len(p['selected_sources'])==1 and p['actual_prior_consumers_count']==0 and not p['actual_only']
assert not p['added_header_shadow']['new_headers'] and p['added_header_shadow']['external_files_scanned']>0 and not p['added_header_shadow']['direct_external_edges'] and not p['added_header_shadow']['prior_trace_consumers']
for x in p['prior_include_trace_origins']:assert sha(x['path'])==x['sha256']
assert len(p['named_guard_impacts'])==7 and len(read(R/'named-guards.json'))==7 and all(x['gate_satisfied'] for x in read(R/'named-guards.json'))
guards=read(H/'guard-baseline.json')['rows']; assert {x['name'] for x in guards if x['coverage_role']=='fresh R48 native safeguard'}=={'CombinedLayouts'} and {x['name'] for x in guards if x['coverage_role']=='fresh R35 native safeguard'}=={'TypedLayouts','HUDMemberGuards','HUDAllocationNegative'}
for x in guards:
 assert sha(x['source'])==x['source_sha256'] and sha(x['origin'])==x['origin_sha256']
 for path,h in x['artifacts'].items():assert sha(path)==h
 for path,h in x['actual_included_headers'].items():assert sha(path)==h
for kind in ('netconn','entropy','loading'):
 origin=read(H/(kind+'-retention-origin.json'));binding=read(H/(kind+'-check-binding.json'));assert sha(origin['report'])==origin['report_sha256'] and origin['completion_sha256']==sha(R/'completion.json')
 key={'netconn':'r26_origin_sha256','entropy':'r28_origin_sha256','loading':'r29_origin_sha256'}[kind];assert sha(H/(kind+'-retention-origin.json'))==binding[key]
 for n in list(origin['units']) if kind=='netconn' else [origin['source']]:
  o=origin['units'][n] if kind=='netconn' else origin;expected=o['sha256'] if kind=='netconn' else o['object_sha256'];assert rows[n]['passed'] and sha(rows[n]['object'])==expected
assert read(R/'netconn-codegen.json')['ok'] and read(R/'entropy-frame-verification.json')['status']=='pass' and read(R/'loading-coff-verification.json')['obj_sha256']==read(H/'loading-retention-origin.json')['object_sha256']
assert 'histor' in str(m['trace_qualification']).lower() and sha(H/'R110_RUNTIME_EVIDENCE.json')=='49db0c4b0d42e5f7941016eaad7c395204a597a29db0ab2d00536603cb5717b7'
files=['R110_RUNTIME_EVIDENCE.json','projection.json','project.py','integrate.py','run_intake.py','guard-baseline.json','netconn-check-binding.json','netconn-retention-origin.json','verify_netconn.py','entropy-check-binding.json','entropy-retention-origin.json','verify_entropy_frame.py','loading-check-binding.json','loading-retention-origin.json','verify_loading_coff.py']
result={'candidate':candidate,'cr_at_eol_diff_check':'pass','base':m['base'],'base_tree':m['base_tree'],'projected_tree':m['projected_tree'],'manifest_sha256':sha(B/'MANIFEST.json'),'patch_sha256':sha(B/'r113.patch'),'projection_sha256':sha(H/'projection.json'),'native_target':3710,'selected_count':1,'retained_count':3709,'historical_retirements':sorted(retired),'guard_count':7,'guard_impacts':p['named_guard_impacts'],'added_header_shadow':p['added_header_shadow'],'prior_trace_origins':p['prior_include_trace_origins'],'witnesses':'R110 NetConn, entropy and loading reports and original objects pinned; future intake reruns all three','trace_qualification':m['trace_qualification'],'base_receipts':{k:m[k] for k in ('base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff','original_launch_trace')},'prepared_files':{x:sha(H/x) for x in files},'review':'independent review is separate; no release issued'}
(H/'PREPARATION_BINDING.json').write_text(json.dumps(result,indent=2)+'\n');print('binding sha256',sha(H/'PREPARATION_BINDING.json'));print('projection sha256',sha(H/'projection.json'));print('bindings',json.dumps(result['prepared_files'],indent=2))
