"""Pin the exact Root-authorized four-node R10 source-only graph."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent;R9=HERE.parent/'repair-r9';PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='373dc4b41168751c3fc0d3e14969527ced237453';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4';COMMITS=['2362b528be6096efe97b86732ff58ba1ab8e9d3d','038e40e50efa4c3736dc5cf1bb45cbf7bde6f17d','9c3d2db104691d347ab41eb4967897397c06bed1','024948c896b854855e46c99ef885ebc6b595e588']
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=PARENT/'b5-decomp').decode().strip()==PUB and not git('status','--porcelain=v1',cwd=PARENT/'b5-decomp').strip()
parent_index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==parent_index
p=ART/'wave9/r10-preflight-luna-20261102/preflight.json';raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()=='6edff85b52d56cc4c23d11c5f65a41070d16bfbfa77ea1ee67cfd269a23df85d';pre=json.loads(raw);assert pre['commits']['order']==COMMITS
with(HERE/'R10_REVIEWED_PREFLIGHT_01.json').open('xb')as f:f.write(raw)
for receipt in pre['receipts']:assert sha(receipt['path'])==receipt['sha256']
assert pre['receipts'][2]['sha256']=='e2a0a0e3088e3ad7ab03e5aa40829d0ddbc757ec2d32acee874c072ef49c786e'
tree={}
for record in git('ls-tree','-rz','--full-tree',BASE,'--','src','vendor').split(b'\0'):
 if record:
  info,p=record.split(b'\t',1);tree[p.decode()]=info.decode().split()[2]
images=dict(tree);first={};candidates=[];entries=0
for i,commit in enumerate(COMMITS):
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',commit).decode().splitlines():
  info,p=line.split('\t',1);_,_,old,new,status=info.split();assert status=='M'and p.endswith('.cpp')and images[p]==old,(commit,p,images.get(p),old)
  first.setdefault(p,old);images[p]=new;changes.append({'path':p,'preimage':old,'postimage':new,'status':status});entries+=1
 receipt=pre['receipts'][i];candidates.append({'commit':commit,'label':pre['commits']['metadata'][i]['name'],'paths':changes,'receipt':receipt['path'],'receipt_sha256':receipt['sha256']})
assert len(first)==15 and entries==16
a=read(R9/'composite-artifact-audit.json');idx={r['source']:r for r in read(a['report'])['results']};selected=sorted(first)
plan={'state':'ROOT_AUTHORIZED_EXACT_FOUR_SOURCE_ONLY','base':BASE,'published_base':PUB,'proposed_originals':COMMITS,'collect_now':COMMITS,'candidates':candidates,'unique_paths':selected,
 'planned_source_images':{p:{'baseline':first[p],'final':images[p]}for p in first},'owned_CPP_sources':selected,'changed_headers':[],'new_paths':[],'removed_paths':[],
 'selected_union_native_sources':selected,'selected_requested':15,'selected_R9_passes':sum(idx[p]['passed']for p in selected),'selected_R9_failed':sum(not idx[p]['passed']for p in selected),
 'required_order':[['038e40e50efa4c3736dc5cf1bb45cbf7bde6f17d','9c3d2db104691d347ab41eb4967897397c06bed1']],
 'expected_native_inventory':3615,'expected_native_text_inputs':6299,'parent_gitlink_index':parent_index,'reviewed_preflight_sha256':hashlib.sha256(raw).hexdigest(),
 'reuse_requirements':['All15 final actual native products pass with showIncludes/pre-post bindings.','Only15 CPP contents differ; all headers/other code and tool/external/submodule/flags context equal R9.','No CPP inclusion can carry a changed body into an unaffected TU. Verify every reused R9 object,assembly and log against immutable composite audit.','Assemble explicit complete coverage15 fresh+3600 retained actual R9 rows,never claim3615 freshly compiled. No unnecessary global replay.'],
 'required_gates':['Exact4 normal source-only originals; stop on unexpected path/header/preimage/conflict. No private ancestry/extra source.','CameraUtils038e inseparable from Wzero9c3d correction; superseded old CameraShake object not reused. Preserve all original failed receipts/semantic limits.','All3243 R9 passes retained in composite. Native guards and independent layout/FP/runtime negatives remain,with explicit reused provenance.','No optional link or owner suites; use existing author source/native evidence in bounded scopes. NativeRender header/provider and Main42585 remain excluded.'],
 'known_limits':pre['limits'],'authorization':'Root reviewed effects,camera behavior/type qualifiers,expanded CameraShake/PositionLag and exact W-lane correction; authorizes exact4 once fresh finite preflight is pinned,15 affected native compiles and verified-unaffected composite. No further approval within scope.',
 'Root_handoff_note':'ROOT_NATIVE_CURRENT_20261102.md is historical regarding pending R10 authorization; the later explicit Root task authorizes this finite source collection.',
 'source_changed':False,'compiler_started':False,'formal_review':False,'publication_allowed':False,'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R10_PREFLIGHT_PLAN_01.json',plan)
old=read(R9/'campaign.json');config={'campaign':'wave8-repair-r10','base':BASE,'published_base':PUB,'workspace':str(WORK),'canonical':str(PARENT/'b5-decomp'),'artifact_root':str(HERE),
 'baseline_metadata':str(R9/'native-parallel-wave8-r9-targeted-stage1-patch.json'),'baseline_report':a['report'],'baseline_artifact_manifest':str(R9/'composite-artifact-audit.json'),
 'published_report':old['published_report'],'published_artifact_manifest':old['published_artifact_manifest'],'baseline_passed':3243,'baseline_failed':372,'baseline_requested':3615,'current_plan':str(HERE/'R10_PREFLIGHT_PLAN_01.json')}
for k in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:config[k+'_sha256']=sha(config[k])
save(HERE/'campaign.json',config);save(HERE/'repair-r10-receipts.json',{'state':'complete','workspace':str(WORK),'head':BASE,'receipts':[],'queue':[],'todo':[]})
save(HERE/'R10_ROOT_AUTHORIZATION_01.json',{'plan_sha256':sha(HERE/'R10_PREFLIGHT_PLAN_01.json'),'authorization':plan['authorization'],'originals':COMMITS,'preflight_sha256':hashlib.sha256(raw).hexdigest()})
with(HERE/'R10_PREFLIGHT_SUMMARY_01.md').open('x',encoding='utf-8')as f:f.write('# Authorized R10 source-only plan\n\nFour originals,15 unique existing CPPs and16 path deltas. CameraUtils must be followed by the exact Wzero constructor correction;the old CameraShake object is superseded. No headers/new files/inventory changes.\n\nCompile15 actual final TUs with showIncludes and frozen merged headers. Reuse3600 unaffected R9 rows only after source/header/tool/external/flags identity,no changed CPP inclusion and artifact verification. Preserve all3243 R9 passes and old negatives. No full replay,optional link/owner suites,NativeRender/Main integration or publication.\n')
print('PINNED',sha(HERE/'R10_PREFLIGHT_PLAN_01.json'),'4originals 15CPP targeted15','prior_passes',plan['selected_R9_passes'],flush=True)
