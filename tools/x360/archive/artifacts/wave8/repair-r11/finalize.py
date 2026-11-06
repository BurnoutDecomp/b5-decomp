"""Seal the actual R11 full native inventory and scoped integrated48-byte proof."""
from datetime import datetime,timezone
import argparse,hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
p=argparse.ArgumentParser();p.add_argument('--binder-session',type=int,required=True);args=p.parse_args()
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
C=read(HERE/'campaign.json');plan=read(C['current_plan']);auditpath=HERE/'full-stage1-terminal-audit.json';a=read(auditpath);r=read(a['report']);terminal=read(a['exit_receipt']);g=read(HERE/'selected-gates-completion.json');receipts=read(HERE/'repair-r11-receipts.json');byteproof=read(HERE/'full-getparameters-proof.json')
assert r['state']=='complete'and len(r['results'])==r['requested']==3616 and r['passed']+r['failed']==3616
assert a['head']==terminal['head']==receipts['head']==g['head']==byteproof['head']=='e572f7db4c533546832e42c6f3f73149071b5f82'
assert terminal['report_sha256']==a['report_sha256']==sha(a['report'])and terminal['exit_code']==(1 if r['failed']else 0)
assert len(receipts['receipts'])==2 and receipts['state']=='complete'and len(plan['unique_paths'])==10 and len(plan['owned_CPP_sources'])==7
assert len(git(C['workspace'],'rev-list',C['base']+'..HEAD').splitlines())==2
assert len(a['source_snapshot']['hashes'])==6302 and len(plan['new_paths'])==3
assert set(a['summary']['new_sources'])=={p for p in plan['new_paths']if p.endswith('.cpp')}and not a['summary']['removed_sources']
assert g['all_required_gates_satisfied']and byteproof['exact48']and byteproof['code_relocations']==[]
state={k:{'head':git(C[k],'rev-parse','HEAD'),'status':git(C[k],'status','--porcelain=v1')}for k in ['workspace','canonical']}
assert state['workspace']=={'head':a['head'],'status':''}and state['canonical']=={'head':C['published_base'],'status':''}
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
baseline=read(C['baseline_artifact_manifest']);pm=read(C['published_artifact_manifest']);s=a['summary'];pub=a['published_comparison'];ext=a['external_inputs'];now=datetime.now(timezone.utc).isoformat()
idx={x['source']:x for x in r['results']};failed_owned=[p for p in plan['owned_CPP_sources']if not idx[p]['passed']];crows=[x for x in r['results']if Path(x['source']).suffix.lower()=='.c'];assert len(crows)==42 and all(x['passed']for x in crows)
summary={'head':a['head'],'base':C['base'],'published_head':C['published_base'],'coverage_kind':'ONE fresh full native sweep','requested':3616,'passed':r['passed'],'failed':r['failed'],
 'source_requested':s['source_requested'],'source_passed':s['source_passed'],'native_C_passed':42,'native_C_requested':42,'versus_R10':s,'versus_published_wave7':pub,
 'input_hashes':6302,'actual_artifact_hash_entries':sum(len(v)for v in a['artifact_manifest'].values()),'R10_artifact_entries_verified':sum(len(v)for v in baseline['artifact_manifest'].values()),
 'published_artifact_entries_verified':sum(len(v)for v in pm['artifact_manifest'].values()),'external_include_files':sum(len(v['files'])for v in ext['include_trees'].values()),
 'compiler_runtime_files':len(ext['compiler_and_runtime']),'clean_pinned_submodules':len(ext['submodules']),
 'originals':2,'changed_paths':10,'changed_CPP':7,'changed_headers':3,'new_headers':[p for p in plan['new_paths']if p.endswith('.h')],'new_native_CPP':[p for p in plan['new_paths']if p.endswith('.cpp')],
 'native_inventory_before':3615,'native_inventory_after':3616,'text_inputs_before':6299,'text_inputs_after':6302,'owned_full_failures':failed_owned,
 'selected_gate':{'requested':8,'passed':8,'failed':0,'prior_registration_PASS_retained':True,'showIncludes':True,'actual_unique_headers':read(HERE/'selected-stage1-actual-includes.json')['unique_actual_headers']},
 'five_positive_native_header_probes':True,'two_independent_C2118_negatives_retained':True,'required_gates_sha256':sha(HERE/'selected-gates-completion.json'),
 'integrated_selected_exact48_proof_sha256':sha(HERE/'integrated-getparameters-proof.json'),'integrated_full_exact48_proof_sha256':sha(HERE/'full-getparameters-proof.json'),
 'exact_method_scope':'Only complete renderengine::IndexBuffer::GetParameters,48 bytes/12 instructions,zero code relocations,no normalization,calibrated original reader. Companion physical-memory protection method and whole-TU/program are not claimed byte-identical.',
 'native_type_scope':'MaterialState genuine three-pointer12-byte dispatch view;serialized descriptorFC stays separate. Real Resource&/RenderableMesh/native IndexBuffer declarations,provider/import symbols and protected memory operations retained;no padding/facade.',
 'report':a['report'],'report_sha256':a['report_sha256'],'terminal_audit':str(auditpath),'terminal_audit_sha256':sha(auditpath),'plan_sha256':sha(C['current_plan']),
 'source_binding_sha256':sha(HERE/'intake-source-binding.json'),'selected_binder_initial_new_source_failure_preserved':str(HERE/'selected-binder-initial-failure.json'),
 'compiler_session':60091,'compiler_exit':terminal['exit_code'],'binder_session':args.binder_session,'binder_exit':0,
 'canonical_and_intake':state,'parent_gitlink_index':plan['parent_gitlink_index'],'limits':plan['known_limits'],
 'scope':'Full native compile/source/artifact coverage plus narrowly scoped ABI and48-byte helper identity. No overall match percentage,optional owner/link suite,full-game behavior,strict image/launch or publication claim.',
 'excluded':'All resource/FPU physics,Director/GUI/SoundCPP/Main42585/6a3a and later source remain outside exact2 cutoff.',
 'formal_review':False,'publication_allowed':False,'goal':'ACTIVE; native source/provider/link/image/launch work remains','created_utc':now}
