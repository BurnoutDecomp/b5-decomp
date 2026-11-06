"""Sole-writer collection of the exact Root-authorized4 source originals."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
C=json.loads((HERE/'campaign.json').read_text());WORK=Path(C['workspace']);PLAN=json.loads(Path(C['current_plan']).read_text());STATE=HERE/'repair-r12-receipts.json';state=json.loads(STATE.read_text())
def git(*a,check=True,cwd=WORK):
 r=subprocess.run(['git','--no-optional-locks',*a],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace')
 if check and r.returncode:raise RuntimeError((a,r.stdout,r.stderr))
 return r
def update():
 STATE.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8')
 op=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=json.loads(op.read_text());assert o['owner']=='/root/wave5_integrator_sol'
 o.update(phase='wave8_repair_r12_intake_'+state['state'],intake_head=state['head'],frozen_head=None,source_freeze_status='R8 exact19 integration; no compiler',live_session=None,live_process_id=None,
  campaign_root=str(HERE),plan=C['current_plan'],receipts=str(STATE),intake_receipts=len(state['receipts']),pending_source_commit=state.get('pending'),
  instruction='Sole-writer exact4/20 R8 source collection only. Preserve current R7 and published edits, all native guards and historical failures. No private ancestry/extra candidates/canonical/index/ledger publication.',updated_utc=datetime.now(timezone.utc).isoformat())
 op.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8')
assert len(PLAN['proposed_originals'])==4 and len(PLAN['unique_paths'])==20
assert git('rev-parse','HEAD').stdout.strip()==state['head']==C['base']and state['state']=='complete'and not state['receipts']
assert not git('branch','--show-current').stdout.strip()and not git('status','--porcelain=v1').stdout.strip()
assert git('rev-parse','HEAD',cwd=Path(C['canonical'])).stdout.strip()==C['published_base']and not git('status','--porcelain=v1',cwd=Path(C['canonical'])).stdout.strip()
assert git('ls-files','--stage','b5-decomp',cwd=PARENT).stdout.strip()==PLAN['parent_gitlink_index']
state.update(state='integrating',queue=list(PLAN['proposed_originals']),todo=list(PLAN['proposed_originals']));update()
for candidate in PLAN['candidates']:
 commit=candidate['commit'];assert state['todo'][0]==commit
 for change in candidate['paths']:
  actual=git('rev-parse','HEAD:'+change['path']).stdout.strip()
  if actual!=change['preimage']:
   state.update(state='unexpected_preimage',pending=commit);update();raise AssertionError((commit,change['path'],actual,change['preimage']))
 r=git('cherry-pick',commit,check=False)
 if r.returncode:
  state.update(state='conflict',pending=commit,head=git('rev-parse','HEAD').stdout.strip());update();print(r.stdout,r.stderr,flush=True);raise SystemExit(r.returncode)
 state['head']=git('rev-parse','HEAD').stdout.strip()
 for change in candidate['paths']:assert git('rev-parse','HEAD:'+change['path']).stdout.strip()==change['postimage']
 state['receipts'].append({'source_commit':commit,'integrated_commit':state['head'],'paths':[p['path']for p in candidate['paths']],'outcome':'exact normal cherry-pick; pre/postimages matched','receipt_sha256':candidate['receipt_sha256']})
 state['todo'].pop(0);update();print(len(state['receipts']),commit[:12],'->',state['head'][:12],flush=True)
assert not git('status','--porcelain=v1').stdout.strip()
assert set(git('diff','--name-only',C['base']).stdout.splitlines())==set(PLAN['unique_paths'])
for p,images in PLAN['planned_source_images'].items():assert git('rev-parse','HEAD:'+p).stdout.strip()==images['final']
state['state']='complete';update()
binding={'head':state['head'],'base':C['base'],'originals':PLAN['proposed_originals'],'receipts':len(state['receipts']),'paths':PLAN['unique_paths'],'all20_final_postimages_verified':True,
 'patch_sha256':hashlib.sha256(git('diff','--binary',C['base']).stdout.encode('utf-8')).hexdigest(),
 'scope':'Exact4 normal cherry-picks with pre/postimage verification; no stale final-file adoption or conflict resolution. Source frozen for approved selected/native gates; canonical/main/dev/parent-index/ledger untouched.'}
# Binary diff byte hash is computed independently without text newline conversion.
binding['patch_sha256']=hashlib.sha256(subprocess.check_output(['git','--no-optional-locks','diff','--binary',C['base']],cwd=WORK)).hexdigest()
with(HERE/'intake-source-binding.json').open('x',encoding='utf-8')as f:json.dump(binding,f,indent=2)
print('FROZEN_R12',state['head'],'receipts',len(state['receipts']),flush=True)
