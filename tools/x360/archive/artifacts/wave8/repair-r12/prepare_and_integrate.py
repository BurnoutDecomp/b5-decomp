"""Pin and collect Root-authorized exact R12 four-commit/20CPP batch."""
from pathlib import Path
from datetime import datetime,timezone
import json,hashlib,subprocess,sys
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r11';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');CAN=P/'b5-decomp'
BASE='e572f7db4c533546832e42c6f3f73149071b5f82';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4';COMMITS=['53940edcc490355220554bdfa0da1eb63f6fe54e','e0cf39de404c11c71cbcf127e768f96df186fed5','ff0e2beddace3108448cb6a1c77c58cf4262f52c','d8a66c8dcc9d066052739f14b5d2e29eb561fbbd']
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def guards(head):
 assert git('rev-parse','HEAD').decode().strip()==head and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
 assert git('rev-parse','HEAD',cwd=CAN).decode().strip()==PUB and not git('status','--porcelain=v1',cwd=CAN).strip()
 assert git('ls-files','--stage','b5-decomp',cwd=P).decode().strip()=='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp'
guards(BASE)
prepath=A/'wave9/r12-controls-preflight-luna-20261102/preflight.json';assert sha(prepath)=='c12c5cb9fb28ddf22d10f3290343887aba25364196535f06ce4ce9c9123aabf3';pre=read(prepath);assert [r['commit']for r in pre['candidate_commits']]==COMMITS
assert sha(B/'FROZEN_FULL_DIAGNOSTIC_R11.json')=='c93575e8c416a05f8d051464f50a93b19088c1e93e0502b38aee0821dbaf2014'
with(H/'R12_REVIEWED_PREFLIGHT_01.json').open('xb')as f:f.write(prepath.read_bytes())
a=read(B/'full-stage1-terminal-audit.json');old={r['source']:r for r in read(a['report'])['results']};candidates=[];images={}
for c in pre['candidate_commits']:
 assert sha(c['receipt_path'])==c['receipt_sha256']
 assert git('rev-parse',c['commit']+'^').decode().strip()=='1598404ea16b7e16cbe00396bbd051e38b0d59f4'
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c['commit']).decode().splitlines():
  info,p=line.split('\t',1);_,_,oldblob,newblob,status=info.split();assert status=='M'and p.endswith('.cpp')and p not in images
  assert git('rev-parse',BASE+':'+p).decode().strip()==oldblob
  images[p]={'baseline':oldblob,'final':newblob};changes.append({'path':p,'preimage':oldblob,'postimage':newblob,'status':status})
 assert len(changes)==5 and sorted(p['path']for p in changes)==sorted(c['paths'])
 candidates.append({'commit':c['commit'],'label':c['subject'],'paths':changes,'receipt':c['receipt_path'],'receipt_sha256':c['receipt_sha256']})
assert len(images)==20;paths=sorted(images)
plan={'state':'ROOT_AUTHORIZED_EXACT_FOUR_SOURCE_ONLY','base':BASE,'published_base':PUB,'proposed_originals':COMMITS,'collect_now':COMMITS,'candidates':candidates,'unique_paths':paths,'planned_source_images':images,'owned_CPP_sources':paths,'changed_headers':[],'new_paths':[],'removed_paths':[],'selected_union_native_sources':paths,'selected_requested':20,'selected_R11_passes':sum(old[p]['passed']for p in paths),'selected_R11_failed':sum(not old[p]['passed']for p in paths),'expected_native_inventory':3616,'expected_native_text_inputs':6302,'parent_gitlink_index':'160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp','reviewed_preflight_sha256':sha(prepath),'known_limits':pre['remaining_work'],'authorization':'Explicit Root task authorizes exact4 collection,20 fresh native gates and verified unaffected3596 R11 composite. No extra source/header/link/publication/ledger/index work.','created_utc':datetime.now(timezone.utc).isoformat()}
save(H/'R12_PREFLIGHT_PLAN_01.json',plan)
cfgold=read(B/'campaign.json');C={'campaign':'wave8-repair-r12','base':BASE,'published_base':PUB,'workspace':str(W),'canonical':str(CAN),'artifact_root':str(H),'baseline_metadata':str(B/'native-parallel-wave8-r11-full-stage1-patch.json'),'baseline_report':a['report'],'baseline_artifact_manifest':str(B/'full-stage1-terminal-audit.json'),'published_report':cfgold['published_report'],'published_artifact_manifest':cfgold['published_artifact_manifest'],'baseline_passed':3265,'baseline_failed':351,'baseline_requested':3616,'current_plan':str(H/'R12_PREFLIGHT_PLAN_01.json')}
for k in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:C[k+'_sha256']=sha(C[k])
save(H/'campaign.json',C);save(H/'R12_ROOT_AUTHORIZATION_01.json',{'authorization':plan['authorization'],'plan_sha256':sha(C['current_plan']),'preflight_sha256':sha(prepath),'originals':COMMITS})
state={'state':'integrating','base':BASE,'head':BASE,'workspace':str(W),'receipts':[],'todo':COMMITS.copy()};statepath=H/'repair-r12-receipts.json';save(statepath,state)
for c in candidates:
 guards(state['head'])
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['preimage']
 r=subprocess.run(['git','cherry-pick',c['commit']],cwd=W,capture_output=True,text=True)
 if r.returncode:
  state.update(state='conflict',pending=c['commit'],stdout=r.stdout,stderr=r.stderr);statepath.write_text(json.dumps(state,indent=2)+'\n');raise RuntimeError(r.stderr)
 state['head']=git('rev-parse','HEAD').decode().strip()
 for d in c['paths']:assert git('rev-parse','HEAD:'+d['path']).decode().strip()==d['postimage']
 state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[d['path']for d in c['paths']],'outcome':'normal cherry-pick,exact pre/postimages verified','receipt_sha256':c['receipt_sha256']});state['todo'].pop(0);statepath.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8');print('COLLECTED',c['commit'],'->',state['head'],flush=True)
guards(state['head']);assert sorted(git('diff','--name-only',BASE).decode().splitlines())==paths
state['state']='complete';statepath.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8')
save(H/'intake-source-binding.json',{'head':state['head'],'base':BASE,'originals':COMMITS,'receipts':4,'paths':paths,'all20_final_postimages_verified':True,'patch_sha256':hashlib.sha256(git('diff','--binary',BASE)).hexdigest(),'scope':'Exact4 normal cherry-picks;no conflicts,extra source,header,new file,inventory or index change.'})
save(H/'INTEGRATION_OWNER.json',{'owner':'/root/r12_integrator','previous_C_owner_record_retained':str(P/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json'),'head':state['head'],'phase':'R12 frozen exact4/20CPP','goal':'ACTIVE','live_process_id':None})
print('FROZEN_R12',state['head'],'prior selection',plan['selected_R11_passes'],'pass',plan['selected_R11_failed'],'fail',flush=True)
