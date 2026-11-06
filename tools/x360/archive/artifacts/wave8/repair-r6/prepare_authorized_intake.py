"""Prepare NEW R6 action records for the exact Root-reviewed finite proposal."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
BASE='04ed276fcd79c0d91e5e91db9cd08438c4e0689d'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(p,*a):return subprocess.check_output(['git','--no-optional-locks','-C',str(p),*a]).decode().strip()
def save(p,d):
 with Path(p).open('x',encoding='utf-8') as f:json.dump(d,f,indent=2);f.write('\n')
planpath=HERE/'R6_PREFLIGHT_PLAN_01.json';assert sha(planpath)=='fe3c4c2fb56ab29b532c3b4f726bf2b93e7138539ff5db90e9e25d60cebbb4a6'
plan=read(planpath);assert git(WORK,'rev-parse','HEAD')==BASE and not git(WORK,'status','--porcelain=v1') and not git(WORK,'branch','--show-current')
assert git(PARENT/'b5-decomp','rev-parse','HEAD')==plan['published_base'] and not git(PARENT/'b5-decomp','status','--porcelain=v1')
assert git(PARENT,'ls-files','--stage','b5-decomp')==plan['parent_gitlink_index']
prior=HERE.parent/'repair-r5';meta=read(prior/'native-parallel-wave8-r5-full-stage1-patch.json')
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
for x in read(HERE/'R6_RECEIPT_BINDINGS_02.json')['primary']:assert sha(x['path'])==x['expected_sha256']
approved={**plan,'state':'ROOT_AUTHORIZED_EXACT_R6_INTAKE_AND_GATES','collectable':True,'collect_now':plan['proposed_originals'],'reviewed_plan':str(planpath),'reviewed_plan_sha256':sha(planpath),'authorization':'Root explicitly authorized exact14 originals/44paths;277 selected plus named ABI/current-object/negative checks, then ONE stable full3615/6298 if204 selected R5passes retain. No other candidates or source correction without Root review.','authorized_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'intake-plan-approved-01.json',approved)
config={'campaign':'wave8-repair-r6','base':BASE,'published_base':plan['published_base'],'workspace':str(WORK),'canonical':str(PARENT/'b5-decomp'),'artifact_root':str(HERE),'baseline_metadata':str(prior/'native-parallel-wave8-r5-full-stage1-patch.json'),'baseline_report':str(prior/'full-stage1/compile-results.json'),'baseline_artifact_manifest':str(prior/'full-stage1-terminal-audit.json'),'published_report':str(HERE.parent.parent/'wave7/full-successor1/compile-results.json'),'published_artifact_manifest':str(HERE.parent.parent/'wave7/native-parallel-wave7-final-artifact-manifest.json'),'baseline_passed':3105,'baseline_failed':511,'baseline_requested':3616,'current_plan':str(HERE/'intake-plan-approved-01.json')}
for key in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:config[key+'_sha256']=sha(config[key])
save(HERE/'campaign.json',config)
save(HERE/'repair-r6-receipts.json',{'workspace':str(WORK),'base':BASE,'head':BASE,'state':'complete','queue':[],'todo':[],'receipts':[],'updated_utc':datetime.datetime.now(datetime.timezone.utc).isoformat() if False else datetime.now(timezone.utc).isoformat()})
text=(prior/'integrate.py').read_text()
text=text.replace('repair-r5-receipts.json','repair-r6-receipts.json').replace('wave8_repair_r5','wave8_repair_r6').replace('Wave8 R5','Wave8 R6').replace('R5 source union, preserving R4','R6 source union, preserving R5').replace('native Wave8 R5','native Wave8 R6')
text=text.replace('Wave8 R6 finite 13 plus one declaration repair intake. Source not frozen; no compiler. Preserve R4 full 3098/518 and its 354 published regressions, plus published3534. Only exact approved R5 cohorts; no newXCam/GUI/Physics/Volume/unlisted additions or publication.','Wave8 R6 exact14-original intake,44paths, one failed Boundary removal. Source not frozen; no compiler. Preserve R5 full3105/511 and352published regressions plus published3534. No unlisted candidates/source changes/publication.')
text=text.replace('Collect only the 13 originals plus explicit Traffic declaration correction in the pinned R3 plan, preserving R4 and published source edits. No files are new; no new product CPP. all XCam/GUI/Physics/Volume and other unnamed additions remain excluded.','Collect only14 originals/44paths in the Root-reviewed R6 plan, preserving all R5/published edits. No newfiles; retire only failed BootLegalBoundary. All unlisted Volume/SAT/LineSeg/later18CPP/RootStuntPolygon/geometry work excluded.')
text=text.replace('Current R4 full baseline remains3098/3616PASS/518FAIL/354published regressions; no new globalresult.','Current R5 full baseline remains3105/3616PASS/511FAIL/352published regressions; no new global result.')
# Verify each exact path preimage immediately before its authorized cherry-pick.
text=text.replace("    result = git('cherry-pick', commit, check=False)","    candidate = next(c for c in PLAN['candidates'] if c['commit'] == commit)\n    for change in candidate['paths']:\n        current = git('rev-parse', 'HEAD:' + change['path']).stdout.strip()\n        assert current == change['preimage'], ('Changed preimage; stop before mutation', commit, change['path'], current, change['preimage'])\n    result = git('cherry-pick', commit, check=False)")
with (HERE/'integrate.py').open('x',encoding='utf-8') as f:f.write(text)
print(json.dumps({'base':BASE,'reviewed_plan_sha256':sha(planpath),'approved_plan_sha256':sha(HERE/'intake-plan-approved-01.json'),'new_queue':14,'rechecked_R5_inputs':len(meta['source_snapshot']['hashes'])}))
