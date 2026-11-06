"""Root-authorized exact four-commit Physics intake;no other candidates."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess
H=Path(__file__).resolve().parent;B=H.parent/'repair-r13-prep-20261103';A=H.parent.parent;W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');CAN=P/'b5-decomp';BASE='7460ae2bc101aacca52e184a892f6dcfc76ee94f';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4'
COMMITS=['3dea2a28a784097d2df2ae73c685d75a4eac5f39','b787dda687aeb5d5cbde636b98420737b887773a','cd4f459f22f56bfe3dc1d43fa81bc920f2b1f8e9','801e88837926970f94c670d094f20f9a52c11fe1']
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==PUB and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()=='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp'
guards(BASE)
pf=A/'wave10/physics-intake-preflight-luna/PHYSICS_INTAKE_PREFLIGHT.json';assert sha(pf)=='b63a7a66e134ae3d2f563a49342e3b7e32456204204b767c7ccc95624c437987';pre=read(pf);assert [c['commit']for c in pre['candidate']['chain']]==COMMITS
receipt=A/'wave10/physics-contact-sign-correction/DELIVERY.json';assert sha(receipt)=='ec5db55e3704888ec143f0d6f71e121c9d07eb183481a369fdb312e6d5749b30'
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.json')=='d9611d1bd98c37d66b683b7bd2a950fac8758e37b263217982a7a98c280e25ee'
assert sha(B/'composite-effective.json')=='aecf04820635d06f14b07cd1b1b63f03166fc3e85b2ac74b641eabb0b8e49761'
assert sha(B/'composite-artifact-audit.json')=='a1e5387b4a9d860d88c8abd7272f80caca262bd177b766f4945d452aa87b1ead'
images={};candidates=[]
for i,c in enumerate(COMMITS):
 parent=git('rev-parse',c+'^').decode().strip();assert parent==(COMMITS[i-1]if i else '1598404ea16b7e16cbe00396bbd051e38b0d59f4');changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
  info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status=='M'and Path(p).suffix.lower()in ['.cpp','.h','.hpp']
  if p in images:assert images[p]['final']==a
  else:assert git('rev-parse',BASE+':'+p).decode().strip()==a;images[p]={'baseline':a,'final':a}
  images[p]['final']=b;changes.append({'path':p,'preimage':a,'postimage':b,'status':status})
 candidates.append({'commit':c,'parent':parent,'paths':changes})
assert sorted(images)==sorted(pre['candidate']['cumulative_paths'])and len(images)==17
plan={'base':BASE,'commits':COMMITS,'candidates':candidates,'paths':sorted(images),'planned_source_images':images,'changed_CPP':pre['candidate']['changed_cpp'],'changed_headers':pre['candidate']['changed_headers'],'expected_preflight_sources':[r['path']for r in pre['consumer_closure']['future_affected_cpp']],'expected1040':1040,'baseline_passes':3333,'baseline_failed':283,'native_inventory':3616,'text_inputs':6302,'parent_gitlink_index':'160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp','preflight':str(pf),'preflight_sha256':sha(pf),'author_successor_receipt':str(receipt),'author_successor_receipt_sha256':sha(receipt),'limits':pre['limits'],'created_utc':datetime.now(timezone.utc).isoformat()}
save(H/'R14_FINITE_PLAN.json',plan)
C={'base':BASE,'published_base':PUB,'workspace':str(W),'canonical':str(CAN),'plan':str(H/'R14_FINITE_PLAN.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R13.json'),'baseline_report':str(B/'composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'source-metadata.json'),'baseline_driver_command':str(B/'native48-command.json'),'baseline_named_gates':str(H.parent/'repair-r11/named-native-gates/gate-results.json')}
for k in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_named_gates']:C[k+'_sha256']=sha(C[k])
save(H/'campaign.json',C);save(H/'R14_ROOT_AUTHORIZATION.json',{'root_explicit_authorization':True,'plan_sha256':C['plan_sha256'],'preflight_sha256':sha(pf),'commits':COMMITS,'authorization':'Root explicitly authorizes exact4 Physics chain including required801e correction;post-header closure recomputation,1040 or expanded affected native gates/jobs8/all9/showIncludes,affected named safeguards only,precise composite seal if no regressions or bounded regression diagnostic. No other source/full/link/owner/published-history/canonical/index/ledger action.'})
state={'state':'integrating','base':BASE,'head':BASE,'receipts':[],'todo':COMMITS.copy()};path=H/'intake-receipts.json';save(path,state)
for c in candidates:
 guards(state['head'])
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage']
 r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if r.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);path.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError(r.stderr)
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']]});state['todo'].pop(0);path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',len(state['receipts']),c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',BASE).decode().splitlines())==sorted(images)
state['state']='complete';path.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');patch=git('diff','--binary',BASE)
save(H/'intake-source-binding.json',{'head':state['head'],'base':BASE,'exact4commits':COMMITS,'paths':sorted(images),'final17postimages_verified':True,'required_contact_successor_applied':True,'patch_sha256':hashlib.sha256(patch).hexdigest()})
with(H/'integrated.diff').open('xb')as f:f.write(patch)
print('FROZEN_R14',state['head'],'17paths/10CPP/7headers',flush=True)
