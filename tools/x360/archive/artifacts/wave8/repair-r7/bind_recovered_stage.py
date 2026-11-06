"""Audit the complete diagnostic inventory with explicit interruption lineage."""
from datetime import datetime,timezone
import hashlib,json,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
oldpath=HERE/'full-stage1/compile-results.json';checkpoint=read(HERE/'full-stage1-interrupted-checkpoint.json');old=read(oldpath)
assert old['state']=='running'and len(old['results'])==1700 and sha(oldpath)==checkpoint['report_sha256']
missingpath=HERE/'full-missing-stage1/compile-results.json';missing=read(missingpath);exitpath=HERE/'full-missing-stage1-exit.json';terminal=read(exitpath)
assert missing['state']=='complete'and len(missing['results'])==missing['requested']==1915
assert terminal['report_sha256']==sha(missingpath)and terminal['exit_code']==(1 if missing['failed']else 0)
fullpath=HERE/'full-recovered/compile-results.json';full=read(fullpath)
assert full['state']=='complete'and full['results']==old['results']+missing['results']
assert len({r['source']for r in full['results']})==3615
assert not(HERE/'full-stage1-exit.json').exists()
subprocess.run([sys.executable,'-X','utf8',str(HERE/'bind_stage.py'),'--name','full-recovered'],check=True)
auditpath=HERE/'full-recovered-terminal-audit.json';audit=read(auditpath)
for source,hashes in checkpoint['completed_artifact_manifest'].items():assert audit['artifact_manifest'][source]==hashes
result={'head':audit['head'],'kind':'Completed diagnostic inventory after interrupted original process; not a full restart or second full sweep',
 'original_report':str(oldpath),'original_report_sha256':sha(oldpath),'original_state':'running/interrupted','original_process_exit_observed':False,
 'original_saved_rows':1700,'original_completed_artifacts_verified':sum(len(v)for v in checkpoint['completed_artifact_manifest'].values()),
 'interrupted_checkpoint_sha256':sha(HERE/'full-stage1-interrupted-checkpoint.json'),
 'missing_report':str(missingpath),'missing_report_sha256':sha(missingpath),'missing_rows':1915,'missing_process_exit':terminal['exit_code'],
 'missing_exit_receipt_sha256':sha(exitpath),'diagnostic_report':str(fullpath),'diagnostic_report_sha256':sha(fullpath),
 'diagnostic_terminal_audit':str(auditpath),'diagnostic_terminal_audit_sha256':sha(auditpath),
 'counts':{'requested':3615,'passed':full['passed'],'failed':full['failed']},'versus_R6':audit['summary'],'versus_published':audit['published_comparison'],
 'same_frozen_6299_inputs_flags_external_tools_and_submodules':True,
 'scope':'Completed rows are actual original artifacts with exact saved command provenance and unchanged frozen inputs/context. Only absent recorded inventory was compiled in fresh output. Unrecorded original artifacts remain preserved and are not promoted. Aggregate diagnostic status is not an invented original compiler exit. No parity/link/image/launch/publication claim.',
 'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'full-recovery-terminal-supplement.json').open('x')as f:json.dump(result,f,indent=2)
print('RECOVERY_AUDIT',sha(auditpath),'SUPPLEMENT',sha(HERE/'full-recovery-terminal-supplement.json'),flush=True)
