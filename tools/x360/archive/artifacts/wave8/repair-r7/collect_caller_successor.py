"""Collect only the Root-reviewed three-CPP caller closure after immutable selected FAIL."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');BASE='ff58ecb8dd1102fb3a85d70b9e1e6defc66c5b25'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a,check=True):return subprocess.run(['git','--no-optional-locks','-C',str(WORK),*a],capture_output=True,text=True,encoding='utf-8',errors='replace',check=check)
assert git('rev-parse','HEAD').stdout.strip()==BASE and not git('status','--porcelain=v1').stdout.strip()
failed=read(HERE/'native-parallel-wave8-r7-selected-stage1-patch.json')
for p,h in failed['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
world=HERE/'caller-repair-terra/DELIVERY.json';root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/overlap-r7-repair-root/DELIVERY.json')
assert sha(world)=='69b197558b3c874822142aa22d6ff613382d2ef258ca028d20313029f1e67ea4' and sha(root)=='024d83db98d6026da7bbe5080fa99397d3dd2132eab5f5464ead66238fc331c7'
oldplan=HERE/'intake-plan-approved-01.json';plan=read(oldplan);nodes=['11dd33d84195968adf9368ce619d1379528baf50','91086ca81b3f946fd69cd9728a59e9d3a7014010','00e62ec946c1bbbf369e37d7d6790bf93961c31a'];new_candidates=[]
for c in nodes:
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).stdout.splitlines():
  left,p=line.split('\t',1);_,_,pre,post,kind=left[1:].split();assert kind=='M'and p.endswith('.cpp')
  assert git('rev-parse','HEAD:'+p).stdout.strip()==pre
  changes.append({'path':p,'preimage':pre,'postimage':post,'first_touch':True,'preimage_matches':True})
 assert len(changes)==1;new_candidates.append({'commit':c,'parents':git('show','-s','--format=%P',c).stdout.split(),'message':git('show','-s','--format=%B',c).stdout,'paths':changes})
approved={**plan,'state':'ROOT_AUTHORIZED_R7_EXACT3CPP_CALLER_SUCCESSOR','proposed_originals':plan['proposed_originals']+nodes,'collect_now':plan['proposed_originals']+nodes,'candidate_count':23,'candidates':plan['candidates']+new_candidates,'unique_paths':sorted(set(plan['unique_paths'])|{c['paths'][0]['path']for c in new_candidates}),'owned_CPP_sources':sorted(set(plan['owned_CPP_sources'])|{c['paths'][0]['path']for c in new_candidates}),'changed_CPP':79,'parent_action_plan':str(oldplan),'parent_action_plan_sha256':sha(oldplan),'caller_correction_authority':'Root reviewed exact3Cpp preimages/receipts; fresh335 if strict resolved-header reuse not provable. No header/API adapters/inventory changes.','reuse_decision':'Recompile exact335 as first report has no complete resolved-header trace; preserve first335 FAIL and do not relabel prior objects.','failed_checkpoint_sha256':sha(HERE/'FIRST_SELECTED_FAIL_CHECKPOINT.json'),'created_utc':datetime.now(timezone.utc).isoformat()}
approved['planned_source_images']=dict(plan['planned_source_images'])
for c in new_candidates:
 q=c['paths'][0];approved['planned_source_images'][q['path']]={'R6_blob':git('rev-parse',plan['base']+':'+q['path']).stdout.strip(),'planned_postimage':q['postimage'],'caller_successor_preimage':q['preimage']}
pp=HERE/'intake-plan-approved-successor1.json'
with pp.open('x')as f:json.dump(approved,f,indent=2)
config=read(HERE/'campaign.json');config['current_plan']=str(pp);(HERE/'campaign.json').write_text(json.dumps(config,indent=2)+'\n')
statepath=HERE/'repair-r7-receipts.json';state=read(statepath);assert state['head']==BASE and state['state']=='complete'
state.update(state='integrating_caller_successor',pending_source_nodes=nodes);statepath.write_text(json.dumps(state,indent=2)+'\n')
for c in new_candidates:
 q=c['paths'][0];assert git('rev-parse','HEAD:'+q['path']).stdout.strip()==q['preimage']
 result=git('cherry-pick',c['commit'],check=False);print(result.stdout,result.stderr,flush=True)
 if result.returncode:
  state.update(state='conflict',pending=c['commit'],head=git('rev-parse','HEAD').stdout.strip());statepath.write_text(json.dumps(state,indent=2)+'\n');raise SystemExit(result.returncode)
 state['head']=git('rev-parse','HEAD').stdout.strip();state['receipts'].append({'source_commit':c['commit'],'integrated_commit':state['head'],'paths':[q['path']],'outcome':'Root-reviewed caller successor cherry-picked'});state['queue'].append(c['commit']);state['pending_source_nodes'].pop(0);statepath.write_text(json.dumps(state,indent=2)+'\n')
state['state']='complete';state['updated_utc']=datetime.now(timezone.utc).isoformat();statepath.write_text(json.dumps(state,indent=2)+'\n')
assert not git('status','--porcelain=v1').stdout.strip()
assert set(git('diff','--name-only',plan['base']).stdout.splitlines())==set(approved['unique_paths'])and len(approved['unique_paths'])==110
for p,v in approved['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).stdout.strip()==v['planned_postimage'],p
newhash={p:sha(WORK/p)for p in failed['source_snapshot']['hashes']};diff=[p for p,h in newhash.items()if h!=failed['source_snapshot']['hashes'][p]];assert set(diff)=={c['paths'][0]['path']for c in new_candidates}
binding={'head':state['head'],'base':BASE,'source_clean':True,'new_originals':nodes,'changed_CPP_only':diff,'no_header_changes':True,'all110_postimages_match':True,'first_failed_raw_sha256':sha(HERE/'selected-stage1/compile-results.json'),'first_failed_audit_sha256':sha(HERE/'selected-stage1-terminal-audit.json'),'approved_plan':str(pp),'approved_plan_sha256':sha(pp),'native_inventory':3615,'source_inputs':6299,'receipts':23,'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'caller-successor1-source-binding.json').open('x')as f:json.dump(binding,f,indent=2)
print(json.dumps(binding,indent=2))
