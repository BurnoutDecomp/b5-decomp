"""Pin R5 selected union, current-object scopes, preserved negatives, and resource context."""
from datetime import datetime, timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
plan=read(HERE/'intake-plan-approved-01.json');meta=read(HERE/'native-parallel-wave8-r5-selected-stage1-patch.json')
head=meta['head'];assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==head
assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
assert subprocess.check_output(['git','--no-optional-locks','-C',str(PARENT),'ls-files','--stage','b5-decomp']).decode().strip()==plan['parent_gitlink_index']
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
for b in read(HERE/'manifest-bindings-01.json'):assert sha(b['path'])==b['expected_sha256'],b['path']
binding=read(HERE/'intake-source-binding.json')
for p in binding['paths']:
 assert sha(WORK/p['path'])==p['source_sha256']
 assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD:'+p['path']]).decode().strip()==p['git_blob']
audit=read(HERE/'selected-stage1-terminal-audit.json');report=read(HERE/'selected-stage1/compile-results.json')
assert audit['summary']['regressions']==[]
rows={r['source']:r for r in report['results']}
assert all(rows[p]['passed'] for p in plan['owned_R4_pass_sources']) and len(plan['owned_R4_pass_sources'])==25
base=read(HERE.parent/'repair-r4/full-stage1/compile-results.json');old={r['source']:r for r in base['results']}
gains=[s for s,r in rows.items() if r['passed'] and not old[s]['passed']]
assert len(gains)==7
witness=read(HERE/'current-object-witnesses/completion.json')
retry=read(HERE/'current-object-witnesses-output-v2/completion.json')
assert retry['returncode']==0
model_results=[]
for r in witness['results']:
 name=r['name'];expected=1 if name in ['audio-composed-negative','bloom-passes'] else 0
 assert r['returncode']==expected,(name,r.get('returncode'))
 for p,h in r.get('objects',{}).items():assert sha(p)==h
 for p,h in r.get('reports',{}).items():assert sha(p)==h
 if name=='bloom-passes':
  model_results.append({'name':name,'status':'positive416_followup_output_binding','initial_failed_adapter_preserved':True,'followup_completion':pin(HERE/'current-object-witnesses-output-v2/completion.json')})
 else:model_results.append({'name':name,'status':'expected_known_negative' if name in ['audio-composed-negative','replay-director-post-ice-negative'] else 'bounded_positive','reports':r['reports']})
filt=read(HERE/'current-object-witnesses/audio-composed-negative/butterworth-final-composed-comparison.json')
assert filt['cases']==48 and len(filt['errors'])==28
director=read(HERE/'current-object-witnesses/replay-director-post-ice-negative/r5-selected-director-negative-v1.json')
assert len(director['cases'])==128
assert all(c['compiled']['this']-c['original']['this']==0x110 for c in director['cases'])
probes=read(HERE/'named-header-probes/probe-results.json');assert all(r['gate_satisfied'] for r in probes['rows'])
for r in probes['rows']:
 for p,h in r['artifacts'].items():assert sha(p)==h
guards={}
for p,count in [('src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h',38),('src/GameSource/World/BrnWorldModule.h',40)]:
 assert sha(WORK/p)==meta['source_snapshot']['hashes'][p]
 original=subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse',plan['base']+':'+p]).decode().strip()
 current=subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD:'+p]).decode().strip()
 assert original==current
 guards[p]={'expected_production_guards_retained':count,'same_R4_blob':current,'sha256':sha(WORK/p)}
assert rows['src/GameSource/World/BrnMainX360.cpp']['passed'] if 'src/GameSource/World/BrnMainX360.cpp' in rows else any(Path(p).name=='BrnMainX360.cpp' and r['passed'] for p,r in rows.items())
now=datetime.now(timezone.utc).isoformat()
resource={'created_utc':now,'authority':'Root approved exact R5 finite collection/selected then one full8; notify Root before full to rebalance active private single-worker pool to4. While no full ran, Root allowed up to8 private one-worker jobs.','full_workers_when_started':8,'private_active_worker_cap_during_full':4,'exclusive_process_claim':False,'scope':'Resource observation/policy outside immutable source metadata; not a source/input or candidate amendment.'}
with (HERE/'compiler-resource-policy-01.json').open('x',encoding='utf-8') as f:json.dump(resource,f,indent=2)
result={'state':'SELECTED_UNION_STABLE_READY_FOR_ONE_FULL_DIAGNOSTIC','head':head,'base':plan['base'],'approved_originals':13,'explicit_owned_corrections':1,'actual_commit_records':14,'changed_paths':51,'changed_CPP':38,'new_files':0,'native_inventory':3616,'source_inputs':6299,'selected':{'requested':70,'passed':60,'failed':10,'protected_R4_passes_retained':25,'regressions':0,'sampled_gains':gains,'audit':pin(HERE/'selected-stage1-terminal-audit.json')},'fresh_positive_owner_model_cases':4016,'current_object_models':model_results,'initial_model_runner_completion':pin(HERE/'current-object-witnesses/completion.json'),'bloom_output_binding_followup':pin(HERE/'current-object-witnesses-output-v2/completion.json'),'actual_typed_probes':pin(HERE/'named-header-probes/probe-results.json'),'fresh_positive_typed_header_probes':3,'production_guard_preservation':guards,'preserved_negatives':{'WorldEntity':'Actual sizeof79E0; expected native79F0 still C2118. ParentWorld5E3C80 does not prove the child.','current_Director':'128 actual post-ICE physical negative calls: original+38930 vs product+38A40, difference+110. No this/register normalization. The reused model report static prose about+100 is historical; numeric rows and this binding are current.','historical_Director':'Original pre-ICE+100 negative reports remain immutable; never relabeled current.','ICE':'Current header scene12110/native12170; status7200/native7204; camera36330/native36380; serial38A40/native38930. No filler/ABI closure claim.','Butterworth':'Actual designer controlled chain48 passes; actual real Filter composition preserves28/48 mismatches.','module_outside_owned_replay':'All128 current complete module tests retain differences outside the owned replay interval; no whole Construct parity claim.'},'historical_receipts_rehashed':17,'input_snapshot_rechecked':6299,'source_union_binding':pin(HERE/'intake-source-binding.json'),'resource_context':pin(HERE/'compiler-resource-policy-01.json'),'goal':'ACTIVE; diagnostic only, no formal review/publication/strict link/image/launch approval','created_utc':now,'scope_limits':plan['retained_limits']}
with (HERE/'selected-gates-completion.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
owner_path=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';owner=read(owner_path)
owner.update(phase='wave8_repair_r5_selected_named_gates_bound_ready_full',intake_head=head,frozen_head=head,live_session=None,live_process_id=None,selected_gates_completion=str(HERE/'selected-gates-completion.json'),selected_gates_completion_sha256=sha(HERE/'selected-gates-completion.json'),compiler_resource_policy=str(HERE/'compiler-resource-policy-01.json'),compiler_exclusive=False,updated_utc=now,source_freeze_status='Stable exact R5 source; selected and named gates bound; ONE full not started')
owner_path.write_text(json.dumps(owner,indent=2)+'\n')
print(json.dumps({'head':head,'selected_completion_sha256':sha(HERE/'selected-gates-completion.json'),'fresh_positive_cases':4016,'expected_Filter_mismatch_cases':28,'current_post_ICE_director_negative_cases':128,'ready_for_one_full':True},indent=2))
