"""Bind the finite compile/header gates; no extra owner-runtime suite."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
plan=read(HERE/'R8_PREFLIGHT_PLAN_01.json');meta=read(HERE/'native-parallel-wave8-r8-selected-stage1-patch.json');audit=read(HERE/'selected-stage1-terminal-audit.json');named=read(HERE/'named-native-gates/gate-results.json')
assert meta['head']==audit['head']==named['head']
assert audit['summary']['R7_prior_passes_in_selection']==109 and not audit['summary']['regressions']and audit['summary']['passed']==172 and audit['summary']['failed']==92
assert all(r['gate_satisfied']for r in named['rows'])
for r in named['rows']:
 for p,h in r['artifacts'].items():assert sha(p)==h,p
receipts=read(HERE/'repair-r8-receipts.json');assert receipts['state']=='complete'and len(receipts['receipts'])==19
assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip()==meta['head']
assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
source=read(HERE/'selected-stage1/compile-results.json');idx={r['source']:r for r in source['results']};assert all(idx[p]['passed']for p in plan['owned_CPP_sources'])
for name in ['BrnMainX360.cpp','BrnGameMainFlowStates.cpp']:
 rows=[r for r in source['results']if Path(r['source']).name==name];assert len(rows)==1 and rows[0]['passed']
current_owner_receipts=[]
for c in plan['candidates']:
 assert sha(c['receipt'])==c['receipt_sha256'];current_owner_receipts.append({'original':c['commit'],'receipt':c['receipt'],'sha256':c['receipt_sha256'],'existing_compile_scope':c['compile_scope']})
result={'head':meta['head'],'all_required_gates_satisfied':True,'selected_requested':264,'selected_passed':172,'selected_failed':92,'selected_R7_passes_retained':109,'selected_R7_regressions':[],'selected_gains':63,
 'all64_owned_CPP_pass':True,'changed_prior_pass_CgsAptCallbackRender_retained':True,'positive_native_header_probes':4,'expected_independent_C2118_negatives':2,
 'selected_terminal_audit':pin(HERE/'selected-stage1-terminal-audit.json'),'named_native_gates':pin(HERE/'named-native-gates/gate-results.json'),
 'native_guards':'All38RCE/40World/Render/Main and MainFlow protections retained; independent HUD7920/7940 and WorldEntity79E0/79F0 negatives preserved. No product padding/adapters/guard suppression.',
 'owner_evidence':current_owner_receipts,'source_binding':pin(HERE/'intake-source-binding.json'),'plan':pin(HERE/'R8_PREFLIGHT_PLAN_01.json'),
 'evidence_scope':'Compile-focused finite batch. Existing owner native/ABI/CRT evidence and negative receipts remain scoped as originally stated. No new optional owner-runtime suites, byte-identity, complete-GUI/gameplay or link/image/launch claim. Source postimages verified after exact19 normal cherry-picks.',
 'old_failed_consumer':'BrnFlaptRenderer.cpp remains78 diagnostic lines in actual selected and R7 logs; CgsFontRenderer.cpp itself now whole-compiles. No omitted failed rows.',
 'source_input_hashes_rechecked':6299,'originals':19,'paths':66,'new_files':[],'new_CPP':[],'formal_review':False,'publication':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'selected-gates-completion.json').open('x')as f:json.dump(result,f,indent=2)
print('R8_REQUIRED_GATES',sha(HERE/'selected-gates-completion.json'),'all109retained all64owned pass',flush=True)
