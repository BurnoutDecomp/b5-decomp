import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent;B=H.parent/'r37-batch';R=H.parent/'r36-intake-repaired';W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest();read=lambda p:json.loads(pathlib.Path(p).read_text());git=lambda *x:subprocess.check_output(['git','--no-optional-locks',*x],cwd=W).decode().strip()
m=read(B/'MANIFEST.json');p=read(H/'projection.json'); prior=read(R/'composite-active-native.json');rows={x['source']:x for x in prior['results']};c=read(R/'completion.json')
assert sha(B/'MANIFEST.json')=='96368018bbc931bbf18dbd11d632539de948e30eecc10a6c8841f38a8464303e' and sha(B/'r37.patch')=='3f01ed6d40b4c54a1cbde1f6ef96f38df6c9c42ac95b35ed05519116d9c1037d'
assert git('rev-parse','HEAD')==m['base']=='84df79ddf8d4ce8e0c31beb4fb30ed29e650cabd' and git('rev-parse','HEAD^{tree}')==m['base_tree'] and m['projected_tree']=='9c81196758187d0364e0d7423f658e0819150f83' and not git('status','--porcelain=v1') and not git('branch','--show-current')
assert len(m['final_paths'])==4 and all(e['old'] and e['new'] for e in m['final_paths'])
for e in m['final_paths']:
 assert git('ls-tree','HEAD','--',e['path']).split('\t')[0]==e['old'] and git('cat-file','-t',e['new'].split()[-1])=='blob'
for k in ('base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff'):assert sha(m[k]['path'])==m[k]['sha256']
for g in m['groups']:
 assert sha(g['patch']['path'])==g['patch']['sha256']
 for x in g['receipts']:assert sha(x['path'])==x['sha256']
retired={'src/SDKs/EATech/rwcollision/volumelinequery.cpp','src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp'}
assert c['status']=='SEALED_NO_REGRESSIONS' and c['head']==m['base'] and c['passed']==c['requested']==3694 and c['failed']==0 and len(rows)==3694
assert p['base']==m['base'] and p['native_count']==p['previous_native_count']==3694 and not p['new_native'] and not (retired & set(rows)) and not (retired & set(p['projected_snapshot']['hashes'])) and len(p['selected_sources'])==3 and p['actual_prior_consumers_count']==1 and not p['actual_only']
for x in p['prior_include_trace_origins']:assert sha(x['path'])==x['sha256']
assert len(p['named_guard_impacts'])==7 and len(read(R/'named-guards.json'))==7 and all(x['gate_satisfied'] for x in read(R/'named-guards.json'))
guards=read(H/'guard-baseline.json')['rows']; assert {x['name'] for x in guards if x['coverage_role']=='fresh R36 native safeguard'}=={'CombinedLayouts'} and {x['name'] for x in guards if x['coverage_role']=='fresh R35 native safeguard'}=={'TypedLayouts','HUDMemberGuards','HUDAllocationNegative'}
for x in guards:
 assert sha(x['source'])==x['source_sha256'] and sha(x['origin'])==x['origin_sha256']
 for path,h in x['artifacts'].items():assert sha(path)==h
 for path,h in x['actual_included_headers'].items():assert sha(path)==h
for kind in ('netconn','entropy','loading'):
 origin=read(H/(kind+'-retention-origin.json'));binding=read(H/(kind+'-check-binding.json'));assert sha(origin['report'])==origin['report_sha256'] and origin['completion_sha256']==sha(R/'completion.json')
 key={'netconn':'r26_origin_sha256','entropy':'r28_origin_sha256','loading':'r29_origin_sha256'}[kind];assert sha(H/(kind+'-retention-origin.json'))==binding[key]
 for n in list(origin['units']) if kind=='netconn' else [origin['source']]:
  o=origin['units'][n] if kind=='netconn' else origin;expected=o['sha256'] if kind=='netconn' else o['object_sha256'];assert rows[n]['passed'] and sha(rows[n]['object'])==expected and n not in p['selected_sources']
assert read(R/'netconn-codegen.json')['ok'] and read(R/'entropy-frame-verification.json')['status']=='pass' and read(R/'loading-coff-verification.json')['obj_sha256']==read(H/'loading-retention-origin.json')['object_sha256']
files=['projection.json','project.py','integrate.py','run_intake.py','guard-baseline.json','netconn-check-binding.json','netconn-retention-origin.json','verify_netconn.py','entropy-check-binding.json','entropy-retention-origin.json','verify_entropy_frame.py','loading-check-binding.json','loading-retention-origin.json','verify_loading_coff.py']
result={'base':m['base'],'base_tree':m['base_tree'],'projected_tree':m['projected_tree'],'manifest_sha256':sha(B/'MANIFEST.json'),'patch_sha256':sha(B/'r37.patch'),'projection_sha256':sha(H/'projection.json'),'native_target':3694,'selected_count':3,'retained_count':3691,'historical_retirements':sorted(retired),'guard_count':7,'guard_impacts':p['named_guard_impacts'],'prior_trace_origins':p['prior_include_trace_origins'],'witnesses':'R36 NetConn, entropy and loading reports and original objects pinned; future intake reruns all three','base_receipts':{k:m[k] for k in ('base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff')},'prepared_files':{x:sha(H/x) for x in files},'review':'independent review is separate; no release issued'}
(H/'PREPARATION_BINDING.json').write_text(json.dumps(result,indent=2)+'\n');print('binding sha256',sha(H/'PREPARATION_BINDING.json'));print('projection sha256',sha(H/'projection.json'));print('bindings',json.dumps(result['prepared_files'],indent=2))
