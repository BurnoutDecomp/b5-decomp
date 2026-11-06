import json, hashlib, subprocess, pathlib, re
root=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\sound_lane_luna_ef640')
a=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave15\infrastructure_terra')
base='8741b36e43dfe0d899dc442533f54131d174b393'
paths=['src/GameSource/Physics/BrnPhysicsModuleIO_InputBuffer.cpp','src/GameSource/Physics/BrnPhysicsModule_layout_check.cpp','src/SDKs/CRT/exception.cpp','src/SDKs/CRT/exception.hpp','src/SDKs/EATech/include/Common/vp6/vp6_startdecoder.cpp']
pre=['b0769c65c91264a4fe43e581753a937f49cf9708ca79b4af4dd748a456297bf7','3a2b6fb1350cdf796b277626e77e1cf303f6fa0c2d257a80b9b4b8ff313c05d3','c761874c177cf62c6bda27a35d0a8cbee64d95a608f7f1b9df59412dbebd1a14','bbd16a27e36b32017ed1851b30ff81aea56348d8aae0bdaae013c24d85bc99cd','3b42ea598bcd9b7ed75d7b7f832e44b91e278ecd6f28363dd674104b73d47746']
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(*args): return subprocess.check_output(['git',*args],cwd=root,text=True).strip()
def file(p): return {'path':str(p),'sha256':sha(p)}
gate=json.loads((a/'gate2/compile-results.json').read_text())
incs={}
for r in gate['results']:
 for line in pathlib.Path(r['log']).read_text(errors='replace').splitlines():
  if 'Note: including file:' in line:
   p=line.split('Note: including file:',1)[1].strip()
   incs[p]=sha(p) if pathlib.Path(p).is_file() else None
 for k in ['object','assembly','log']: r[k]=file(r[k])
manifest=a/'include-manifest.json'; manifest.write_text(json.dumps(incs,indent=2)+'\n')
res={'task':'infrastructure_terra','base':base,'final_commit':git('rev-parse','HEAD'),'final_tree':git('rev-parse','HEAD^{tree}'),'branch':git('branch','--show-current'),'previous_branch':'codex/experimental-x360-r17-gamestate-offences-terra','previous_head':'4dfe2f9b3435b14672cd81f4e1898416058cb9da','paths':[{'path':p,'pre_git_blob':git('rev-parse',base+':'+p),'pre_worktree_sha256_measured':h,'post_git_blob':git('rev-parse','HEAD:'+p),'post_worktree_sha256_measured':sha(root/p)} for p,h in zip(paths,pre)],'gate':{'jobs':1,'timeout_seconds':180,'showIncludes':True,'script':file(a/'run-gate2.ps1'),'compiler':file(gate['results'][0]['command'][0]),'build_script':file(root/'tools/build_x360.py'),'report':file(a/'gate2/compile-results.json'),'console':file(a/'gate2-console.log'),'include_manifest':file(manifest),'include_count':len(incs),'missing_includes':[k for k,v in incs.items() if v is None],'results':gate['results']},'prior_attempt':{'report':file(a/'gate1/compile-results.json'),'console':file(a/'gate1-console.log'),'note':'4 passed; VP6 line endings restored afterward'},'primary_evidence':['ARTIST 0x82B57F28, 0x82B58078 and 0x82B580A4 call 0x82B42CC8; 0x82B42CC8 returns 16.','src/SDKs/EATech/eathread/eathread_semaphore.h describes native 16-byte Semaphore; vendor EAThread GetSemaphoreSize returns sizeof(Semaphore).','Physics layout assertions retained, same-type checks expressed by C++03 pointer-to-member overload.','CRT exception empty exception specifications expressed by matching C++03 throw() declarations and definitions.'],'limits':['Selected compile only; no link, runtime, or comprehensive semantic parity claim.','Exception header consumers require downstream integration compile.','Include hashes measured after gate; shared dependency bytes not separately snapshotted beforehand.'],'status_porcelain':git('status','--porcelain')}
(a/'DELIVERY.json').write_text(json.dumps(res,indent=2)+'\n')
print('include count',len(incs),'missing',len(res['gate']['missing_includes']))
