"""Prove source-only reuse and assemble20 fresh+3596 retained actual rows."""
from datetime import datetime,timezone
import hashlib,importlib.util,json,sys
sys.dont_write_bytecode=True
from pathlib import Path
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
C=read(HERE/'campaign.json');plan=read(C['current_plan']);s=importlib.util.spec_from_file_location('r12_composite_evidence',HERE.parent.parent/'wave5-delivery/wave5_delivery.py');e=importlib.util.module_from_spec(s);s.loader.exec_module(e);e.BASE=C['base']
meta=read(HERE/'native-parallel-wave8-r12-targeted-stage1-patch.json');e.check_current(meta)
assert e.git('rev-parse','HEAD',cwd=e.CANONICAL).decode().strip()==C['published_base']and not e.git('status','--porcelain=v1',cwd=e.CANONICAL).strip()
assert e.git('ls-files','--stage','b5-decomp',cwd=e.PARENT).decode().strip()==plan['parent_gitlink_index']
baseline_path=Path(C['baseline_report']);baseaudit_path=Path(C['baseline_artifact_manifest']);assert sha(baseline_path)==C['baseline_report_sha256']and sha(baseaudit_path)==C['baseline_artifact_manifest_sha256']
baseline,old,oldart=e.common.report_check(baseline_path);baseaudit=read(baseaudit_path);assert oldart==baseaudit['artifact_manifest'];e.verify_manifest(baseline,baseaudit['artifact_manifest'])
newpath=HERE/'targeted-stage1/compile-results.json';new,idx,newart=e.common.report_check(newpath);terminal=read(HERE/'targeted-stage1-exit.json')
assert new['requested']==new['passed']==20 and new['failed']==0 and set(idx)==set(plan['selected_union_native_sources'])
assert terminal['exit_code']==0 and terminal['head']==meta['head']and terminal['report_sha256']==sha(newpath)
def flags(row):return[a for a in row['command'][:-1]if not a.startswith(('/Fo','/Fa'))and a.lower()!='/showincludes']
for p,row in idx.items():assert flags(row)==flags(old[p]),p
before=read(HERE/'reuse-input-preconditions-before.json');assert before['all_other_source_and_header_hashes_unchanged']and not before['reverse_edges_into_changed_CPP']and before['no_literal_or_known_nonliteral_include_of_changed_CPP']
assert set(before['changed_only20_CPP'])==set(plan['unique_paths'])
current=e.common.source_snapshot();previous=baseaudit['source_snapshot'];changed={p for p in current['hashes']if current['hashes'][p]!=previous['hashes'][p]}
assert changed==set(plan['unique_paths'])and set(current['hashes'])==set(previous['hashes'])and all(Path(p).suffix.lower()=='.cpp'for p in changed)
assert current==meta['source_snapshot']and len(current['hashes'])==6302
external=e.external_inputs(new)
for key in ['include_trees','submodules','compiler_and_runtime']:
 assert external[key]==baseaudit['external_inputs'][key]==read(HERE/'targeted-stage1-external-inputs-before.json')['inputs'][key]==read(HERE/'targeted-stage1-external-inputs-after.json')['inputs'][key]
actual=read(HERE/'targeted-stage1-actual-includes.json');assert actual['all_actual_includes_frozen_before_and_unchanged_after']and len(actual['rows'])==20
for hdrs in actual['rows'].values():
 for p,h in hdrs.items():assert sha(p)==h,p
for p,row in old.items():
 if p not in idx:assert sha(e.WORK/p)==previous['hashes'][p],p
