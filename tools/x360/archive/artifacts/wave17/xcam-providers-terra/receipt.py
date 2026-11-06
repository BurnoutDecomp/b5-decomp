import pathlib,json,hashlib,subprocess
r=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/sound_lane_luna_ef640'); a=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/xcam-providers-terra'); p='src/GameSource/Network/Managers/X360/BrnNetworkCameraX360.cpp'
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
git=lambda *x:subprocess.check_output(['git',*x],cwd=r,text=True).strip()
f=lambda p:{'path':str(p),'sha256':sha(p)}
j=json.loads((a/'gate1/compile-results.json').read_text()); row=j['results'][0]; includes={}
for line in pathlib.Path(row['log']).read_text(errors='replace').splitlines():
 if 'Note: including file:' in line:
  path=line.split('Note: including file:',1)[1].strip(); includes[path]=sha(path) if pathlib.Path(path).is_file() else None
(a/'includes.json').write_text(json.dumps(includes,indent=2)+'\n')
for key in ('object','assembly','log'):row[key]=f(row[key])
res={'task':'xcam-provider-public-api','base':'6be513e1b3b69f9c28ed42a01ffafdc92924f857','final_commit':git('rev-parse','HEAD'),'tree':git('rev-parse','HEAD^{tree}'),'branch':git('branch','--show-current'),'source':p,'pre_git_blob':'22685e1ec9c2e5ac1be735c3f300b21f389812d6','pre_worktree_sha256_measured':'a03860d9a446dd011f85e9b7425077adde9667f26183dc70460e8321e70620fc','post_git_blob':git('rev-parse','HEAD:'+p),'post_worktree_sha256_measured':sha(r/p),'gate':{'jobs':1,'timeout_seconds':180,'script':f(a/'run-gate1.ps1'),'build_script':f(r/'tools/build_x360.py'),'compiler':f(row['command'][0]),'report':f(a/'gate1/compile-results.json'),'console':f(a/'gate1-console.log'),'includes':f(a/'includes.json'),'include_count':len(includes),'missing_includes':[k for k,v in includes.items() if v is None],'result':row,'coff_machine':'0x01F2','coff_external':['XCamGetStatus','undefined']},'policy':f(a/'XCAM_PROVIDER_POLICY.json'),'status_porcelain':git('status','--porcelain'),'limits':['Selected compile only; no link or runtime validation.','SDK variant comparison excludes relocatable bits; called targets, data and runtime remain unverified.','Include hashes measured after gate; shared dependency bytes were not independently snapshotted before gate.']}
(a/'DELIVERY.json').write_text(json.dumps(res,indent=2)+'\n');print('include count',len(includes),'missing',len(res['gate']['missing_includes']),'delivery sha',sha(a/'DELIVERY.json'))
