"""Exact Root-released R16 intake,10 originals only;no GUI-mask successor or later source."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r16-plan-20261103';A=H.parent.parent;C0=json.loads((Q/'campaign-preparation.json').read_text());plan=json.loads(Path(C0['plan']).read_text());W=Path(C0['workspace']);CAN=Path(C0['canonical']);P=CAN.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C0['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
guards(C0['base']);assert sha(C0['plan'])==C0['plan_sha256']=='0f361ede6029543403dcb365a82fe7c8a11d4b4ac9fc2f3dd2e022ab7fd1e3b6';assert sha(plan['manifest'])==plan['manifest_sha256'];pre=A/'wave12/r16-preflight-luna/FINAL.json';assert sha(pre)=='aa7c1b989e4db3a9e1a0d8799494c8ffed318e4a5fe6467a09d5502df2cac548';pf=read(pre);assert not pf['issues']and pf['manifest_sha256']==plan['manifest_sha256']and pf['baseline']['commit']==C0['base']
review=A/'wave12/r16-fresh-review-terra/REVIEW.md';assert 'VERDICT: pass'in review.read_text();recon=Q/'ROOT_PREFLIGHT_RECONCILIATION.json';r=read(recon);assert r['selected_native_union']==139 and r['retained']==3477 and r['all_preflight_native_consumers_in_conservative_projection']
for item in plan['receipts']:assert sha(item['path'])==item['sha256']
C={**C0,'preflight':str(pre),'preflight_sha256':sha(pre),'review':str(review),'review_sha256':sha(review),'reconciliation':str(recon),'reconciliation_sha256':sha(recon),'baseline_named_gates':str(H.parent/'repair-r15-full-20261103/named-safeguard-results.json'),'jobs':3,'expected_fresh':139,'expected_retained':3477}
C['baseline_named_gates_sha256']=sha(C['baseline_named_gates']);save(H/'campaign.json',C);save(H/'R16_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'preflight_sha256':sha(pre),'review_sha256':sha(review),'commits':plan['commits'],'jobs':3,'authorization':'Root releases exact10orderedmanifest originals/9groups/41paths,postcollection139nativeclosure/3477retained,all9fallback/showIncludes/jobs3,Donly. Reuse proven unaffectedR15products once;onlymateriallyaffectednativeguards. Sealif0regressions otherwise concretependingdefect. No fullreplayunlessunbounded and reportfirst. GUI-mask3e014 and all later refs excluded. Protectedcanonical/main/dev/parentindex/ledger/origin/publication.'})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy()};path=H/'intake-receipts.json';save(path,state)
for c in plan['candidates']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent']
 changes=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(changes)==sorted(d['path']for d in c['paths'])
 for d in c['paths']:assert d['status']=='M'and git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage_blob']
 run=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=run.stdout,stderr=run.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected source conflict;no automatic resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['paths']
for p,v in plan['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).decode().strip()==v['final']
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'original_commits':plan['commits'],'paths':plan['paths'],'all41finalpostimages_verified':True,'patch_sha256':hashlib.sha256(patch).hexdigest(),'excluded_GUI_successor3e014':True})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R16',state['head'],'40CPP1header',flush=True)
