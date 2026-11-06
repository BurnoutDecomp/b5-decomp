"""Pin the exact two-node native-resource/header R11 scope before collection."""
from datetime import datetime,timezone
import hashlib,json,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent;R10=HERE.parent/'repair-r10';PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='1598404ea16b7e16cbe00396bbd051e38b0d59f4';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4';COMMITS=['5ce1523b976dad186e24b0f940c709b7812cc7bd','5d16797854d215767b3de92b9d80580e8ceb3346']
REGISTRATION='src/GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.cpp'
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','HEAD',cwd=PARENT/'b5-decomp').decode().strip()==PUB and not git('status','--porcelain=v1',cwd=PARENT/'b5-decomp').strip()
parent_index='160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp';assert git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==parent_index
p=ART/'wave9/r11-resource-preflight-luna-20261102/preflight.json';raw=p.read_bytes();assert hashlib.sha256(raw).hexdigest()=='19e37e4d3f7ac15ee6c375bf67945ed4332bb0c53d00b655bb0eea8de7375ca0';pre=json.loads(raw)
assert[c['sha']for c in pre['ordered_commits']]==COMMITS
with(HERE/'R11_REVIEWED_PREFLIGHT_01.json').open('xb')as f:f.write(raw)
for ref in pre['evidence_refs']:assert sha(ref['path'])==ref['sha256']
proof=pre['symbols_and_native_evidence']['getparameters_proof'];assert sha(proof['path'])==proof['sha256']=='fed70bdf5dd9edfe78ee9b0cecfd8949124730e819d95e1bbaa4cdca19c85ae1'
tree={}
for record in git('ls-tree','-rz','--full-tree',BASE,'--','src','vendor').split(b'\0'):
 if record:
  info,path=record.split(b'\t',1);tree[path.decode()]=info.decode().split()[2]
images=dict(tree);initial={};candidates=[];receipt_paths=[ART/'wave9/native_render_headers_sol_4277/receipt.json',ART/'wave9/native_render_headers_sol_4277/index-provider-receipt.json']
for i,commit in enumerate(COMMITS):
 changes=[]
 for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',commit).decode().splitlines():
  info,path=line.split('\t',1);_,_,old,new,status=info.split();assert status in ['M','A']and path.startswith('src/')
  assert images.get(path,'0'*40)==old,(commit,path,images.get(path),old)
  initial.setdefault(path,old);images[path]=new;changes.append({'path':path,'preimage':old,'postimage':new,'status':status})
 candidates.append({'commit':commit,'label':'native_headers'if i==0 else'index_provider','paths':changes,'receipt':str(receipt_paths[i]),'receipt_sha256':sha(receipt_paths[i])})
