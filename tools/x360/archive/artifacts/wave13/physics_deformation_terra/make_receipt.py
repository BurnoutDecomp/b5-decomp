import json,hashlib,subprocess,pathlib,re,os
repo=pathlib.Path.cwd(); art=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave13/physics_deformation_terra'); assignments=json.loads(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave13/R17_SOURCE_ASSIGNMENTS.json').read_text()); task=next(t for t in assignments['tasks'] if t['name']=='physics_deformation_terra')
def sha(b):return hashlib.sha256(b).hexdigest()
def filehash(p):return sha(pathlib.Path(p).read_bytes())
def git(*x):return subprocess.check_output(['git',*x]).decode().strip()
def blob(rev,p):return subprocess.check_output(['git','show',rev+':'+p])
def counts(b):return {'crlf':b.count(b'\r\n'),'lf_total':b.count(b'\n'),'bare_lf':b.count(b'\n')-b.count(b'\r\n')}
report=json.loads((art/'gate3/compile-results.json').read_text()); results=[]
for r in report['results']:
    bindings=[];missing=[]
    for line in pathlib.Path(r['log']).read_text(errors='replace').splitlines():
        if line.startswith('Note: including file:'):
            path=line[len('Note: including file:'):].strip()
            try: bindings.append({'path':path,'sha256':filehash(path)})
            except OSError: missing.append(path)
    assert not missing, missing
    assert r['passed'] and r['machine']==498
    results.append({**r,'object_sha256':filehash(r['object']),'assembly_sha256':filehash(r['assembly']),'log_sha256':filehash(r['log']),'include_bindings':bindings})
sources=[]
for p in task['sources']:
    old=blob('2beeee2',p);new=blob('HEAD',p);raw=(repo/p).read_bytes()
    sources.append({'path':p,'baseline_git_blob':git('rev-parse','2beeee2:'+p),'baseline_git_blob_sha256':sha(old),'baseline_blob_line_endings':counts(old),'baseline_worktree_raw_sha256':None,'baseline_worktree_raw_note':'not captured before edit','final_git_blob':git('rev-parse','HEAD:'+p),'final_git_blob_sha256':sha(new),'final_worktree_git_blob':git('hash-object',p),'final_worktree_raw_sha256':sha(raw),'final_line_endings':counts(raw),'baseline_failure_log':task['baseline_logs'][p],'baseline_failure_log_sha256':filehash(task['baseline_logs'][p])})
evidence=[]
for a in ['825C0E70','825C0F08','825C0FB8','826053F8','825E0C78','82609278','82604DE0','825DB418','825DB190','82604948']:
 p=pathlib.Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')/('0x'+a+'.json');evidence.append({'path':str(p),'sha256':filehash(p)})
compiler=report['results'][0]['command'][0];script=repo/'tools/build_x360.py'; header='src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h'
out={'task':task['name'],'base':assignments['baseline'],'previous_head':task['previous_head'],'prep_commit':'3a25b4dd2ac7a80796718f555d29a83360d7a13e','prep_excluded_from_future_intake':True,'required_dependency':'Parts owner actual header/definition commit must be integrated first; header blob must match 9702c4b521cb9a6fbd30cf32e7ca91d3b6dac68a','header_baseline_git_blob':git('rev-parse','2beeee2:'+header),'header_baseline_git_blob_sha256':sha(blob('2beeee2',header)),'header_baseline_worktree_raw_sha256':None,'header_final_git_blob':git('rev-parse','HEAD:'+header),'header_final_worktree_raw_sha256':filehash(repo/header),'final_commit':git('rev-parse','HEAD'),'final_tree':git('rev-parse','HEAD^{tree}'),'branch':git('branch','--show-current'),'worktree_clean':not bool(git('status','--porcelain')),'sources':sources,'gate':{'report':str(art/'gate3/compile-results.json'),'report_sha256':filehash(art/'gate3/compile-results.json'),'build_script':str(script),'build_script_sha256':filehash(script),'compiler':compiler,'compiler_sha256':filehash(compiler),'temp':str(art/'temp3'),'jobs':1,'timeout_seconds':180,'results':results},'prior_gates':[{'report':str(art/'gate1/compile-results.json'),'sha256':filehash(art/'gate1/compile-results.json'),'status':'4/4 pass with baseline header'},{'report':str(art/'gate2/compile-results.json'),'sha256':filehash(art/'gate2/compile-results.json'),'status':'1/1 pass with baseline header before last source edit'}],'primary_evidence':evidence,'operation_notes':['825C0EFC/0FA8/0FAC/1044: lane broadcasts and packed multiply; 82605528–58C: transpose and fused translation; 825E0CC0–CFC: ordered compare and select.','826095E8–5FC and 8260962C/6BC–6D4: VMX min reduction and selected padding.','82605340–53B8: native reciprocal refinement and fused point projection; 825DB430–5C4: packed sphere mean, rsqrt, reciprocal and acos input.','Raw image reader sentinels 82815570=7C8A2378 and 82F264A8=00000004; initializer 82C5B7E0–B804 loads flt_820047C8=3D4CCCCD and stores 82FB9660.','Native contact type and real CgsGeometric::Sphere mData replace prior type casts. Removed marked PC diagnostics and guards; native assertions retained.'],'limitations':['Compile-only; no link, runtime or full parity verification.','VehicleContactFixUp retains pre-existing scalar sin/cos, square-root and other scalarized calculations outside the native sequences documented above; numerical parity of those paths is unverified.','Pre-edit raw working-tree hashes were not captured; baseline Git blob hashes are recorded without claiming checkout line endings.']}
assert out['worktree_clean']; (art/'DELIVERY.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print('receipt',filehash(art/'DELIVERY.json'),'bindings',sum(len(r['include_bindings']) for r in results),'tree',out['final_tree'])

