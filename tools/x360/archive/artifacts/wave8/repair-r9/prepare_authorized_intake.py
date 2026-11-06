"""Pin exact four-original source-only R9 collection and targeted coverage plan."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent;R8=HERE.parent/'repair-r8';PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='42772cbe0386462d1c32c8442e9f58c9e534e75b';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4'
COMMITS=['7b4e83bc22f0734155a0b129addba026cf85e6ff','c423ad18e2c1ae64aaf0cee46d38cc092786eedc','843f8b60b545643ee1975076123272c7c2fb55d8','aa341f31ea78ac4ae402dce77aa9689fa62b52b1']
CONSUMER='src/GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule_wSQ1.cpp'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=PARENT/'b5-decomp').decode().strip()==PUB and not git('status','--porcelain=v1',cwd=PARENT/'b5-decomp').strip()
parent_index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==parent_index
preflight=ART/'wave9/r9-preflight-luna-20261102/preflight.json';raw=preflight.read_bytes();assert hashlib.sha256(raw).hexdigest()=='bf6f2998d121b5745cd0c350997aae00efe50dc44bae67309c1a3a65a27f15e2';pre=json.loads(raw)
with(HERE/'R9_REVIEWED_PREFLIGHT_01.json').open('xb')as f:f.write(raw)
assert[c['commit']for c in pre['commits']]==COMMITS
for r in pre['receipts']:assert sha(r['path'])==r['sha256']
tree={}
for record in git('ls-tree','-rz','--full-tree',BASE,'--','src','vendor').split(b'\0'):
 if record:
  info,p=record.split(b'\t',1);tree[p.decode()]=info.decode().split()[2]
images=dict(tree);first={};candidates=[]
for i,commit in enumerate(COMMITS):
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',commit).decode().splitlines():
  info,p=line.split('\t',1);_,_,old,new,status=info.split();assert status=='M'and p.endswith('.cpp')and images[p]==old,(commit,p,images.get(p),old)
  first.setdefault(p,old);images[p]=new;changes.append({'path':p,'preimage':old,'postimage':new,'status':status})
 receipt=pre['receipts'][i];candidates.append({'commit':commit,'label':pre['commits'][i]['lane'],'paths':changes,'receipt':receipt['path'],'receipt_sha256':receipt['sha256']})
assert len(first)==13 and CONSUMER not in first
a=read(R8/'full-stage1-terminal-audit.json');idx={r['source']:r for r in read(a['report'])['results']};selected=sorted(set(first)|{CONSUMER});assert len(selected)==14 and all(p in idx for p in selected)
plan={'state':'ROOT_AUTHORIZED_EXACT_FOUR_SOURCE_ONLY','base':BASE,'published_base':PUB,'proposed_originals':COMMITS,'collect_now':COMMITS,'candidates':candidates,'unique_paths':sorted(first),
 'planned_source_images':{p:{'baseline':first[p],'final':images[p]}for p in first},'owned_CPP_sources':sorted(first),'changed_headers':[],'new_paths':[],'removed_paths':[],
 'selected_union_native_sources':selected,'selected_requested':14,'unchanged_direct_consumer':CONSUMER,'selected_R8_passes':sum(idx[p]['passed']for p in selected),'selected_R8_failed':sum(not idx[p]['passed']for p in selected),
 'expected_native_inventory':3615,'expected_native_text_inputs':6299,'parent_gitlink_index':parent_index,'reviewed_preflight_sha256':hashlib.sha256(raw).hexdigest(),
 'reuse_requirements':['All14 current native products must pass at the final merged head with showIncludes/pre/post hashes.','Only13 CPP contents may differ from R8; every header/other source and all compiler/tools/external include/submodule context must remain identical.','Prove changed CPPs cannot be included by unaffected products, including literal/reverse graph and nonliteral include context; verify every reused R8 log/object/assembly against immutable audit.','Compose complete3615 coverage from fresh14 and unaffected3601 R8 rows, with explicit per-row origin/build head. Do not claim3615 freshly compiled or run an unnecessary global replay.'],
 'required_gates':['Exact4/13 source-only collection, no aliases/ancestors/headers/build/ledger/index changes. Stop on unexpected preimage/conflict/path expansion.','Actual14-source native compile, all14 pass and all3231 R8 passes retained in final coverage. Preserve native RCE/World/Render/Main/HUD/WorldEntity safeguards and old negatives.','Inspect actual new LineQuery+Fine consumer constant symbols/strict narrow link without FORCE; keep unrelated unresolved failures explicit, no EXE/game-link claim.','No optional owner suites, no later camera/effects15CPP or GameModule/link-input-selection changes.'],
 'known_limits':pre.get('unknowns',[]),'authorization':'Root reviewed fresh preflightbf6f2998, Geometry Frustum/nativeCross and Scene diffs plus both own changes; authorizes exact4 collection and14 actual TUs with reuse only if proven. No full replay/force/publication.',
 'source_changed':False,'compiler_started':False,'formal_review':False,'publication_allowed':False,'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R9_PREFLIGHT_PLAN_01.json',plan)
old=read(R8/'campaign.json');config={'campaign':'wave8-repair-r9','base':BASE,'published_base':PUB,'workspace':str(WORK),'canonical':str(PARENT/'b5-decomp'),'artifact_root':str(HERE),
 'baseline_metadata':str(R8/'native-parallel-wave8-r8-full-stage1-patch.json'),'baseline_report':a['report'],'baseline_artifact_manifest':str(R8/'full-stage1-terminal-audit.json'),
 'published_report':old['published_report'],'published_artifact_manifest':old['published_artifact_manifest'],'baseline_passed':3231,'baseline_failed':384,'baseline_requested':3615,'current_plan':str(HERE/'R9_PREFLIGHT_PLAN_01.json')}
for k in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:config[k+'_sha256']=sha(config[k])
save(HERE/'campaign.json',config);save(HERE/'repair-r9-receipts.json',{'state':'complete','workspace':str(WORK),'head':BASE,'receipts':[],'queue':[],'todo':[]})
save(HERE/'R9_ROOT_AUTHORIZATION_01.json',{'plan_sha256':sha(HERE/'R9_PREFLIGHT_PLAN_01.json'),'authorization':plan['authorization'],'exact_originals':COMMITS,'reviewed_preflight_sha256':hashlib.sha256(raw).hexdigest()})
with(HERE/'R9_PREFLIGHT_SUMMARY_01.md').open('x',encoding='utf-8')as f:f.write('# Authorized R9 source-only plan\n\nFour originals,13 existing CPPs;no headers/new files/inventory changes. Compile the13 plus unchanged FineIntersection consumer,14 actual TUs with showIncludes and final merged R8 headers.\n\nReuse3601 R8 rows only after source/header/tool/external identity,no relevant CPP inclusion and artifact provenance are proved. Complete coverage must be explicitly composite,not a fresh3615-source compile. Preserve3231 R8 passes and every native/layout negative. No optional runtime suites,forced link,publication or later source.\n')
print('PINNED',sha(HERE/'R9_PREFLIGHT_PLAN_01.json'),'4originals 13CPP targeted14','prior_passes',plan['selected_R8_passes'],flush=True)
