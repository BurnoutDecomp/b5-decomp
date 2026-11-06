"""Exact Root-released R17 collection;existing Video postimages preserved;no private preparation."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r17-plan-20261103';A=H.parent.parent;C0=json.loads((Q/'campaign-preparation.json').read_text());plan=json.loads(Path(C0['plan']).read_text());W=Path(C0['workspace']);CAN=Path(C0['canonical']);P=CAN.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
def current_blob(path):
 r=subprocess.run(['git','rev-parse','HEAD:'+path],cwd=W,capture_output=True,text=True);return r.stdout.strip()if r.returncode==0 else None
guards(C0['base']);assert sha(C0['plan'])==C0['plan_sha256']=='0ce1a66ca63b153fbb0a753863862e7a891e49cb77507bb644f2028fec700dbc';assert sha(plan['manifest'])==plan['manifest_sha256']=='207eda897503a17739fc06d2e408dd388e85aca92a2d87029aa4e1813d696c60'
pre=A/'wave13/r17-preflight-luna/FINAL.json';assert sha(pre)=='0347ab119e0d529a343db7fd5d9089fcd09f682fd483d3346e3ff6778cf6cbb3';pf=read(pre);assert not pf['integrity']['issues']and pf['integrity']['chains_valid'];rec=pf['integrator_projection_reconciliation'];assert rec['plan_sha256_matches_expected']and rec['union_source_count']==676 and rec['retained_unchanged_count']==2941 and not rec['mismatch_sources_outside_selection']and not rec['unmapped_mismatches']
review=A/'wave13/r17-fresh-review-terra/REVIEW.md';assert sha(review)=='7ecd7fb75ea7b3a4efad8ea1e2b9158c9bf5fd329f450c140e16a0bc8c6321e0'and 'VERDICT: pass'in review.read_text()
for item in plan['receipts']:assert sha(item['path'])==item['sha256']
assert len(plan['commits'])==23 and plan['commits'].index('312bc353d5744c3be63923e14383eb67d0c563f2')<plan['commits'].index('2dd172dc41af44c65f49921b6261fe1d016d218e')and '3a25b4dd2ac7a80796718f555d29a83360d7a13e'not in plan['commits'];assert '58c6db4876809b8e7bea3813e0f78352db3f4913'in plan['commits']
C={**C0,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','preflight':str(pre),'preflight_sha256':sha(pre),'review':str(review),'review_sha256':sha(review),'jobs':8,'expected_fresh':676,'expected_retained':2941,'expected_native':3617,'expected_text_inputs':6307};save(H/'campaign.json',C)
save(H/'R17_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'preflight_sha256':sha(pre),'review_sha256':sha(review),'commits':plan['commits'],'jobs':8,'authorization':'Root releases exact23originals14groups,Parts312beforeDeform2dd,prep3a25excluded,2Videoidenticalnoops,finalNFSMix58cmandatory. Fresh676native/jobs8/all9/showIncludes/Donly,2941verifiedunaffectedR16rows/input/contextonce. Inventory3617/text6307/newNFSLiveLink. Preserve3408priorpasses andrealperroworigins;newproviderseparate. Onlyaffectedguards. No fullreplay/link/R18extras/bypass;holdsealforregressionrepairs. Protectedcanonical/index/main/dev/ledger/origin/publication.'})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy(),'already_identical_video_paths_preserved':[]};path=H/'intake-receipts.json';save(path,state)
allowed_noops={(x['commit'],x['path'])for x in plan['already_identical_video_deltas']}
for c in plan['candidates']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent'];raw=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(raw)==sorted(d['path']for d in c['changes'])
 for d in c['changes']:
  actual=current_blob(d['path'])
  if actual==d['preimage_blob']:pass
  else:assert (c['commit'],d['path'])in allowed_noops and actual==d['postimage_blob'];state['already_identical_video_paths_preserved'].append(d['path'])
 run=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if run.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=run.stdout,stderr=run.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected collection conflict;no automatic resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['changes']:assert current_blob(d['path'])==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'group':c['group'],'raw_paths':[d['path']for d in c['changes']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['net_changed_paths'];assert sorted(state['already_identical_video_paths_preserved'])==plan['already_final_unchanged_paths']
for p,v in plan['planned_source_images'].items():assert current_blob(p)==v['final']
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'original_commits':plan['commits'],'raw_paths':137,'net_paths':135,'net_changed_paths':plan['net_changed_paths'],'all_final137postimages_verified':True,'added_paths':plan['added_paths'],'identical_Video_noops_preserved':state['already_identical_video_paths_preserved'],'prep3a25excluded':True,'finalNFSMix58c_collected':True,'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R17',state['head'],'raw137/net135/add4/videoNoop2',flush=True)
