"""Exact Root-released R21 four-original layout collection."""
from pathlib import Path
import hashlib,json,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r21-layout-plan-20261103';B=H.parent/'repair-r20-intake-20261103';A=H.parent.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
plan=read(Q/'FINAL_READONLY_PLAN.json');composition=read(Q/'ordered-composition-and-preimages.json');prior=read(B/'campaign.json');W=Path(prior['workspace']);CAN=Path(prior['canonical']);P=CAN.parent
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob(p):return git('rev-parse','HEAD:'+p).decode().strip()
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==prior['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
guards(plan['base']);assert sha(Q/'FINAL_READONLY_PLAN.json')=='a4ed2a4acfcd8faf4c9a9373efb46bd829f662554fdc22b0780cfeb01f925b2a'
manifest=A/'wave17/R21_LAYOUT_CANDIDATES_DRAFT.json';preflight=A/'wave17/r21-preflight-luna/R21_LAYOUT_PREFLIGHT_DRAFT.json';review=A/'wave17/r21-layout-fresh-review-terra/REVIEW.json';closure=A/'wave17/r21-preflight-luna/header-closure.json'
assert sha(manifest)=='cd2d4afe3394b114640b0de24d25d491bea6e44b37f3259afe814660f89f4cd5'
assert sha(preflight)=='d144d5548cba8826fb17e6abcf2ce5d9fff7cf10aae4195ab0f9896a6bc61fb1'
assert sha(review)=='7610aadf907aea7820af92deea469788a140eac9e1516d3d93143b5d1131796d'and read(review)['verdict']=='pass'
m=read(manifest);assert [c for g in m['groups']for c in g['commits']]==plan['commits']==[c['commit']for c in composition['commits']]
assert set(m['unique_paths'])==set(plan['unique_paths'])and len(m['path_records'])==22
for row in m['path_records']:
 image=composition['source_images'][row['path']];assert row['sealed_R20_blob']==image['baseline']and row['final_git_blob']==image['final']
for row in plan['sealed_author_receipts']:assert sha(row['path'])==row['sha256']
C={'base':plan['base'],'workspace':str(W),'canonical':str(CAN),'published_base':prior['published_base'],'plan':str(Q/'FINAL_READONLY_PLAN.json'),'composition':str(Q/'ordered-composition-and-preimages.json'),'manifest':str(manifest),'preflight':str(preflight),'review':str(review),'preflight_closure':str(closure),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R20.json'),'baseline_report':str(B/'composite-active-native.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native36-command.json'),'baseline_guard_retention':str(B/'native-safeguard-current-reuse.json'),'baseline_named_gates':prior['baseline_named_gates'],'baseline_SDK_provider_pins':prior['baseline_SDK_provider_pins'],'jobs':8,'fresh':179,'retained':3437,'native':3616,'text':6305,'SDK_refs':2}
for k,v in list(C.items()):
 if k in ['plan','composition','manifest','preflight','review','preflight_closure']or k.startswith('baseline_'):C[k+'_sha256']=sha(v)
save(H/'campaign.json',C)
save(H/'R21_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'manifest_sha256':C['manifest_sha256'],'preflight_sha256':C['preflight_sha256'],'review_sha256':C['review_sha256'],'commits':plan['commits'],'authorization':'Root releases EXACT b856→ef8→ba058→3937 layout originals,22paths only. Fresh179/jobs8/all9/showIncludes/Donly plus3437verified unaffected;7guards retained,originalGameguards active. Preserve3615oldPASS,2SDKrefs separate. No22795again,repreps,wave17linkrepair,fullreplay/link/canonical/index/main/dev/origin/ledger/publication.','prior_plan_manifest_metadata_superseded':plan['manifest'],'current_manifest_metadata':{'path':str(manifest),'sha256':sha(manifest)},'all22_source_images_unchanged_from_reviewed_plan':True})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy()};path=H/'intake-receipts.json';save(path,state)
for c in composition['commits']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent'];assert sorted(git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines())==sorted(d['path']for d in c['paths'])
 for d in c['paths']:assert blob(d['path'])==d['preimage_blob']
 r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if r.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected source conflict;bounded stop without resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert blob(d['path'])==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==sorted(plan['unique_paths'])
for p,v in composition['source_images'].items():assert blob(p)==v['final']
patch=git('diff','--binary',C['base']);state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8')
save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'exact4originals':plan['commits'],'paths':plan['unique_paths'],'all22_final_postimages_verified':True,'raw_endpoints':23,'added_or_removed_paths':[],'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R21',state['head'],'10CPP12headers',flush=True)
