"""Seal R8 compile-focused terminal evidence; no publication/runtime extrapolation."""
from datetime import datetime,timezone
import argparse,hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
p=argparse.ArgumentParser();p.add_argument('--compiler-session',type=int,required=True);p.add_argument('--binder-session',type=int,required=True);args=p.parse_args()
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
c=read(HERE/'campaign.json');plan=read(c['current_plan']);auditpath=HERE/'full-stage1-terminal-audit.json';a=read(auditpath);r=read(a['report']);terminal=read(a['exit_receipt']);g=read(HERE/'selected-gates-completion.json');receipts=read(HERE/'repair-r8-receipts.json')
assert r['state']=='complete'and r['requested']==len(r['results'])==3615 and r['passed']+r['failed']==3615
assert sha(a['report'])==a['report_sha256']==terminal['report_sha256']and terminal['head']==a['head']==g['head']==receipts['head']
assert terminal['exit_code']==(1 if r['failed']else 0)and g['all_required_gates_satisfied']
assert len(receipts['receipts'])==19 and len(plan['unique_paths'])==66 and len(plan['owned_CPP_sources'])==64
assert len(git(c['workspace'],'rev-list',c['base']+'..HEAD').splitlines())==19
assert len(a['source_snapshot']['hashes'])==6299 and not a['summary']['new_sources']and not a['summary']['removed_sources']
state={k:{'head':git(c[k],'rev-parse','HEAD'),'status':git(c[k],'status','--porcelain=v1')}for k in ['workspace','canonical']}
assert state['workspace']=={'head':a['head'],'status':''}and state['canonical']=={'head':c['published_base'],'status':''}
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
before=read(c['baseline_artifact_manifest']);pubmanifest=read(c['published_artifact_manifest']);s=a['summary'];pub=a['published_comparison'];ext=a['external_inputs'];now=datetime.now(timezone.utc).isoformat()
owned={v['source']:v for v in r['results']};ownedfail=[p for p in plan['owned_CPP_sources']if not owned[p]['passed']]
summary={'head':a['head'],'base':a['base'],'published_head':c['published_base'],'patch_sha256':a['patch_sha256'],'requested':3615,'passed':r['passed'],'failed':r['failed'],
 'source_requested':s['source_requested'],'source_passed':s['source_passed'],'versus_R7':s,'versus_published_wave7':pub,'input_hashes':6299,
 'actual_artifact_hash_entries':sum(len(v)for v in a['artifact_manifest'].values()),'R7_artifact_entries_verified':sum(len(v)for v in before['artifact_manifest'].values()),'published_artifact_entries_verified':sum(len(v)for v in pubmanifest['artifact_manifest'].values()),
 'external_include_files':sum(len(v['files'])for v in ext['include_trees'].values()),'compiler_runtime_files':len(ext['compiler_and_runtime']),'clean_pinned_submodules':len(ext['submodules']),
 'originals':19,'actual_receipts':19,'changed_paths':66,'changed_CPP':64,'changed_headers':2,'new_files':[],'removed_files':[],'native_inventory_changed':False,
 'selected_requested':264,'selected_passed':172,'selected_failed':92,'selected_R7_passes_retained':109,'selected_gains':63,'selected_regressions':[],
 'owned_full_failures':ownedfail,'required_gates_sha256':sha(HERE/'selected-gates-completion.json'),'four_positive_native_header_probes':True,'two_independent_layout_negatives_retained':True,
 'report':a['report'],'report_sha256':a['report_sha256'],'terminal_audit':str(auditpath),'terminal_audit_sha256':sha(auditpath),'plan_sha256':sha(c['current_plan']),
 'owner_final_preflight_sha256':plan['owner_preflight_sha256'],'source_binding_sha256':sha(HERE/'intake-source-binding.json'),
 'scope':'Whole native source compile diagnostic and input/artifact audit. Existing owner ABI/CRT/case evidence and all original limits/negatives remain; no new optional runtime suites, global byte identity, strict link/image/launch or whole-GUI/game behavior claim.',
 'limits':plan['known_limits'],'excluded':plan['excluded'],'canonical_and_intake':state,'parent_gitlink_index':plan['parent_gitlink_index'],
 'compiler_session':args.compiler_session,'compiler_exit':terminal['exit_code'],'binder_session':args.binder_session,'binder_exit':0,
 'formal_review':False,'publication_allowed':False,'goal':'ACTIVE; native source/provider/link/image/launch work remains','created_utc':now}
