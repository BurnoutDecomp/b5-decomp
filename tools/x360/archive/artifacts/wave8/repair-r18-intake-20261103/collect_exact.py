"""Exact Root-released R18 collection;18 originals only,no wrong-ABI/Main/R19 variants."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r18-plan-20261103';A=H.parent.parent;C0=json.loads((Q/'campaign-preparation.json').read_text());plan=json.loads(Path(C0['plan']).read_text());W=Path(C0['workspace']);CAN=Path(C0['canonical']);P=CAN.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C0['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
guards(C0['base']);assert sha(C0['plan'])==C0['plan_sha256']=='b4d21452e6fe7f3923f9f5dfc38a8fc2852716c4c39aabe02c66ec79e98f210c';assert sha(plan['manifest'])==plan['manifest_sha256']=='951ca7086061a2eb6991b83d2fd8c4802bf54f2a6f801cd729443f31c73a6d96';assert sha(plan['preflight'])=='5b8697e7fb998da8735bf91df20e9a1d8a3b6237cbfb9ace1ae3d30bb88cf9b2'
recon=A/'wave14/r18-preflight-luna/INTAKE_GATE_RECONCILIATION.json';assert sha(recon)=='ebd05ea676acf0b0218f3712c4da5efcc68c9e0ea1a9a0d3b5aeac01cb5f0ea0';rec=read(recon);assert rec['selected_gate_union_count']==385 and rec['header_consumer_count']==337
assert sha(plan['review'])=='d1788e29c7f6058ef6266b6ae925a9b1da57983db5efc0ca2aea46bcf850b6e3'and 'VERDICT: pass'in Path(plan['review']).read_text()
for item in plan['receipts']:assert sha(item['path'])==item['sha256']
assert len(plan['commits'])==18 and '22795e60f07c044e0a6ada8c3f3e9ef3ec49c4e3'in plan['commits']and not any(c.startswith(('e7d','b856','093fc91'))for c in plan['commits'])
C={**C0,'preflight':plan['preflight'],'preflight_sha256':sha(plan['preflight']),'review':plan['review'],'review_sha256':sha(plan['review']),'reconciliation':str(recon),'reconciliation_sha256':sha(recon),'jobs':8,'expected_fresh':385,'expected_retained':3232,'expected_native':3617,'expected_text_inputs':6307};save(H/'campaign.json',C)
save(H/'R18_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'preflight_sha256':C['preflight_sha256'],'reconciliation_sha256':sha(recon),'review_sha256':C['review_sha256'],'commits':plan['commits'],'jobs':8,'authorization':'Root releases exact18originals16groups91paths82CPP9headers,finite385incl12conservativeFS/audioextras,3232retained. Main22795only,Replay19e→6b50only,Graphicsca951→c131;noR19/policy/moves/excludedvariants. Postcollection3617/6307/385proof;all9/showIncludes/Donly/jobs8;onceunaffectedproof andrealorigins;preserve3464priorPASS. Only3materialguardsfresh/4retained. Sealif0regressions,otherwiseownedrepairdiagnostic. No full/link/CRT/runtime/globalbyteclaim/canonical/index/ledger/main/dev/origin/publication.'})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy()};path=H/'intake-receipts.json';save(path,state)
for c in plan['candidates']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent'];raw=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(raw)==sorted(d['path']for d in c['paths'])
 for d in c['paths']:assert d['status']=='M'and git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage_blob']
 run=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=run.stdout,stderr=run.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected source conflict;no automatic resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'group':c['group'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['paths']
for p,v in plan['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).decode().strip()==v['final']
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'original_commits':plan['commits'],'paths':plan['paths'],'all91finalpostimages_verified':True,'added_or_removed_paths':[],'Main22795only':True,'Replay19e_before6b50':True,'no_shadowingdevice_CPP_edit':True,'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R18',state['head'],'82CPP9headers/net91',flush=True)
