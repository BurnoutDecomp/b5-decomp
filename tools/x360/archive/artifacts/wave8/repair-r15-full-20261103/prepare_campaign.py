"""Prepare exact R15 source metadata;no source collection or compile authorization implied."""
from pathlib import Path
from datetime import datetime,timezone
import subprocess,json,hashlib
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r14-physics-1040-resume-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');BASE='7d2b120e6124e98f9187d9c4d18322536bd0c4f9'
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
manifestpath=A/'wave11/R15_CANDIDATES.json';m=read(manifestpath);assert m['baseline']==BASE
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R14.json')=='18d16f5096fdb7c7b9b5398c952695fa3ec0c6e7c162787c65c309bcfa250de4'
assert sha(B/'composite-effective.json')=='992a0c5b32d77fafbecf95a343c3da6c30ff5147ff7dd8d21107a8abead4bcc6'
assert sha(B/'composite-artifact-audit.json')=='681b8461bf97d97dbcd11a27d69859fef5f07fb5a79f8329f70a921937416d95'
review=A/'wave11/r15-fresh-review-terra/REVIEW.md';assert sha(review)=='335cb1bb1dfb0e7484afcd6c90bbe87f8062ecec02d1d5b799ba83f1ed1cc0eb'
images={};candidates=[];receipts=[];owners={}
for group in m['groups']:
 for rel in group['receipts']:receipts.append({'path':str(A/rel),'sha256':sha(A/rel),'group':group['name']})
 for c in group['commits']:
  changes=[];parent=git('rev-parse',c+'^').decode().strip()
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,old,new,status=info.split();assert status in ['M','A']and Path(p).suffix.lower()in ['.cpp','.h','.hpp','.inl','.inc','.ipp']
   if p in images:assert owners[p]==group['name']and images[p]['final']==old
   elif status=='A':
    assert p in m['expected_new_headers']and old=='0'*40;r=subprocess.run(['git','cat-file','-e',BASE+':'+p],cwd=W,capture_output=True);assert r.returncode!=0;images[p]={'baseline':None,'final':old};owners[p]=group['name']
   else:assert git('rev-parse',BASE+':'+p).decode().strip()==old;images[p]={'baseline':old,'final':old};owners[p]=group['name']
   images[p]['final']=new;changes.append({'path':p,'preimage':None if old=='0'*40 else old,'postimage':new,'status':status})
  candidates.append({'commit':c,'parent':parent,'group':group['name'],'paths':changes})
cpp=sorted(p for p in images if Path(p).suffix.lower()=='.cpp');headers=sorted(set(images)-set(cpp));new=sorted(p for p,v in images.items()if v['baseline']is None)
assert len(candidates)==15 and len(images)==52 and len(cpp)==40 and len(headers)==12 and new==m['expected_new_headers']
plan={'state':'PREPARATION_ONLY','base':BASE,'manifest':str(manifestpath),'manifest_sha256':sha(manifestpath),'commits':[c['commit']for c in candidates],'candidates':candidates,'receipts':receipts,'paths':sorted(images),'planned_source_images':images,'changed_CPP':cpp,'changed_headers':headers,'new_headers':new,'native_inventory':3616,'text_inputs_before':6302,'text_inputs_after':6303,'baseline_passed':3342,'baseline_failed':274,'review':str(review),'review_sha256':sha(review),'parent_gitlink_index':'160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp','limits':m['required_limits'],'created_utc':datetime.now(timezone.utc).isoformat()};save(H/'R15_FINITE_PLAN.json',plan)
C={'base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','workspace':str(W),'canonical':str(P/'b5-decomp'),'plan':str(H/'R15_FINITE_PLAN.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R14.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native-consumers-command.json'),'baseline_named_gates':str(B/'named-safeguard-results.json'),'preflight':str(A/'wave11/r15-preflight-luna/FINAL.json')}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:C[k+'_sha256']=sha(C[k])
save(H/'campaign.json',C);save(H/'preparation-receipt.json',{'state':'PREPARATION_ONLY','head':BASE,'intake_clean_detached':True,'commits':15,'groups':10,'CPP':40,'headers':12,'paths':52,'new_headers':new,'expected_text_inputs':6303,'expected_native_inventory':3616,'plan_sha256':C['plan_sha256'],'campaign_sha256':sha(H/'campaign.json'),'source_modified':False,'compiler_started':False,'awaiting':'Separate Root authorization binding final metadata preflight and exact plan.'})
print('PREPARED_R15','15commits/10groups/52paths40CPP12headers','plan',C['plan_sha256'])