path=HERE/'FROZEN_FULL_DIAGNOSTIC_R11.json';md=HERE/'FROZEN_FULL_DIAGNOSTIC_R11.md';save(path,summary)
text=(f"# Frozen R11 native resource diagnostic\n\nClean frozen intake `{a['head']}`. One actual full sweep: **{r['passed']} PASS / {r['failed']} FAIL / 3616**; compiler exit {terminal['exit_code']}.\n\n"
 f"R10: {s['prior_passes_retained']}/3258 prior passes retained, {len(s['new_passes'])} gains, {len(s['regressions'])} losses, {len(s['timeouts'])} timeouts. Published Wave7: {pub['prior_passes_retained']}/3400 passes retained; {len(pub['regressions'])} historical pass regressions remain, {len(pub['new_passes'])} gains.\n\n"
 'Exactly two ordered originals change ten paths: seven CPPs and three headers. Two genuine headers and one native provider CPP are new. Inventory 3615→3616; code inputs 6299→6302. Eight actual selected TUs passed with showIncludes, including the old passing registration consumer. Five native/header probes pass; both independent HUD7920/7940 and WorldEntity79E0/79F0 C2118 negatives remain. No guard or padding workaround was introduced.\n\n'
 'The actual selected and full IndexBuffer::GetParameters bodies match all 48 original bytes, 12 instructions, with zero code relocations and no normalization. Reader sentinels were calibrated. This is one complete method only; MemoryFlags, whole TU/program, runtime and overall matching percentage are not claimed. MaterialState is a genuine 12-byte three-pointer view; the original serialized descriptor literal FC is separate.\n\n'
 f"All 6302 source inputs and {summary['actual_artifact_hash_entries']} current artifact entries are bound; {summary['R10_artifact_entries_verified']} R10 and {summary['published_artifact_entries_verified']} published artifact hashes verified. External files 766; compiler/runtime files 7; EA pins 3. All 42 C sources pass.\n\n"
 f"Report `{a['report']}` SHA256 `{a['report_sha256']}`. Audit `{auditpath}` SHA256 `{sha(auditpath)}`.\n\n"
 'Canonical remains clean published3534; main/dev/origin/parent index/ledger unchanged. Later source was excluded; no optional link or owner suites were run. No formal review/publication or game image/launch proof. The whole-native goal remains active.\n')
with md.open('x',encoding='utf-8')as f:f.write(text)
completion={'head':a['head'],'compiler_session':60091,'compiler_exit':terminal['exit_code'],'binder_session':args.binder_session,'binder_exit':0,'report_sha256':a['report_sha256'],'audit_sha256':sha(auditpath),
 'summary_sha256':sha(path),'summary_md_sha256':sha(md),'exact_full_method_proof_sha256':sha(HERE/'full-getparameters-proof.json'),'canonical_and_intake':state,'goal':'ACTIVE','no_publication':True};save(HERE/'full-stage1-completion.json',completion)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath)
o.update(phase='wave8_repair_r11_full_terminal_bound',frozen_head=a['head'],intake_head=a['head'],live_session=None,live_process_id=None,compiler_terminal_session=60091,compiler_terminal_exit=terminal['exit_code'],
 binder_terminal_session=args.binder_session,binder_terminal_exit=0,terminal_passed=r['passed'],terminal_failed=r['failed'],terminal_requested=3616,terminal_report=a['report'],terminal_report_sha256=a['report_sha256'],
 terminal_audit=str(auditpath),terminal_audit_sha256=sha(auditpath),last_actual_full_head=a['head'],last_actual_full_report=a['report'],last_actual_full_report_sha256=a['report_sha256'],
 current_diagnostic=str(path),current_diagnostic_sha256=sha(path),current_coverage_kind='R11 one fresh full3616,new genuine provider;exact48 method proof separately scoped',
 published_passes_retained=pub['prior_passes_retained'],actual_regression_count=len(pub['regressions']),actual_gain_count=len(pub['new_passes']),R10_regressions=len(s['regressions']),R10_gains=len(s['new_passes']),
 source_freeze_status='FROZEN clean R11 checkpoint; later source requires finite Root authorization',compiler_reservation_released=True,current_review_status='Not requested;published pass regressions remain',updated_utc=now,
 instruction='R11 exact2 collection,selected8,header safeguards,one fresh full and exact48 provider proof terminal. Preserve raw failures,old negatives and honest byte scope. No source expansion,optional suites,publication/main/dev/index/ledger action.')
ownerpath.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8');ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text(text,encoding='utf-8')
print(json.dumps({'head':a['head'],'passed':r['passed'],'failed':r['failed'],'R10_losses':len(s['regressions']),'R10_gains':len(s['new_passes']),'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md)},indent=2))