proof={'head':meta['head'],'base':C['base'],'fresh_sources':sorted(idx),'fresh_count':20,'retained_R11_count':3596,
 'R11_report':str(baseline_path),'R11_report_sha256':sha(baseline_path),'R11_audit_sha256':sha(baseaudit_path),'all_R11_artifact_entries_verified':sum(len(x)for x in oldart.values()),
 'only20_CPP_changed':sorted(changed),'all_other6282_inputs_unchanged':True,'no_headers_new_or_removed':True,'actual20_flags_equal_R11_except_showIncludes_and_output_paths':True,
 'actual20_includes':str(HERE/'targeted-stage1-actual-includes.json'),'actual20_includes_sha256':sha(HERE/'targeted-stage1-actual-includes.json'),
 'reverse_CPP_include_edges':{},'unrelated_C_amalgamation_note':'The precondition scan retains Lua etc/all.c literal C includes. None is a changed CPP and no reverse include edge reaches any20 changed CPP. No claim that all C amalgamation includes are absent.',
 'nonliteral_scope':'Only LUA_USER_H; absent from all source definitions and actual baseline command definitions. Header/external/tool contexts unchanged.',
 'external_include_compiler_runtime_submodule_context_equal':True,'native_inventory_equal3616':True,'no_global_recompile':True,'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'reuse-proof-terminal.json',proof)
composite={k:v for k,v in baseline.items()if k!='results'};composite['results']=[]
for row in baseline['results']:
 fresh=row['source']in idx;origin=idx[row['source']]if fresh else row
 if fresh:
  value={**origin,'build_head':meta['head'],'origin_kind':'fresh R12 targeted actual compile','origin_report':str(newpath),'origin_report_sha256':sha(newpath)}
 else:
  # R11 is a real full sweep. Attribute retained rows to its actual source head and report.
  value={**origin,'build_head':origin.get('build_head',C['base']),'origin_report':origin.get('origin_report',str(baseline_path)),'origin_report_sha256':origin.get('origin_report_sha256',C['baseline_report_sha256']),'retained_via_report':str(baseline_path),'retained_via_report_sha256':C['baseline_report_sha256'],'coverage_layer_role':'unaffected verified R11 coverage row retained'}
  assert value['build_head']==row.get('build_head',C['base'])and value['origin_report']==row.get('origin_report',str(baseline_path))and value['origin_report_sha256']==row.get('origin_report_sha256',C['baseline_report_sha256'])
 composite['results'].append(value)
composite.update(state='complete',requested=3616,passed=sum(r['passed']for r in composite['results']));composite['failed']=3616-composite['passed']
composite['provenance']={'kind':'Complete-coverage source-only composite; NOT a fresh full sweep','source_head':meta['head'],'freshly_compiled':20,'retained_prior_actual_rows':3596,
 'source_only_changed_CPP':20,'unchanged_consumer_rebuilt':plan.get('unchanged_direct_consumer'),'reuse_proof':str(HERE/'reuse-proof-terminal.json'),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),
 'R11_report_sha256':sha(baseline_path),'R12_targeted_report_sha256':sha(newpath),'per_row_build_and_report_attribution':True,'underlying_prior_origins_preserved':True,'no_full_process_exit_claim':True}
effective=HERE/'native-parallel-wave8-r12-composite-effective.json';save(effective,composite)
_,effective_index,art=e.common.report_check(effective);assert len(effective_index)==3616 and art=={**oldart,**newart}
summary=e.compare(composite,baseline);assert summary['prior_passes_retained']==3265 and not summary['regressions']and not summary['new_sources']and not summary['removed_sources']
pub=read(C['published_report']);pm=read(C['published_artifact_manifest']);assert sha(C['published_report'])==C['published_report_sha256']and sha(C['published_artifact_manifest'])==C['published_artifact_manifest_sha256'];e.verify_manifest(pub,pm['artifact_manifest']);published=e.compare(composite,pub)
audit={'head':meta['head'],'base':C['base'],'report':str(effective),'report_sha256':sha(effective),'kind':composite['provenance']['kind'],'summary':summary,'published_comparison':published,
 'artifact_manifest':art,'source_snapshot':current,'external_inputs':external,'targeted_actual_report':str(newpath),'targeted_actual_report_sha256':sha(newpath),'targeted_exit_receipt_sha256':sha(HERE/'targeted-stage1-exit.json'),
 'fresh_actual_compiles':20,'retained_actual_R11_rows':3596,'reuse_proof':str(HERE/'reuse-proof-terminal.json'),'reuse_proof_sha256':sha(HERE/'reuse-proof-terminal.json'),'receipts_sha256':sha(HERE/'repair-r12-receipts.json'),
 'goal':'ACTIVE; compile coverage only, no strict full link/image/launch claim','created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'composite-artifact-audit.json',audit)
print(json.dumps({'head':meta['head'],'coverage_passed':composite['passed'],'coverage_failed':composite['failed'],'fresh':20,'retained':3596,'R11_regressions':len(summary['regressions']),'gains':len(summary['new_passes']),
 'published_regressions':len(published['regressions']),'artifacts':sum(len(x)for x in art.values()),'audit_sha256':sha(HERE/'composite-artifact-audit.json'),'report_sha256':sha(effective)},indent=2))
