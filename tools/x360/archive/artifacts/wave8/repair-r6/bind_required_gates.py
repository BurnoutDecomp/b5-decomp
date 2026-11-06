"""Pin completed R6 selected/header/current-object gates while authorized full runs."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
meta=read(HERE/'native-parallel-wave8-r6-selected-stage1-patch.json');plan=read(HERE/'intake-plan-approved-01.json');audit=read(HERE/'selected-stage1-terminal-audit.json');named=read(HERE/'named-native-gates/gate-results.json')
assert audit['head']==named['head']==meta['head']
assert not audit['summary']['regressions'] and audit['summary']['R5_prior_passes_in_selection']==204
assert all(r['gate_satisfied']for r in named['rows'])
for r in named['rows']:
 for p,h in r['artifacts'].items():assert sha(p)==h,p
positive=0;models=[]
def count(d):
 if isinstance(d.get('cases'),int):return d['cases']
 if isinstance(d.get('cases'),dict):return sum(d['cases'].values())
 if isinstance(d.get('total_cases'),int):return d['total_cases']
 return sum(g['cases']for g in d.get('groups',[]))
for path in [HERE/'current-object-witnesses/completion.json',HERE/'current-gpu-witnesses/completion.json']:
 comp=read(path);assert comp['head']==meta['head']
 for m in comp['results']:
  negative=m['name']=='triangle-contract-negative';assert m['returncode']==(1 if negative else 0),(m['name'],m.get('returncode'))
  reports=[]
  for p,h in m['reports'].items():
   assert sha(p)==h,p;d=read(p);n=count(d)
   if not negative:positive+=n
   reports.append({'path':p,'sha256':h,'cases':n})
  for p,h in m.get('objects',m.get('actual_objects',{})).items():assert sha(p)==h,p
  models.append({'name':m['name'],'status':'expected_full_scratch_negative'if negative else'bounded_positive','reports':reports,'model_binding':str(Path(m['log']).parent/'binding.json'),'model_binding_sha256':sha(Path(m['log']).parent/'binding.json')})
assert positive==7674,positive
triangle=read(HERE/'triangle-contract-scope-supplement.json');assert triangle['expected_negative_preserved'] and triangle['input_vertex_contract_passed']==6 and triangle['whole_scratch_match']==0
assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==meta['head']
assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
for p,v in plan['expected_postimage_binding'].items():
 if v['planned_postimage']is None:assert not(WORK/p).exists()
 else:assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD:'+p]).decode().strip()==v['planned_postimage']
for p in ['src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h','src/GameSource/World/BrnWorldModule.h','src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_Render.cpp']:
 assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'diff',plan['base'],'HEAD','--',p])==b''
current=read(HERE/'full-stage1/compile-results.json');old={r['source']:r for r in read(HERE.parent/'repair-r5/full-stage1/compile-results.json')['results']};loss=[r['source']for r in current['results']if old[r['source']]['passed']and not r['passed']]
result={'state':'ALL_REQUIRED_GATES_BOUND_DURING_AUTHORIZED_ONE_FULL','head':meta['head'],'selected_requested':277,'selected_passed':221,'selected_failed':56,'selected_R5_passes_retained':204,'selected_R5_regressions':[],'selected_gains':17,'owned_R5_passes_retained':12,'owned_current_failed_CPP':['src/GameSource/Game/BrnGameModule.cpp','src/GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.cpp','src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo_wN3_03.cpp','src/GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.cpp'],'all_required_gates_satisfied':True,'selected_terminal_audit':pin(HERE/'selected-stage1-terminal-audit.json'),'named_native_gates':pin(HERE/'named-native-gates/gate-results.json'),'additional_real_native_providers':2,'positive_typed_layout_or_diagnostic_probes':4,'independent_child_C2118_negative_retained':True,'bounded_current_owner_positive_comparisons':positive,'raw_triangle_vertex_contract_cases':6,'triangle_full_B0_matched':0,'triangle_full_B0_cases':6,'triangle_scope':pin(HERE/'triangle-contract-scope-supplement.json'),'models':models,'model_completions':[pin(HERE/'current-object-witnesses/completion.json'),pin(HERE/'current-gpu-witnesses/completion.json')],'execution_order_adjustment':pin(HERE/'execution-order-adjustment-01.json'),'actual_full_handle':45749,'actual_full_PID':11156,'partial_full_at_binding':{'state':current['state'],'rows':len(current['results']),'passed':current['passed'],'failed':current['failed'],'R5_pass_losses_in_completed_rows':loss},'source_input_hashes_rechecked':6298,'guard_preservation':'RCE38/World40/Render signature unchanged; full physical/ABI/hardware limits retained','goal':'ACTIVE; no publication/formal review/strict link/image/launch approval','limits':plan['owner_limits'],'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'selected-gates-completion.json').open('x')as f:json.dump(result,f,indent=2)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath);o.update(owner_model_checks='All27generic models +2GPU complete;7674positive comparisons,Triangle6raw contract/0whole scratch expected negative',selected_gates_completion=str(HERE/'selected-gates-completion.json'),selected_gates_completion_sha256=sha(HERE/'selected-gates-completion.json'),updated_utc=datetime.now(timezone.utc).isoformat());ownerpath.write_text(json.dumps(o,indent=2)+'\n')
print(json.dumps({'head':meta['head'],'positive_cases':positive,'completion_sha256':sha(HERE/'selected-gates-completion.json'),'full_partial':result['partial_full_at_binding']},indent=2))
