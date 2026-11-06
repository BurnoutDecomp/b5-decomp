"""Seal R7's completed diagnostic inventory and explicit recovery lineage."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
c=read(HERE/'campaign.json');plan=read(c['current_plan']);auditpath=HERE/'full-recovered-terminal-audit.json';audit=read(auditpath);report=read(audit['report'])
missing=read(HERE/'full-missing-stage1-exit.json');recovery=read(HERE/'full-recovery-terminal-supplement.json');receipts=read(HERE/'repair-r7-receipts.json')
gates=read(HERE/'selected-gates-completion.json');runtime=read(HERE/'full-selected-runtime-retention.json');timeproof=read(HERE/'runtime-time-exception-supplement.json')
head=audit['head'];assert head==receipts['head']==gates['head']==runtime['head']==missing['head']=='129afef95856e383366a55fb8f30a66ceb74de42'
assert report['state']=='complete'and len(report['results'])==report['requested']==3615 and report['passed']==3168 and report['failed']==447
assert sha(audit['report'])==audit['report_sha256']==runtime['full_raw_sha256']==recovery['diagnostic_report_sha256']
assert recovery['original_saved_rows']==1700 and recovery['missing_rows']==1915 and not recovery['original_process_exit_observed']
assert read(HERE/'full-stage1/compile-results.json')['state']=='running'and not(HERE/'full-stage1-exit.json').exists()
assert missing['exit_code']==1 and gates['all_required_gates_satisfied']and timeproof['all79_changed_owner_product_objects_exact']
assert timeproof['all_current_owner_model_product_inputs_exactly_retained_in_inventory']
assert len(receipts['receipts'])==len(plan['proposed_originals'])==23 and receipts['state']=='complete'
assert len(git(c['workspace'],'rev-list',c['base']+'..HEAD').splitlines())==23
assert len(audit['source_snapshot']['hashes'])==6299 and len(plan['unique_paths'])==110 and len(plan['owned_CPP_sources'])==79
assert not audit['summary']['regressions']and not audit['summary']['timeouts']and audit['summary']['prior_passes_retained']==3122
assert not audit['summary']['new_sources']and not audit['summary']['removed_sources']
state={k:{'head':git(c[k],'rev-parse','HEAD'),'status':git(c[k],'status','--porcelain=v1')}for k in ['canonical','workspace']}
assert state['canonical']=={'head':c['published_base'],'status':''}and state['workspace']=={'head':head,'status':''}
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
baseline=read(c['baseline_artifact_manifest']);published=read(c['published_artifact_manifest']);ext=audit['external_inputs'];r6=audit['summary'];pub=audit['published_comparison'];now=datetime.now(timezone.utc).isoformat()
crows=[r for r in report['results']if Path(r['source']).suffix.lower()=='.c'];assert len(crows)==42 and all(r['passed']for r in crows)
summary={'head':head,'base':audit['base'],'published_head':c['published_base'],'patch_sha256':audit['patch_sha256'],
 'requested':3615,'passed':3168,'failed':447,'source_requested':r6['source_requested'],'source_passed':r6['source_passed'],'native_C_passed':42,'native_C_requested':42,
 'versus_R6':r6,'versus_published_wave7':pub,'input_hashes':6299,
 'actual_artifact_hash_entries':sum(len(v)for v in audit['artifact_manifest'].values()),'R6_artifact_entries_verified':sum(len(v)for v in baseline['artifact_manifest'].values()),
 'published_artifact_entries_verified':sum(len(v)for v in published['artifact_manifest'].values()),'external_include_files':sum(len(v['files'])for v in ext['include_trees'].values()),
 'compiler_runtime_files':len(ext['compiler_and_runtime']),'clean_pinned_submodules':len(ext['submodules']),
 'initial_originals':20,'separately_authorized_caller_repairs':3,'actual_receipts':23,'actual_commits':23,'changed_paths':110,'changed_CPP':79,'changed_headers':31,'new_headers':plan['new_paths'],'new_product_CPP':[],'removed_product_CPP':[],
 'selected':{'requested':335,'passed':272,'failed':63,'prior_R6_passes_retained':226,'changed_R6_passes_retained':30,'gains':46,'regressions':0,'fresh_retry':True},
 'first_failed335_preserved':True,'first_failed335_passed':269,'first_failed335_failed':66,'first_failed335_prior_passes_retained':223,
 'recovery':{'original_full_unified_handle':86018,'original_python_pid':19080,'original_process_exit_observed':False,'original_saved_state':'running/interrupted','original_saved_rows':1700,
  'missing_only_unified_handle':55275,'missing_only_python_pid':37292,'missing_only_process_exit':1,'newly_compiled_missing_sources':1915,'missing_passed':1656,'missing_failed':259,
  'completed_diagnostic_kind':'Original saved valid1700 rows plus missing-only1915 recovery; not a restarted/second full sweep','binder_handle':21872,'binder_exit':0,
  'interrupted_checkpoint':str(HERE/'full-stage1-interrupted-checkpoint.json'),'interrupted_checkpoint_sha256':sha(HERE/'full-stage1-interrupted-checkpoint.json'),
  'supplement':str(HERE/'full-recovery-terminal-supplement.json'),'supplement_sha256':sha(HERE/'full-recovery-terminal-supplement.json')},
 'report':audit['report'],'report_sha256':audit['report_sha256'],'terminal_audit':str(auditpath),'terminal_audit_sha256':sha(auditpath),
 'required_gates':str(HERE/'selected-gates-completion.json'),'required_gates_sha256':sha(HERE/'selected-gates-completion.json'),
 'model_groups':35,'current_owner_bounded_comparisons':6054,'current_camera_flag_cases':768,'authenticated_current_initializer_contracts':6,'total_bounded_comparisons':6828,
 'runtime_binding':str(HERE/'full-selected-runtime-retention.json'),'runtime_binding_sha256':sha(HERE/'full-selected-runtime-retention.json'),
 'strict_runtime_exact_objects':271,'runtime_products_compared':272,'raw_all_exact_flag':False,'all79_changed_owner_products_exact':True,
 'runtime_time_exception_supplement':str(HERE/'runtime-time-exception-supplement.json'),'runtime_time_exception_supplement_sha256':sha(HERE/'runtime-time-exception-supplement.json'),
 'runtime_exception_scope':'Unowned unchanged RenderMetrics __TIME__22:03:55 vs22:12:16,9 literal bytes plus2 symbolic data references; instruction bytes and all other sections identical. Raw strict comparison remains false. Distinct from prior R6 AutoTest and RootGUI extra whole-COFF metadata.',
 'RootGUI_raw_COFF_clarification':'Ignoring ONLY timestamp did not yield equality due additional metadata. Separate874 runtime records/data/named relocations matched; current bounded Road264 and Mugshot324 are separately rebound.',
 'reviewed_plan_sha256':sha(HERE/'R7_PREFLIGHT_PLAN_01.json'),'action_plan':c['current_plan'],'action_plan_sha256':sha(c['current_plan']),
 'source_union_binding_sha256':sha(HERE/'intake-source-binding.json'),'caller_source_binding_sha256':sha(HERE/'caller-successor1-source-binding.json'),'caller_metadata_correction_sha256':sha(HERE/'caller-successor1-binding-supplement.json'),
 'canonical_and_intake':state,'parent_gitlink_index':plan['parent_gitlink_index'],'formal_review':False,'publication_allowed':False,
 'limits':plan.get('retained_limits',plan.get('owner_limits')),'goal':'ACTIVE; all native C/C++, real provider/strict link/image/launch not complete','created_utc':now}
jsonpath=HERE/'FROZEN_FULL_DIAGNOSTIC_R7.json';mdpath=HERE/'FROZEN_FULL_DIAGNOSTIC_R7.md';save(jsonpath,summary)
text=(
 '# Frozen R7 native diagnostic\n\n'
 f'Clean frozen intake `{head}`. Complete inventory: **3168 PASS / 447 FAIL / 3615**; source CPP/C3020/3460, all42 C inputs pass. All3122 R6 passes retained,46 gains,zero regressions/timeouts. Published Wave7:3079/3400 passes retained;321 historical pass regressions remain,89 gains.\n\n'
 'The original full process was interrupted with1700 saved rows and no observed exit. Those rows, commands, logs, assemblies and objects were validated against the unchanged6299 source inputs and external/tool context. Only the1915 missing recorded sources were compiled in a fresh D output,1656PASS/259FAIL, actual exit1. The completed report is an explicit1700+1915 diagnostic composite; the original running-state report and unrecorded artifacts remain immutable. No full restart or second full sweep occurred.\n\n'
 'Exactly20 reviewed originals plus3 separately authorized caller repairs:23 commits/receipts,110 paths,79 CPPs and31 headers. One genuine Mugshot header added; no CPP inventory changes. The initial335-source269/66 failure is retained. A fresh corrected335 gate completed272/63, retaining226/226 selected R6 passes and all30 changed prior passes; all79 owned CPPs pass.\n\n'
 f"Hash audit:6299 source inputs;{summary['actual_artifact_hash_entries']} current artifact entries;{summary['R6_artifact_entries_verified']} R6 and{summary['published_artifact_entries_verified']} published artifact hashes verified. External include files766,compiler/runtime files7,clean EA pins3. Canonical remains clean published3534; parent index/main/dev/ledger/origin unchanged.\n\n"
 'Four positive native/header probes pass; independent HUD7920-vs7940 and WorldEntity79E0-vs79F0 C2118 negatives remain. Current owner checks pass6054 comparisons across35 model groups, plus768 Camera flag cases and6 authenticated current CRT initializer contracts. Filename/helper-loader adapter failures are preserved separately; arithmetic/models/expected bytes were unchanged. These bounded software/controlled-service checks do not establish whole-runtime parity.\n\n'
 'Runtime binding:271/272 selected product objects are exact in all non-debug sections/data/named relocations; every79 changed owner product and every product input to the current owner checks is exact. The raw exact-all flag remains false: unchanged unowned RenderMetrics emits compiler time22:03:55 versus22:12:16,with identical instruction bytes and only the9-byte literal/two data references differing. This is distinct from prior R6 AutoTest. RootGUI raw whole-COFF comparison also remains failed for additional metadata,not merely timestamp; its separate874 runtime records/data/relocations are retained.\n\n'
 'All38 RCE/40 World/Render/Main protections remain. Original allocation/alignment uncertainty,mesh prefix/full extent/populated-hit limits,cache arithmetic,Scene helper-vs-inline,Prop initializer,old quartic negative and corrected-decoder attribution,finite/software-estimate/FP/hardware/service limits remain explicit. No padding/adapters/guard drops/forced link or later candidates entered this cutoff.\n\n'
 f"Report `{audit['report']}` SHA256 `{audit['report_sha256']}`. Audit `{auditpath}` SHA256 `{sha(auditpath)}`.\n\n"
 'No formal review or publication occurred. The whole-native goal remains active;447 native source failures and strict link/image/launch work remain.\n')
with mdpath.open('x',encoding='utf-8')as f:f.write(text)
completion={'head':head,'missing_compiler_session':55275,'missing_compiler_exit':1,'original_full_exit_observed':False,'binder_session':21872,'binder_exit':0,
 'report_sha256':audit['report_sha256'],'audit_sha256':sha(auditpath),'recovery_supplement_sha256':sha(HERE/'full-recovery-terminal-supplement.json'),
 'summary_sha256':sha(jsonpath),'summary_md_sha256':sha(mdpath),'runtime_binding_sha256':sha(HERE/'full-selected-runtime-retention.json'),'required_gates_sha256':sha(HERE/'selected-gates-completion.json'),
 'canonical_and_intake':state,'goal':'ACTIVE','no_publication':True};save(HERE/'full-recovered-completion.json',completion)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';owner=read(ownerpath)
owner.update(phase='wave8_repair_r7_completed_inventory_terminal_bound',frozen_head=head,intake_head=head,live_session=None,live_session_kind=None,live_process_id=None,
 original_full_interrupted=True,original_full_exit_observed=False,compiler_terminal_session=55275,compiler_terminal_exit=1,binder_terminal_session=21872,binder_terminal_exit=0,
 terminal_passed=3168,terminal_failed=447,terminal_requested=3615,terminal_report_sha256=audit['report_sha256'],terminal_audit=str(auditpath),terminal_audit_sha256=sha(auditpath),
 last_actual_full_head=head,last_actual_full_report=audit['report'],last_actual_full_report_sha256=audit['report_sha256'],last_actual_full_kind='Interrupted inventory completed from valid1700 plus missing-only1915; original exit unknown',
 current_diagnostic=str(jsonpath),current_diagnostic_sha256=sha(jsonpath),published_passes_retained=3079,actual_regression_count=321,actual_gain_count=89,R6_regressions=0,R6_gains=46,
 selected_gates_completion=str(HERE/'selected-gates-completion.json'),selected_gates_completion_sha256=sha(HERE/'selected-gates-completion.json'),current_review_packet=None,current_review_result=None,
 current_review_status='Not requested;321 published pass regressions remain. Completed diagnostic inventory only.',source_freeze_status='FROZEN clean R7 checkpoint; next read-only finite R8 preflight only',
 compiler_reservation_released=True,updated_utc=now,instruction='R7 missing-only recovery and artifact binder actual terminal. Original incomplete report/unknown exit and every raw negative preserved. No repeat global compile, source expansion, formal review or publication; Root reviews finite R8 before source action.')
ownerpath.write_text(json.dumps(owner,indent=2)+'\n',encoding='utf-8')
ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text('# R7 completed inventory terminal checkpoint\n\n'+text.split('\n\n',1)[1],encoding='utf-8')
print(json.dumps({'head':head,'passed':3168,'failed':447,'audit_sha256':sha(auditpath),'summary_sha256':sha(jsonpath),'summary_md_sha256':sha(mdpath),'completion_sha256':sha(HERE/'full-recovered-completion.json')},indent=2))
