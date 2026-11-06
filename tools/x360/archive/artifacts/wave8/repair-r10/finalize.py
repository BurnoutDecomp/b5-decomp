"""Seal R10 source-only composite with inherited build origins unchanged."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;R8=HERE.parent/'repair-r8';PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
C=read(HERE/'campaign.json');plan=read(C['current_plan']);auditpath=HERE/'composite-artifact-audit.json';a=read(auditpath);r=read(a['report']);receipts=read(HERE/'repair-r10-receipts.json');terminal=read(HERE/'targeted-stage1-exit.json')
assert a['head']==receipts['head']==terminal['head']=='1598404ea16b7e16cbe00396bbd051e38b0d59f4'
assert r['state']=='complete'and r['requested']==len(r['results'])==3615 and r['passed']+r['failed']==3615 and sha(a['report'])==a['report_sha256']
assert terminal['exit_code']==0 and a['fresh_actual_compiles']==15 and a['retained_actual_R9_rows']==3600
assert len(receipts['receipts'])==4 and receipts['state']=='complete'and len(plan['unique_paths'])==15 and not plan['changed_headers']
assert a['summary']['prior_passes_retained']==3243 and not a['summary']['regressions']and not a['summary']['timeouts']
assert len(a['source_snapshot']['hashes'])==6299 and len(git(C['workspace'],'rev-list',C['base']+'..HEAD').splitlines())==4
assert r['provenance']['underlying_R8_R9_origins_preserved']
state={k:{'head':git(C[k],'rev-parse','HEAD'),'status':git(C[k],'status','--porcelain=v1')}for k in ['workspace','canonical']}
assert state['workspace']=={'head':a['head'],'status':''}and state['canonical']=={'head':C['published_base'],'status':''}
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
named=read(R8/'named-native-gates/gate-results.json');headers={};artifacts={}
for row in named['rows']:
 assert row['gate_satisfied']
 for p,h in row['artifacts'].items():assert sha(p)==h,p;artifacts[p]=h
 for p,h in row['actual_included_headers'].items():assert sha(p)==h,p;headers[p]=h
save(HERE/'native-safeguard-current-reuse.json',{'head':a['head'],'original_probe_head':named['head'],'original_probe_report_sha256':sha(R8/'named-native-gates/gate-results.json'),
 'same_actual_probes_no_recompile':True,'all_headers_unchanged':True,'actual_included_header_hashes_rechecked':len(headers),'probe_artifact_hashes_rechecked':len(artifacts),
 'four_positive_two_expected_negative_gates_retained':True,'scope':'Source-only CPP delta,zero incoming include edges; original native/header probe and headers unchanged. RCE/World/Render/Main,Hud7920/7940 and WorldEntity79E0/79F0 negatives preserved without new tests.'})
before=read(C['baseline_artifact_manifest']);pm=read(C['published_artifact_manifest']);s=a['summary'];pub=a['published_comparison'];ext=a['external_inputs'];now=datetime.now(timezone.utc).isoformat()
summary={'head':a['head'],'base':C['base'],'published_head':C['published_base'],'coverage_kind':'Complete-coverage COMPOSITE, NOT a fresh full native sweep','requested':3615,'passed':r['passed'],'failed':r['failed'],
 'source_requested':s['source_requested'],'source_passed':s['source_passed'],'fresh_actual_compiles':15,'retained_R9_coverage_rows':3600,'underlying_R8_R9_build_origins_preserved':True,
 'originals':4,'changed_CPP':15,'changed_file_entries':16,'changed_headers':0,'new_files':[],'inventory_change':False,'versus_R9':s,'versus_published_wave7':pub,
 'input_hashes':6299,'actual_artifact_hash_entries':sum(len(v)for v in a['artifact_manifest'].values()),'R9_artifact_entries_verified':sum(len(v)for v in before['artifact_manifest'].values()),
 'published_artifact_entries_verified':sum(len(v)for v in pm['artifact_manifest'].values()),'external_include_files':sum(len(v['files'])for v in ext['include_trees'].values()),
 'compiler_runtime_files':len(ext['compiler_and_runtime']),'clean_pinned_submodules':len(ext['submodules']),
 'targeted_gate':{'requested':15,'passed':15,'failed':0,'showIncludes':True,'actual_unique_headers':read(HERE/'targeted-stage1-actual-includes.json')['unique_actual_headers'],'compiler_session':99992,'compiler_exit':0,'binder_session':25478,'binder_exit':0},
 'ordered_semantic_correction':'038e CameraUtils is inseparable from9c3d; only final current CameraShake object compiled. Explicit native float3 constructor retains W+0; superseded object and original semantic finding remain historical.',
 'reuse_proof':str(HERE/'reuse-proof-terminal.json'),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),'native_safeguard_reuse_sha256':sha(HERE/'native-safeguard-current-reuse.json'),
 'report':a['report'],'report_sha256':a['report_sha256'],'artifact_audit':str(auditpath),'artifact_audit_sha256':sha(auditpath),'plan_sha256':sha(C['current_plan']),
 'source_binding_sha256':sha(HERE/'intake-source-binding.json'),'owner_preflight_sha256':plan['reviewed_preflight_sha256'],'canonical_and_intake':state,'parent_gitlink_index':plan['parent_gitlink_index'],
 'scope':'Actual15 integrated native compiles plus verified unaffected3600 prior coverage rows. Header/tool/external/flags identity and no changed-CPP inclusion prove reuse. No optional link/owner suites or whole behavior/byte identity/link/image/launch claim.',
 'limits':plan['known_limits'],'excluded':'NativeRender header/provider chain,Main42585,resource inventory addition and all later source remain outside R10. No diagnostic link-input pruning.',
 'formal_review':False,'publication_allowed':False,'goal':'ACTIVE; native source/provider/link/image/launch work remains','created_utc':now}
path=HERE/'FROZEN_COMPOSITE_DIAGNOSTIC_R10.json';md=HERE/'FROZEN_COMPOSITE_DIAGNOSTIC_R10.md';save(path,summary)
text=(f"# Frozen R10 composite native diagnostic\n\nClean frozen intake `{a['head']}`. Complete coverage: **{r['passed']} PASS / {r['failed']} FAIL / 3615**. Explicitly **15 fresh compiles + 3600 retained R9 coverage rows**, not a fresh full sweep. Underlying actual R8/R9 build origins remain unchanged.\n\n"
 f"All3243 R9 passes retained,{len(s['new_passes'])} gains,zero losses/timeouts. Published Wave7:{pub['prior_passes_retained']}/3400 passes retained;{len(pub['regressions'])} historical pass regressions remain,{len(pub['new_passes'])} gains.\n\n"
 'Four exact originals change15 existing CPPs;no headers,new files or inventory changes. All15 final integrated TUs compile with showIncludes against frozen merged headers. CameraUtils038e was followed inseparably by9c3d;the final native float3 constructor preserves W+0 and the superseded object was not reused. Existing author compile/codegen/semantic limits remain;no optional runtime or link suite was added.\n\n'
 'Only15 CPP hashes differ from R9. All headers/other code,compiler flags/tools,external files and EA pins match. No include edge reaches a changed CPP;unrelated Lua C amalgamation includes remain recorded. Every reused object/listing/log was verified and original per-row build/report attribution is preserved.\n\n'
 f"All6299 source inputs and{summary['actual_artifact_hash_entries']} composite artifact entries are bound;{summary['R9_artifact_entries_verified']} R9 and{summary['published_artifact_entries_verified']} published artifact hashes verified. External files766;compiler/runtime files7;EA pins3. Original native safeguards and independent layout/FP/runtime negatives remain.\n\n"
 f"Composite `{a['report']}` SHA256 `{a['report_sha256']}`. Audit `{auditpath}` SHA256 `{sha(auditpath)}`.\n\n"
 'Canonical remains clean published3534;main/dev/origin/parent index/ledger unchanged. NativeRender/Main/resource and later sources excluded. No formal review/publication or game link/image/launch proof;whole-native goal remains active. R9 optional wrong-utility link timeout stays separately recorded and was not replayed.\n')
with md.open('x',encoding='utf-8')as f:f.write(text)
completion={'head':a['head'],'kind':summary['coverage_kind'],'compiler_session':99992,'compiler_exit':0,'binder_session':25478,'binder_exit':0,'report_sha256':a['report_sha256'],
 'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),'canonical_and_intake':state,'goal':'ACTIVE','no_publication':True};save(HERE/'composite-completion.json',completion)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath)
o.update(phase='wave8_repair_r10_composite_terminal_bound',frozen_head=a['head'],intake_head=a['head'],live_session=None,live_process_id=None,compiler_terminal_session=99992,compiler_terminal_exit=0,
 binder_terminal_session=25478,binder_terminal_exit=0,terminal_passed=r['passed'],terminal_failed=r['failed'],terminal_requested=3615,terminal_report=a['report'],terminal_report_sha256=a['report_sha256'],
 terminal_audit=str(auditpath),terminal_audit_sha256=sha(auditpath),current_diagnostic=str(path),current_diagnostic_sha256=sha(path),current_coverage_kind='R10 composite15fresh+3600verifiedR9;underlying build origins preserved,no full replay',
 published_passes_retained=pub['prior_passes_retained'],actual_regression_count=len(pub['regressions']),actual_gain_count=len(pub['new_passes']),R9_regressions=0,R9_gains=len(s['new_passes']),
 source_freeze_status='FROZEN clean R10 composite checkpoint; no later sources authorized',compiler_reservation_released=True,current_review_status='Not requested;published regressions remain',updated_utc=now,
 instruction='R10 targeted15 and composite artifact binding actual terminal. No unnecessary full replay,optional link/owner suite,source expansion,publication or main/dev/index/ledger changes. Preserve underlying R8/R9 per-row origins and old negatives.')
ownerpath.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8');ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text(text,encoding='utf-8')
print(json.dumps({'head':a['head'],'coverage_passed':r['passed'],'coverage_failed':r['failed'],'fresh':15,'retained':3600,'R9_losses':0,'R9_gains':len(s['new_passes']),'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md)},indent=2))
