import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent;B=H.parent/'r34-batch';R=H.parent/'r33-intake';W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest();read=lambda p:json.loads(pathlib.Path(p).read_text());git=lambda *x:subprocess.check_output(['git','--no-optional-locks',*x],cwd=W).decode().strip()
m=read(B/'MANIFEST.json'); p=read(H/'projection.json');ret=read(H/'deletion-check-binding.json'); rows={x['source']:x for x in read(R/'composite-active-native.json')['results']}
assert sha(B/'MANIFEST.json')=='cafd53f919dca31073f1ed949db0d94a67989e0a590145aa9fd1d7c808e9c225' and sha(B/'r34.patch')=='a169bd943629913dd2b3be035a4931455f058c6b2453fdf7f62b06d6de5c27ad'
assert m['base']=='f2eb2b7941b3af91bc6b1262ee82f52ab0d8de1e' and git('rev-parse',m['base']+'^{tree}')==m['base_tree'] and not git('status','--porcelain=v1') and not git('branch','--show-current')
observed=git('rev-parse','HEAD'); assert observed==m['base'] or (git('rev-parse',observed+'^')==m['base'] and git('rev-parse',observed+'^{tree}')==m['projected_tree'] and set(git('diff','--name-only',m['base'],observed).splitlines())=={x['path'] for x in m['final_paths']})
for e in m['final_paths']:
 line=git('ls-tree',m['base'],'--',e['path']);assert line.split('\t')[0]==e['old']
 if e['new']: assert git('cat-file','-t',e['new'].split()[-1])=='blob'
for k in ('deletion_boundary','base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff'):
 assert sha(m[k]['path'])==m[k]['sha256']
assert p['native_count']==3694 and p['previous_native_count']==3695 and p['removed_native']==m['retired_native'] and not p['new_native'] and len(p['selected_sources'])==85 and ret['prior_retired_source'] not in rows and ret['prior_retired_source'] not in p['projected_snapshot']['hashes']
assert len(p['named_guard_impacts'])==7 and all(not x['changed_source'] and not x['changed_actual_headers'] for x in p['named_guard_impacts']) and len(read(R/'named-guards.json'))==7 and all(x['gate_satisfied'] for x in read(R/'named-guards.json'))
for kind in ('netconn','entropy','loading'):
 origin=read(H/(kind+'-retention-origin.json')); binding=read(H/(kind+'-check-binding.json'));assert sha(origin['report'])==origin['report_sha256'] and origin['completion_sha256']==sha(R/'completion.json')
 key={'netconn':'r26_origin_sha256','entropy':'r28_origin_sha256','loading':'r29_origin_sha256'}[kind];assert sha(H/(kind+'-retention-origin.json'))==binding[key]
 names=list(origin['units']) if kind=='netconn' else [origin['source']]
 for n in names:
  o=origin['units'][n] if kind=='netconn' else origin; expected=o['sha256'] if kind=='netconn' else o['object_sha256'];assert rows[n]['passed'] and sha(rows[n]['object'])==expected and n not in p['selected_sources']
assert read(R/'netconn-codegen.json')['ok'] and read(R/'entropy-frame-verification.json')['status']=='pass' and read(R/'loading-coff-verification.json')['obj_sha256']==read(H/'loading-retention-origin.json')['object_sha256']
files=['projection.json','project.py','integrate.py','run_intake.py','guard-baseline.json','deletion-check-binding.json','netconn-check-binding.json','netconn-retention-origin.json','verify_netconn.py','entropy-check-binding.json','entropy-retention-origin.json','verify_entropy_frame.py','loading-check-binding.json','loading-retention-origin.json','verify_loading_coff.py']
result={'observed_head':observed,'base':m['base'],'base_tree':m['base_tree'],'manifest_sha256':sha(B/'MANIFEST.json'),'patch_sha256':sha(B/'r34.patch'),'projection_sha256':sha(H/'projection.json'),'native_target':3694,'selected_count':85,'retained_count':3609,'retired_current':m['retired_native'],'prior_retired':ret['prior_retired_source'],'guard_count':7,'guard_impacts':p['named_guard_impacts'],'witnesses':'R33 NetConn, entropy, and loading reports and original objects pinned; future intake reruns all three','base_receipts':{k:m[k] for k in ('deletion_boundary','base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff')},'prepared_files':{x:sha(H/x) for x in files},'review':'independent review is separate; no release issued'}
(H/'PREPARATION_BINDING.json').write_text(json.dumps(result,indent=2)+'\n');print('binding sha256',sha(H/'PREPARATION_BINDING.json'));print('projection sha256',sha(H/'projection.json'));print('bindings',json.dumps(result['prepared_files'],indent=2))
