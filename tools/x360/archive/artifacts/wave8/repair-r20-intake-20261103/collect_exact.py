"""Root-released exact two-commit R20 intake;no typed/layout partials."""
from pathlib import Path
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;Q=H.parent/'repair-r20-plan-20261103';A=H.parent.parent;C0=json.loads((Q/'campaign-preparation.json').read_text());plan=json.loads(Path(C0['plan']).read_text());W=Path(C0['workspace']);CAN=Path(C0['canonical']);P=CAN.parent
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def blob(path):
 r=subprocess.run(['git','rev-parse','HEAD:'+path],cwd=W,capture_output=True,text=True);return r.stdout.strip()if r.returncode==0 else None
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip();assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==C0['published_base']and not git('status','--porcelain=v1',cwd=CAN).strip();assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()==plan['parent_gitlink_index']
guards(C0['base']);assert sha(C0['plan'])==C0['plan_sha256']=='2c6336ac238552d6400cd461f56657950995c8fe7df3b231363207a75db5e37d';assert sha(plan['manifest'])==plan['manifest_sha256'];assert plan['commits']==['3019269372a86cf2d1e386aa781a5dc70012cbdd','b1422cc53f622174d2b75271bc856ef33ce9c468']
pre=A/'wave16/r20-preflight-luna/FINAL.json';review=A/'wave16/r20-fresh-review-terra/REVIEW.md';closure=A/'wave16/r20-preflight-luna/header-closure.json';assert sha(pre)=='5248a0f284b76637ef5af685d23ed9d07b4409dc308da3bed4ee27dbc521b9bb';assert sha(review)=='debbb2a0c382711a85ba2399f7c4a82c452920d42bd37f8a61117800d023aecb'and 'VERDICT: pass'in review.read_text()
for r in plan['receipts']:assert sha(r['path'])==r['sha256']
C={**C0,'preflight':str(pre),'preflight_sha256':sha(pre),'review':str(review),'review_sha256':sha(review),'preflight_closure':str(closure),'preflight_closure_sha256':sha(closure),'jobs':8,'expected_fresh':36,'expected_retained':3580,'native_before':3615,'native_after':3616,'text_before':6303,'text_after':6305,'SDK_refs':2};save(H/'campaign.json',C);save(H/'R20_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'base':C['base'],'plan_sha256':C['plan_sha256'],'preflight_sha256':sha(pre),'review_sha256':sha(review),'commits':plan['commits'],'jobs':8,'authorization':'Root releases Vertex301926 thenEntryb142 only,5paths3CPP2h. Freshfinite36/max8/all9/showIncludes/Donly,realnewprovider/3616native/6305text,2SDKrefsseparate. Freeze inputs/context,verifyunchangedR19rowsonce,oldShadowrepair/newprovider/Entrycontrolcorrectnessseparate;preserve3613priorPASS,Mainhardfailurehonest. Retainoldproof;no fullrecompile/ICEReplaypartials/layout/gameLink/canonical/index/ledger/main/dev/origin.'})
state={'state':'integrating','base':C['base'],'head':C['base'],'receipts':[],'todo':plan['commits'].copy()};path=H/'intake-receipts.json';save(path,state)
for c in plan['candidates']:
 guards(state['head']);assert git('rev-parse',c['commit']+'^').decode().strip()==c['parent'];raw=git('diff-tree','--no-commit-id','--name-only','-r',c['commit']).decode().splitlines();assert sorted(raw)==sorted(x['path']for x in c['paths'])
 for d in c['paths']:assert blob(d['path'])==d['preimage_blob']
 r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if r.returncode:state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError('Unexpected sourceconflict;no automatic resolution')
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert blob(d['path'])==d['postimage_blob']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',C['base']).decode().splitlines())==plan['paths']
for p,v in plan['planned_source_images'].items():assert blob(p)==v['final']
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',C['base']);save(H/'intake-source-binding.json',{'head':state['head'],'base':C['base'],'exact2originals':plan['commits'],'paths':plan['paths'],'all5postimagesverified':True,'newCppAndHeader':plan['added_paths'],'no_typed_layout_partial_intake':True,'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R20',state['head'],'3CPP2h/newprovider1',flush=True)
