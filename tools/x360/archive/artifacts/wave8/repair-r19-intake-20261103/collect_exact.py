"""Exact Root-released R19 source collection and explicit SDK-reference moves."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r19-plan-20261103';A=H.parent.parent;C0=json.loads((Q/'campaign-preparation.json').read_text());plan=json.loads(Path(C0['plan']).read_text());W=Path(C0['workspace']);CAN=Path(C0['canonical']);P=CAN.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob(path):
 r=subprocess.run(['git','rev-parse','HEAD:'+path],cwd=W,capture_output=True,text=True);return r.stdout.strip()if r.returncode==0 else None
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip();assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C0['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip();assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
guards(C0['base']);assert sha(C0['plan'])==C0['plan_sha256']=='6216afe20ee77dc048ef06d2571dd5040bd7d6b520ceb14658aa42ded06120b6';assert sha(plan['manifest'])==plan['manifest_sha256']=='b49edb2d88eda97b3e07d64ea743e41a89cae0467afbf93d1063ae8f908b3907'
pre=A/'wave15/r19-preflight-luna/FINAL.json';review=A/'wave15/r19-fresh-review-terra/REVIEW.md';assert sha(pre)=='5661612b858c423d94dc40ad0c677cb27c8a21558d83042f2783f22d5f0e8079';assert sha(review)=='21c90ceef2a0fd7fe5c18b3350c5279f0adedd25b84e31a815c46ffba52c5903'and 'VERDICT: pass'in review.read_text()
for r in plan['receipts']:assert sha(r['path'])==r['sha256']
assert len(plan['commits'])==16 and not any(c.startswith(('0d9a0653f','e506a05'))for c in plan['commits']);assert plan['commits'][-3:]==['3e4cc2042a8a825337980a5c0a502d4a07fc738b','7cfe919c7f58307e4eadf3303b22ea750a250ad3','2af72c111f7fd42de581f182db7b15352b799a5a']
C={**C0,'preflight':str(pre),'preflight_sha256':sha(pre),'review':str(review),'review_sha256':sha(review),'jobs':8,'expected_fresh':116,'expected_retained':3499,'expected_native':3615,'expected_native_text':6303,'SDK_reference_units':2};save(H/'campaign.json',C);save(H/'R19_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'preflight_sha256':sha(pre),'review_sha256':sha(review),'commits':plan['commits'],'jobs':8,'authorization':'Root releases exact16originals14groups,all87manifest/91endpoints,4R100reference moves,SDKpolicy/archive/proof pins. Native3617to3615/text6307to6303;2oldFAIL SDKreferences NOTcompiler gains,broader3617dispositionspreserved. Fresh116jobs8/all9/showIncludes/Donly;3499activeunaffectedonce/realorigins;preserve3544priorPASS,69oldrepairpotentialseparate. Retain7guardswithinputproof;no layoutrecoveries/full/gameLink/canonical/index/main/dev/ledger/originpublication. Holdregressionsforownedrepair.'})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy(),'move_proofs':[]};path=H/'intake-receipts.json';save(path,state)
for c in plan['candidates']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent'];actualraw=git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r','-M',c['commit']).decode().splitlines();assert len(actualraw)==len(c['changes']);working_before={}
 for d in c['changes']:
  if d['status']=='R100':
   assert blob(d['source'])==d['preimage_blob']==d['postimage_blob']and blob(d['destination'])is None;working_before[d['source']]={'sha256':sha(W/d['source']),'bytes':(W/d['source']).stat().st_size}
  else:assert blob(d['path'])==d['preimage_blob']
 run=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=run.stdout,stderr=run.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected collection conflict;no automatic resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['changes']:
  if d['status']=='R100':
   assert blob(d['source'])is None and blob(d['destination'])==d['postimage_blob'];post={'sha256':sha(W/d['destination']),'bytes':(W/d['destination']).stat().st_size};assert post==working_before[d['source']];state['move_proofs'].append({'source':d['source'],'reference':d['destination'],'Git_blob':d['postimage_blob'],'physical_working_bytes_before_after_identical':True,**post})
  else:assert blob(d['path'])==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'group':c['group'],'changes':c['changes']});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only','--no-renames',C['base']).decode().splitlines())==plan['all_affected_path_endpoints'];assert len(state['move_proofs'])==4
for p,v in plan['planned_source_images'].items():assert blob(p)==v['final']
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary','--no-renames',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'original_commits':plan['commits'],'manifest_paths87':plan['manifest_canonical_paths'],'all_endpoints91':plan['all_affected_path_endpoints'],'all_final_source_reference_policy_postimages_verified':True,'move_R100_proofs':state['move_proofs'],'SDK_reference_units_not_compiler_gains':2,'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R19',state['head'],'87manifest/91endpoints/4identicalmoves',flush=True)