assert len(initial)==10
cpp=sorted(p for p in initial if p.endswith('.cpp'));headers=sorted(set(initial)-set(cpp));newpaths=sorted(p for p,v in initial.items()if v=='0'*40)
assert len(cpp)==7 and len(headers)==3 and len(newpaths)==3 and sum(p.endswith('.cpp')for p in newpaths)==1
a=read(R10/'composite-artifact-audit.json');idx={r['source']:r for r in read(a['report'])['results']};assert idx[REGISTRATION]['passed'];selected=sorted(set(cpp)|{REGISTRATION});assert len(selected)==8
plan={'state':'ROOT_AUTHORIZED_EXACT_TWO_NATIVE_RESOURCE','base':BASE,'published_base':PUB,'proposed_originals':COMMITS,'collect_now':COMMITS,'candidates':candidates,'unique_paths':sorted(initial),
 'planned_source_images':{p:{'baseline':initial[p],'final':images[p]}for p in initial},'owned_CPP_sources':cpp,'changed_headers':headers,'new_paths':newpaths,'removed_paths':[],
 'selected_union_native_sources':selected,'selected_requested':8,'registration_consumer':REGISTRATION,'registration_prior_pass_required':True,
 'selected_R10_passes':sum(idx.get(p,{}).get('passed',False)for p in selected),'selected_R10_failed_or_new':sum(not idx.get(p,{}).get('passed',False)for p in selected),
 'expected_native_inventory':3616,'expected_native_text_inputs':6302,'old_native_inventory':3615,'old_native_text_inputs':6299,'parent_gitlink_index':parent_index,
 'reviewed_preflight_sha256':hashlib.sha256(raw).hexdigest(),'getparameters_private_proof':proof,
 'required_gates':['Exact ordered pair only; validate all10 pre/postimages,including absence of3 new inputs. Stop on path/conflict expansion.','Actual8 native TUs with showIncludes/current merged headers. Preserve registration oldPASS,all7 changed/new CPPs pass before full.','Fresh/rebound native guards and independent negatives; preserve RCE/World/Render/Main shape. MaterialState genuine12-byte pointer view is distinct from original descriptor literalFC. No fake padding/fixup facade.','Bind actual integrated GetParameters symbol/body to calibrated exact48-byte/12-instruction original proof,no relocations. Keep MemoryFlags non-byte-identical and whole-method/source/runtime/link limits explicit.','Only after selected/native/hash/context positive,one fresh3616/6302 fulljobs8; protect all3258 R10passes and retain every failure. Header/global closure uncertain,so no reuse-only final coverage.','No optional link/owner suites,resource/FPUphysics,Director/GUI/Sound/Main42585/6a3a or later source. Prepared tool-host only if ever necessary; no raw SDK utility replay.'],
 'known_limits':pre['unresolved'],'authorization':'Root read/hashverified preflight19e37e4d and actual declarations/Resource&/Raster/Material/IndexBuffer protected-memory operations plus exact48-byte proof; authorizes exact2 collection,8 selected and one stable full. No further permission within scope.',
 'source_changed':False,'compiler_started':False,'formal_review':False,'publication_allowed':False,'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R11_PREFLIGHT_PLAN_01.json',plan)
old=read(R10/'campaign.json');config={'campaign':'wave8-repair-r11','base':BASE,'published_base':PUB,'workspace':str(WORK),'canonical':str(PARENT/'b5-decomp'),'artifact_root':str(HERE),
 'baseline_metadata':str(R10/'native-parallel-wave8-r10-targeted-stage1-patch.json'),'baseline_report':a['report'],'baseline_artifact_manifest':str(R10/'composite-artifact-audit.json'),
 'published_report':old['published_report'],'published_artifact_manifest':old['published_artifact_manifest'],'baseline_passed':3258,'baseline_failed':357,'baseline_requested':3615,'current_plan':str(HERE/'R11_PREFLIGHT_PLAN_01.json')}
for k in ['baseline_metadata','baseline_report','baseline_artifact_manifest','published_report','published_artifact_manifest']:config[k+'_sha256']=sha(config[k])
save(HERE/'campaign.json',config);save(HERE/'repair-r11-receipts.json',{'state':'complete','workspace':str(WORK),'head':BASE,'receipts':[],'queue':[],'todo':[]})
save(HERE/'R11_ROOT_AUTHORIZATION_01.json',{'plan_sha256':sha(HERE/'R11_PREFLIGHT_PLAN_01.json'),'authorization':plan['authorization'],'originals':COMMITS,'preflight_sha256':hashlib.sha256(raw).hexdigest()})
with(HERE/'R11_PREFLIGHT_SUMMARY_01.md').open('x',encoding='utf-8')as f:f.write('# Authorized R11 native-resource plan\n\nExact ordered pair,10 paths:7 CPPs and3 headers. Two genuine headers andone native provider CPP are added. Inventory3615→3616;code inputs6299→6302. All seven existing preimages andthree absent new paths must match.\n\nCompile8 actual selected TUs including unchanged registration;preserve its priorPASS. Rebind native safeguards and the actual integrated GetParameters48-byte body. DescriptorFC is not MaterialState class size;retain genuine12-byte pointer view and all physical-memory/API limits.\n\nAfter positive/stable gates,one fresh3616-source full sweep protectsall3258 R10passes;header/global reuse is not sufficient. No optional link/owner suites,extra source or publication.\n')
print('PINNED',sha(HERE/'R11_PREFLIGHT_PLAN_01.json'),'2originals 10paths selected8 newCPP1 newheaders2',flush=True)
