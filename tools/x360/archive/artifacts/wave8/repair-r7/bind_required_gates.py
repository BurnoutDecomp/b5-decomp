"""Bind selected/native/current-object R7 gates, retaining all negative history."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
def git(*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),*a]).decode().strip()
plan=read(HERE/'intake-plan-approved-successor1.json');meta=read(HERE/'native-parallel-wave8-r7-selected-successor1-patch.json');audit=read(HERE/'selected-successor1-terminal-audit.json')
assert audit['head']==meta['head']==git('rev-parse','HEAD')and not git('status','--porcelain=v1')
assert len(plan['proposed_originals'])==23 and len(plan['unique_paths'])==110 and len(plan['owned_CPP_sources'])==79
assert audit['summary']['R6_prior_passes_in_selection']==226 and not audit['summary']['regressions']
assert audit['summary']['passed']==272 and audit['summary']['failed']==63
named=read(HERE/'named-native-gates/gate-results.json');named_current=read(HERE/'named-native-gates/successor1-current-binding.json')
assert named_current['head']==meta['head']and named_current['gate_satisfied']and named_current['only3CPP_changes_no_headers']
assert all(r['gate_satisfied']for r in named['rows'])
for row in named['rows']:
 for p,h in row['artifacts'].items():assert sha(p)==h,p
completions=[HERE/'current-core-owner-witnesses/completion.json',HERE/'current-world-owner-witnesses/completion.json',HERE/'current-world-owner-retry1/completion.json',HERE/'current-world-auxiliary-witnesses/completion.json']
models={};initial_failures=[]
def bad(j):
 if isinstance(j,dict):
  for k,v in j.items():
   if k in ['mismatches','errors','exceptions','failures']and v:raise AssertionError((k,str(v)[:300]))
   if isinstance(v,(dict,list)):bad(v)
 elif isinstance(j,list):
  for v in j:
   if isinstance(v,(dict,list)):bad(v)
def sum_counts(j):
 if isinstance(j,int):return j
 if isinstance(j,dict):return sum(sum_counts(v)for v in j.values())
 raise AssertionError(('Unrecognized counts',j))
def count(j):
 if isinstance(j.get('cases'),int):return j['cases']
 if isinstance(j.get('cases'),dict):return sum_counts(j['cases'])
 if isinstance(j.get('compared'),int):return j['compared']
 if isinstance(j.get('cases'),list):return len(j['cases'])
 if isinstance(j.get('requested'),int):return j['requested']
 if isinstance(j.get('groups'),list):return sum(count(g)for g in j['groups'])
 if isinstance(j.get('counts'),dict):return sum_counts(j['counts'])
 raise AssertionError(('No unambiguous comparison count',list(j)))
for p in completions:
 comp=read(p);assert comp['head']==meta['head']
 for m in comp['results']:
  if m.get('returncode')!=0:
   initial_failures.append({'name':m['name'],'initial_completion':str(p),'returncode':m.get('returncode'),'adapter_error':m.get('adapter_error'),'log':m.get('log')});continue
  assert not m.get('report_issues')
  reports=[]
  for rp,h in m['reports'].items():
   assert sha(rp)==h,rp;j=read(rp);bad(j);reports.append({**pin(rp),'bounded_comparisons':count(j)})
  for op,h in m.get('objects',{}).items():assert sha(op)==h,op
  assert m['name']not in models
  models[m['name']]={'name':m['name'],'reports':reports,'binding':pin(Path(m['log']).parent/'binding.json'),'initial_completion':pin(p)}
assert len(models)==35 and {f['name']for f in initial_failures}=={'caller-box','caller-triangle','caller-triangle-alias','caller-primitive','torus-linked-quartic'}
assert all(f['name']in models for f in initial_failures)
positive=sum(r['bounded_comparisons']for m in models.values()for r in m['reports']);assert positive==6054,positive
extra=read(HERE/'current-constants-and-camera-flags.json');assert extra['head']==meta['head']and extra['passed_current_initializer_contracts']==6 and extra['camera_cases']==768
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
assert len(meta['source_snapshot']['hashes'])==6299
for p in ['src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h','src/GameSource/World/BrnWorldModule.h','src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_Render.cpp']:
 assert not git('diff',plan['base'],'HEAD','--',p),p
raw=read(HERE/'selected-successor1/compile-results.json');idx={r['source']:r for r in raw['results']}
assert all(idx[p]['passed']for p in plan['owned_CPP_sources'])
flow=[r for r in raw['results']if Path(r['source']).name=='BrnGameMainFlowStates.cpp'];assert len(flow)==1 and flow[0]['passed']
result={'head':meta['head'],'all_required_gates_satisfied':True,'original20_plus3_caller_repairs':23,'changed_paths':110,'changed_CPP':79,'changed_headers':31,'new_genuine_headers':plan['new_paths'],
 'selected_requested':335,'selected_passed':272,'selected_failed':63,'selected_R6_passes_retained':226,'changed_R6_passes_retained':30,'selected_R6_regressions':[],'selected_gains':46,'all79_owned_CPP_pass':True,
 'first_failed335_checkpoint':pin(HERE/'FIRST_SELECTED_FAIL_CHECKPOINT.json'),'first_failed335_raw_sha256':sha(HERE/'selected-stage1/compile-results.json'),
 'first_failed335_scope':'Initial223/226 prior passes retained,3 real new caller failures; immutable. All3 explicitly repaired under separate authorization; fresh335 retry rather than unsupported332-object reuse.',
 'selected_terminal_audit':pin(HERE/'selected-successor1-terminal-audit.json'),'caller_source_binding':pin(HERE/'caller-successor1-source-binding.json'),'caller_binding_metadata_correction':pin(HERE/'caller-successor1-binding-supplement.json'),
 'named_native_gates':pin(HERE/'named-native-gates/gate-results.json'),'named_current_binding':pin(HERE/'named-native-gates/successor1-current-binding.json'),
 'positive_probes':4,'expected_independent_C2118_negatives':2,'negative_scope':'HUDnatural7920 vs original allocation7940; WorldEntityactual79E0 vs independent79F0. No padding/guard suppression in product.',
 'current_owner_model_groups':35,'current_owner_bounded_comparisons':positive,'current_camera_flag_cases':768,'current_authentic_CRT_initializer_contracts':6,'total_current_bounded_comparisons':positive+768+6,
 'models':list(models.values()),'model_completions':[pin(p)for p in completions],'initial_adapter_failures_preserved':initial_failures,
 'model_loader_correction':'Four missing object filename aliases and one helper AST.ClassDef prefix-loader correction only. Initial failures retained; algorithms/arithmetic/RNG/expected bytes unchanged.',
 'extra_constants_and_flags':pin(HERE/'current-constants-and-camera-flags.json'),'owner36_strict_runtime_comparison':pin(HERE/'world-owner-current-runtime-binding.json'),
 'owner36_runtime_scope':'22 exact whole runtime objects may carry corresponding bounded historical proofs. Differing objects received actual current finite line/GP/prism/fine/torus/cylinder/box/constructor model checks as specified; cache arithmetic, Scene helper-vs-inline, Prop initializer and mesh extent/hits remain unproved. No broad normalization of old objects.',
 'RootGUI_raw_COFF_clarification':'Comparison excluding ONLY timestamp FAILED due additional metadata; separate874 runtime records/data/named-relocation equality is positive. Do not describe it as timestamp-only.',
 'execution_order_adjustment':pin(HERE/'execution-order-adjustment-01.json'),'recovery_interruption_checkpoint':pin(HERE/'full-stage1-interrupted-checkpoint.json'),
 'source_input_hashes_rechecked':6299,'guard_preservation':'All38RCE/40World/Render signature and selected MainFlow preserved; named typed/physical ABI probes positive; independent child/HUD negatives remain.',
 'retained_limits':plan.get('retained_limits',plan.get('owner_limits')),'formal_review':False,'publication':False,'goal':'ACTIVE; strict link/image/launch and whole-game behavior unproved','created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'selected-gates-completion.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps({'head':meta['head'],'model_groups':35,'bounded_cases':positive,'camera_cases':768,'CRT_contracts':6,'completion_sha256':sha(HERE/'selected-gates-completion.json')}))
