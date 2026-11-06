"""Seal explicitly composite R9 coverage, preserving the optional link timeout."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;R8=HERE.parent/'repair-r8';PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
C=read(HERE/'campaign.json');plan=read(C['current_plan']);auditpath=HERE/'composite-artifact-audit.json';a=read(auditpath);r=read(a['report']);receipts=read(HERE/'repair-r9-receipts.json');terminal=read(HERE/'targeted-stage1-exit.json');proof=read(HERE/'reuse-proof-terminal.json');kd=read(HERE/'kd-symbols-and-link-timeout-supplement.json')
assert a['head']==receipts['head']==terminal['head']==kd['head']=='373dc4b41168751c3fc0d3e14969527ced237453'
assert r['state']=='complete'and r['requested']==len(r['results'])==3615 and r['passed']==3243 and r['failed']==372
assert sha(a['report'])==a['report_sha256']and terminal['exit_code']==0
assert a['fresh_actual_compiles']==14 and a['retained_actual_R8_rows']==3601 and len(receipts['receipts'])==4 and receipts['state']=='complete'
assert a['summary']['prior_passes_retained']==3231 and not a['summary']['regressions']and not a['summary']['timeouts']
assert len(plan['unique_paths'])==13 and len(a['source_snapshot']['hashes'])==6299 and not plan['changed_headers']
assert len(git(C['workspace'],'rev-list',C['base']+'..HEAD').splitlines())==4
state={k:{'head':git(C[k],'rev-parse','HEAD'),'status':git(C[k],'status','--porcelain=v1')}for k in ['workspace','canonical']}
assert state['workspace']=={'head':a['head'],'status':''}and state['canonical']=={'head':C['published_base'],'status':''}
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
# Read-only liveness observation; never replay the timed-out raw SDK utility.
observation=subprocess.check_output(['powershell','-NoProfile','-Command',"@(Get-Process -Id 35264,31340 -ErrorAction SilentlyContinue | Select-Object Id,ProcessName) | ConvertTo-Json -Compress"],text=True).strip()
assert observation in ['', '[]'],observation
named=read(R8/'named-native-gates/gate-results.json');headers={};artifacts={}
for row in named['rows']:
 assert row['gate_satisfied']
 for p,h in row['artifacts'].items():assert sha(p)==h,p;artifacts[p]=h
 for p,h in row['actual_included_headers'].items():assert sha(p)==h,p;headers[p]=h
safeguards={'head':a['head'],'original_probe_head':C['base'],'original_probe_report_sha256':sha(R8/'named-native-gates/gate-results.json'),
 'same_actual_probes_no_new_compile':True,'all_headers_unchanged':True,'actual_header_hashes_rechecked':len(headers),'original_probe_artifact_hashes_rechecked':len(artifacts),
 'four_positive_two_expected_negative_gates_retained':True,'scope':'Source-only CPP delta with no incoming CPP include edges; actual native/header probe source,object,listing/log and included headers unchanged. RCE/World/Render/Main and HUD/WorldEntity negatives retained without recompile or guard edits.'}
save(HERE/'native-safeguard-current-reuse.json',safeguards)
baseaudit=read(C['baseline_artifact_manifest']);pubmanifest=read(C['published_artifact_manifest']);s=a['summary'];pub=a['published_comparison'];ext=a['external_inputs'];now=datetime.now(timezone.utc).isoformat()
summary={'head':a['head'],'base':C['base'],'published_head':C['published_base'],'coverage_kind':'Complete-coverage COMPOSITE, NOT a fresh full native sweep','requested':3615,'passed':3243,'failed':372,
 'source_requested':s['source_requested'],'source_passed':s['source_passed'],'fresh_actual_compiles':14,'retained_actual_R8_rows':3601,'changed_CPP':13,'changed_headers':0,'originals':4,'new_files':[],'inventory_change':False,
 'versus_R8':s,'versus_published_wave7':pub,'input_hashes':6299,'actual_artifact_hash_entries':sum(len(v)for v in a['artifact_manifest'].values()),
 'R8_artifact_entries_verified':sum(len(v)for v in baseaudit['artifact_manifest'].values()),'published_artifact_entries_verified':sum(len(v)for v in pubmanifest['artifact_manifest'].values()),
 'external_include_files':sum(len(v['files'])for v in ext['include_trees'].values()),'compiler_runtime_files':len(ext['compiler_and_runtime']),'clean_pinned_submodules':len(ext['submodules']),
 'targeted_gate':{'requested':14,'passed':14,'failed':0,'source_of_13_plus_unchanged_consumer':plan['unchanged_direct_consumer'],'showIncludes':True,'actual_unique_headers':read(HERE/'targeted-stage1-actual-includes.json')['unique_actual_headers'],
  'compiler_session':38663,'compiler_exit':0,'binder_session':13751,'binder_exit':0},
 'reuse_proof':str(HERE/'reuse-proof-terminal.json'),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),'native_safeguard_reuse_sha256':sha(HERE/'native-safeguard-current-reuse.json'),
 'CPP_include_scope':'No include edge into any13 changed CPP. Unrelated Lua C amalgamation includes remain listed and unchanged; not falsely described as absent. Only LUA_USER_H nonliteral site,not defined in actual source/commands.',
 'KD_constant':'Actual new LineQuery/Fine pair both compiler COMDAT,00000020. Existing Root prepared-native strict1-to0 duplicate proof preserved.',
 'KD_evidence_supplement':str(HERE/'kd-symbols-and-link-timeout-supplement.json'),'KD_evidence_supplement_sha256':sha(HERE/'kd-symbols-and-link-timeout-supplement.json'),
 'optional_link_timeout':{'session':15934,'script_exit':1,'native_exit_observed':False,'timeout_seconds':180,'wrong_utility':kd['new_optional_link_attempt']['raw_SDK_utility_path'],
  'correct_prepared_utility':kd['new_optional_link_attempt']['correct_prepared_utility_path'],'no_owned_link_job_remains':True,'rerun':False,'scope':'Inconclusive optional baseline attempt using raw SDK utility; no native returncode,duplicate verdict or EXE inferred. Root instructed no replay; source coverage unaffected.'},
 'report':a['report'],'report_sha256':a['report_sha256'],'artifact_audit':str(auditpath),'artifact_audit_sha256':sha(auditpath),'plan_sha256':sha(C['current_plan']),
 'source_binding_sha256':sha(HERE/'intake-source-binding.json'),'canonical_and_intake':state,'parent_gitlink_index':plan['parent_gitlink_index'],
 'limits':'Existing Geometry/Frustum/nativeCross,Scene/native declaration and debug-render compile/codegen scopes retained. No optional owner suites,full runtime/byte identity,forced unresolved,game image/link/launch or publication claim.',
 'excluded':'Later camera/effects15 CPPs,GameModule,resource header/provider and all other R10 followups excluded. No diagnostic link-input exclusions.',
 'formal_review':False,'publication_allowed':False,'goal':'ACTIVE; native source/provider/link/image/launch work remains','created_utc':now}
path=HERE/'FROZEN_COMPOSITE_DIAGNOSTIC_R9.json';md=HERE/'FROZEN_COMPOSITE_DIAGNOSTIC_R9.md';save(path,summary)
text=(f"# Frozen R9 composite native diagnostic\n\nClean frozen intake `{a['head']}`. Complete coverage: **3243 PASS / 372 FAIL / 3615**. This is explicitly **14 fresh compiles + 3601 verified R8 rows**, not a fresh full sweep.\n\n"
 f"All3231 R8 passes retained,12 gains,zero losses/timeouts. Published Wave7:{pub['prior_passes_retained']}/3400 passes retained;{len(pub['regressions'])} historical pass regressions remain,{len(pub['new_passes'])} gains.\n\n"
 'Four exact originals change13 existing CPPs;no headers,new files or inventory changes. All13 plus unchanged FineIntersection consumer compile14/14 with showIncludes against final merged headers. Only13 CPP hashes differ;all other source/header hashes,compiler flags/tools,external context and submodule pins match R8. No include edge reaches a changed CPP. Unrelated Lua C amalgamation includes remain recorded. Every reused object/listing/log is hash-verified and per-row provenance is explicit.\n\n'
 f"All6299 source inputs and{summary['actual_artifact_hash_entries']} composite artifact entries are bound;{summary['R8_artifact_entries_verified']} R8 and{summary['published_artifact_entries_verified']} published artifact hashes verified. External files766;compiler/runtime files7;EA pins3. Existing actual native/header safeguards and all independent negatives remain bound without new tests or guard edits.\n\n"
 'Actual new LineQuery/Fine objects both emit the matching native COMDAT constant32. Root\'s existing prepared-tool strict link proves the old duplicate1→0 transition. The extra local baseline recheck used raw SDK bin/win32/link.exe instead of prepared tool-host/link.exe and timed out after180 seconds;no native exit/duplicate result or executable is claimed. The response and wrong-tool path/hash are retained. No owned link process remains and no retry was run. This does not invalidate the compile/coverage result.\n\n'
 f"Composite `{a['report']}` SHA256 `{a['report_sha256']}`. Audit `{auditpath}` SHA256 `{sha(auditpath)}`.\n\n"
 'Canonical remains clean published3534;main/dev/origin/parent index/ledger unchanged. Later sources were excluded. No formal review/publication or full-game link/image/launch proof;whole-native goal remains active.\n')
with md.open('x',encoding='utf-8')as f:f.write(text)
completion={'head':a['head'],'kind':summary['coverage_kind'],'compiler_session':38663,'compiler_exit':0,'binder_session':13751,'binder_exit':0,'optional_link_timeout_explicit':True,
 'report_sha256':a['report_sha256'],'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),
 'canonical_and_intake':state,'goal':'ACTIVE','no_publication':True};save(HERE/'composite-completion.json',completion)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath)
o.update(phase='wave8_repair_r9_composite_terminal_bound',frozen_head=a['head'],intake_head=a['head'],live_session=None,live_process_id=None,compiler_terminal_session=38663,compiler_terminal_exit=0,
 binder_terminal_session=13751,binder_terminal_exit=0,terminal_passed=3243,terminal_failed=372,terminal_requested=3615,terminal_report=a['report'],terminal_report_sha256=a['report_sha256'],
 terminal_audit=str(auditpath),terminal_audit_sha256=sha(auditpath),current_diagnostic=str(path),current_diagnostic_sha256=sha(path),current_coverage_kind='R9 composite14fresh+3601verifiedR8; no full replay',
 published_passes_retained=pub['prior_passes_retained'],actual_regression_count=len(pub['regressions']),actual_gain_count=len(pub['new_passes']),R8_regressions=0,R8_gains=12,
 auxiliary_link_handle=None,optional_link_attempt='Raw SDK linker180s timeout,not a native exit; no owned link remains,no rerun; actualnew COMDAT32 +Root prepared strict proof retained',
 source_freeze_status='FROZEN clean R9 composite checkpoint; no later sources authorized',compiler_reservation_released=True,current_review_status='Not requested;published regressions remain',updated_utc=now,
 instruction='R9 targeted14 and composite artifact binding actual terminal. Explicit3601 reusedR8 rows,not a fresh full sweep. Preserve old negatives and raw-link timeout; no source expansion,optional suites,publication or main/dev/index/ledger changes.')
ownerpath.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8');ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text(text,encoding='utf-8')
print(json.dumps({'head':a['head'],'coverage_passed':3243,'coverage_failed':372,'fresh':14,'retained':3601,'R8_losses':0,'R8_gains':12,'audit_sha256':sha(auditpath),'summary_sha256':sha(path),'summary_md_sha256':sha(md)},indent=2))