path=HERE/'FROZEN_FULL_DIAGNOSTIC_R8.json';md=HERE/'FROZEN_FULL_DIAGNOSTIC_R8.md';save(path,summary)
text=(f"# Frozen R8 native compile diagnostic\n\nClean frozen intake `{a['head']}`. Actual full **{r['passed']} PASS / {r['failed']} FAIL / 3615**; process exit {terminal['exit_code']}.\n\n"
 f"R7: {s['prior_passes_retained']}/3168 prior passes retained, {len(s['new_passes'])} gains, {len(s['regressions'])} losses, {len(s['timeouts'])} timeouts. Published Wave7: {pub['prior_passes_retained']}/3400 passes retained; {len(pub['regressions'])} historical pass regressions remain, {len(pub['new_passes'])} gains.\n\n"
 'Exactly 19 originals, 66 existing paths: 64 CPPs and two headers. No inventory change, new files or link-input exclusions. The actual selected gate was 172/92, retaining all 109 selected R7 passes; all 64 owned CPPs passed. Four fresh native/header probes pass; HUD 7920/7940 and WorldEntity 79E0/79F0 independent C2118 negatives remain. Main/MainFlow and all existing RCE/World/Render safeguards are preserved.\n\n'
 'The debug initializer candidate was collected inseparably with its actual-CRT-backed Origin/RightUp correction. GuiView follows its attested native default-global successor and removes the host witness. FontRenderer and BoundaryLine native declarations/direct caller closure were included in the conservative 264-source consumer gate. FlaptRenderer retains its prior 78 diagnostic lines; compile success is not whole UI/runtime parity.\n\n'
 f"All 6299 native code inputs and {summary['actual_artifact_hash_entries']} current artifact entries are bound; {summary['R7_artifact_entries_verified']} R7 and {summary['published_artifact_entries_verified']} published historical artifact hashes were verified. External files {summary['external_include_files']}; compiler/runtime files 7; clean EA pins 3.\n\n"
 'Existing owner native cases and negative controls remain scoped; no optional new runtime suites or global byte-identity claim were added. R7 interruption/recovery history, layout/alignment/mesh/cache/Prop/Scene/FP/hardware limits and the failed strict-link diagnostic remain unchanged. Later geometry, scene, GameModule and link-provider/input work were excluded.\n\n'
 f"Report `{a['report']}` SHA256 `{a['report_sha256']}`. Audit `{auditpath}` SHA256 `{sha(auditpath)}`.\n\nCanonical remains clean published 3534. Main/dev/origin/parent index/ledger are unchanged. No formal review or publication; the whole-native goal remains active.\n")
with md.open('x',encoding='utf-8')as f:f.write(text)
completion={'head':a['head'],'compiler_session':args.compiler_session,'compiler_exit':terminal['exit_code'],'binder_session':args.binder_session,'binder_exit':0,
 'raw_sha256':a['report_sha256'],'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md),'canonical_and_intake':state,'goal':'ACTIVE','no_publication':True};save(HERE/'full-stage1-completion.json',completion)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath)
o.update(phase='wave8_repair_r8_full_terminal_bound',frozen_head=a['head'],intake_head=a['head'],live_session=None,live_process_id=None,compiler_terminal_session=args.compiler_session,
 compiler_terminal_exit=terminal['exit_code'],binder_terminal_session=args.binder_session,binder_terminal_exit=0,terminal_passed=r['passed'],terminal_failed=r['failed'],terminal_requested=3615,
 terminal_report_sha256=a['report_sha256'],terminal_audit=str(auditpath),terminal_audit_sha256=sha(auditpath),last_actual_full_head=a['head'],last_actual_full_report=a['report'],last_actual_full_report_sha256=a['report_sha256'],
 current_diagnostic=str(path),current_diagnostic_sha256=sha(path),published_passes_retained=pub['prior_passes_retained'],actual_regression_count=len(pub['regressions']),actual_gain_count=len(pub['new_passes']),
 R7_regressions=len(s['regressions']),R7_gains=len(s['new_passes']),current_review_status='Not requested; diagnostic only, published pass regressions remain',source_freeze_status='FROZEN clean R8 checkpoint; next finite Root instruction required',
 compiler_reservation_released=True,updated_utc=now,instruction='R8 compiler and artifact binder actual terminal. Preserve all raw failures and prior R7 recovery/negative history. No silent source expansion, repeated full, formal review/publication or main/dev/index/ledger changes.')
ownerpath.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8');ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text(text,encoding='utf-8')
print(json.dumps({'head':a['head'],'passed':r['passed'],'failed':r['failed'],'R7_losses':len(s['regressions']),'R7_gains':len(s['new_passes']),'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md)},indent=2))
