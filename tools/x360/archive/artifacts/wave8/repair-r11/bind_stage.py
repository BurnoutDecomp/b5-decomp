"""Bind actual R8 terminal input/artifact evidence; retain every old/raw failure."""
import argparse,hashlib,importlib.util,json,sys
from datetime import datetime,timezone
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument('--name',required=True);args=parser.parse_args()
config=json.loads((HERE/'campaign.json').read_text());plan=json.loads(Path(config['current_plan']).read_text())
spec=importlib.util.spec_from_file_location('r11_actual_native_terminal',HERE.parent.parent/'wave5-delivery/wave5_delivery.py')
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);e.BASE=config['base']
meta_path=HERE/('native-parallel-wave8-r11-'+args.name+'-patch.json');meta=e.read(meta_path)
assert e.git('rev-parse','HEAD').decode().strip()==meta['head']
assert not e.git('status','--porcelain=v1').strip()
assert hashlib.sha256(e.git('diff','--binary',meta['base'])).hexdigest()==meta['sha256']
assert e.common.source_snapshot()==meta['source_snapshot']
assert set(meta['paths'])==set(plan['unique_paths'])
removed=set(plan['removed_paths']);assert len(removed)==0
assert set(meta['approved_removed_sources'])==removed
for p,h in meta['source_sha256'].items():
 if h is None:assert p in removed and not (e.WORK/p).exists()
 else:assert e.sha(e.WORK/p)==h,p
assert e.git('rev-parse','HEAD',cwd=e.CANONICAL).decode().strip()==config['published_base']
assert not e.git('status','--porcelain=v1',cwd=e.CANONICAL).strip()
assert e.git('ls-files','--stage','b5-decomp',cwd=e.PARENT).decode().strip()==plan['parent_gitlink_index']
report_path=HERE/args.name/'compile-results.json';report,index,artifacts=e.common.report_check(report_path)
assert set(index)==set(meta['selected_sources'])
exit_path=HERE/(args.name+'-exit.json');terminal=e.read(exit_path)
assert terminal['head']==meta['head'] and terminal['report_sha256']==e.sha(report_path)
assert terminal['exit_code']==(1 if report['failed'] else 0)
prior=e.read(config['baseline_report']);old={r['source']:r for r in prior['results']}
assert e.sha(config['baseline_report'])==config['baseline_report_sha256']
external=e.external_inputs(report);before=e.read(HERE/(args.name+'-external-inputs-before.json'))['inputs']
for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==before[k]
if meta['selection']=='full':
 assert len(index)==len(meta['native_inventory'])==3616 and set(index)==set(meta['native_inventory'])
 assert set(index)=={p for p in e.common.inventory() if Path(p).suffix.lower() in ['.c','.cpp']}
 summary=e.compare(report,prior)
 assert set(summary['removed_sources'])==removed and set(summary['new_sources'])=={p for p in plan['new_paths']if p.endswith('.cpp')}
 assert all(not old[p]['passed'] for p in removed),'No R7PASS retirement authorized'
 manifest_path=Path(config['baseline_artifact_manifest']);assert e.sha(manifest_path)==config['baseline_artifact_manifest_sha256']
 manifest=e.read(manifest_path);e.verify_manifest(prior,manifest['artifact_manifest'])
 assert manifest['report_sha256']==config['baseline_report_sha256']
 published=e.read(config['published_report']);pm=e.read(config['published_artifact_manifest'])
 assert e.sha(config['published_report'])==config['published_report_sha256']
 assert e.sha(config['published_artifact_manifest'])==config['published_artifact_manifest_sha256']
 e.verify_manifest(published,pm['artifact_manifest']);published_comparison=e.compare(report,published)
else:
 summary={'selected_requested':len(index),'passed':report['passed'],'failed':report['failed'],'regressions':[s for s,r in index.items() if old.get(s,{}).get('passed') and not r['passed']],'new_passes':[s for s,r in index.items() if r['passed'] and not old.get(s,{}).get('passed',False)],'timeouts':[s for s,r in index.items() if r['returncode']==-1],'R10_prior_passes_in_selection':sum(old.get(s,{}).get('passed',False) for s in index),'NOT_A_GLOBAL_RESULT':True}
 assert summary['R10_prior_passes_in_selection']==plan['selected_R10_passes']
assert len(meta['source_snapshot']['hashes'])==6302
result={'published_comparison':published_comparison if meta['selection']=='full' else None,'head':meta['head'],'base':meta['base'],'patch_sha256':meta['sha256'],'metadata':str(meta_path),'metadata_sha256':e.sha(meta_path),'report':str(report_path),'report_sha256':e.sha(report_path),'exit_receipt':str(exit_path),'exit_receipt_sha256':e.sha(exit_path),'summary':summary,'approved_failed_source_retirement':list(removed),'artifact_manifest':artifacts,'source_snapshot':meta['source_snapshot'],'external_inputs':external,'receipts_sha256':meta['receipt_sha256'],'goal':'ACTIVE; diagnostic only, strict link/image/launch unproved','verifier_sha256':e.sha(__file__),'created_utc':datetime.now(timezone.utc).isoformat()}
e.save_new(HERE/(args.name+'-terminal-audit.json'),result)
print(json.dumps({'audit_sha256':e.sha(HERE/(args.name+'-terminal-audit.json')),'head':meta['head'],'report_sha256':e.sha(report_path),'requested':report['requested'],'passed':report['passed'],'failed':report['failed'],'R10_regressions':len(summary['regressions']),'new_gains':len(summary.get('new_passes',[])),'published_regressions':len(published_comparison['regressions']) if meta['selection']=='full' else None,'artifacts':sum(len(v) for v in artifacts.values()),'full':meta['selection']=='full'},indent=2))
