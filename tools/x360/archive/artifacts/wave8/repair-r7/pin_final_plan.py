"""Pin finite R7 review proposal; no repository writes or compiler execution."""
from collections import defaultdict,deque
from datetime import datetime,timezone
import hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');ART=HERE.parent.parent;BASE='ef64014612b510bffa6fa17bebe91461bd95a0ab'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd).decode().strip()
def save(p,d):
 with Path(p).open('x',encoding='utf-8')as f:json.dump(d,f,indent=2);f.write('\n')
scan=read(HERE/'R7_FINAL_SOURCE_SCAN_01.json');assert not scan['preimage_conflicts'] and len(scan['proposed_originals'])==20
assert git('rev-parse','HEAD')==BASE and not git('status','--porcelain=v1')
assert git('rev-parse','HEAD',cwd=PARENT/'b5-decomp')=='3534f7f16103a58ff904bbd1604e18e9934868f4'and not git('status','--porcelain=v1',cwd=PARENT/'b5-decomp')
worldpath=ART/'parallel/wave8_scene_terra/volume-baseline/CANDIDATE_DELIVERY.json';guipath=ART/'race-hud-native-root/DELIVERY.json'
assert sha(worldpath)=='b52246c70cf49eec68b8f8561f9bfa0d018c30b3e6b5099505e27e3c61b13b33'and sha(guipath)=='886aff3ea4ddc8d1db6488de21e45f8be9cb125095a962f77fbbd0c6829eb8f0'
world=read(worldpath);gui=read(guipath);status=read(HERE/'R7_TARGETED_STATUS_01.json')
primary=list(status['immutable_primary_pins'])
for row in primary:assert sha(row['path'])==row['sha256']
primary+=[{'role':'World55_final','path':str(worldpath),'sha256':sha(worldpath)},{'role':'RootGUI10_final','path':str(guipath),'sha256':sha(guipath)}]
for appendix in ['R7_RECEIVED_SEALS_APPENDIX_01.json','R7_RECEIVED_SEALS_APPENDIX_02.json']:
 d=read(HERE/appendix)
 if 'receipt'in d:assert sha(d['receipt'])==d['receipt_sha256'];primary.append({'role':'Fresh_native_lane_rebind','path':d['receipt'],'sha256':d['receipt_sha256']})
 for x in d.get('pins',[]):assert sha(x['path'])==x['sha256'];primary.append(x)
hashre=re.compile(r'^[0-9a-fA-F]{64}$');artifacts={}
def add(path,h):
 p=Path(path);h=h.lower()
 if not p.is_absolute()or not str(p).replace('\\','/').lower().startswith(str(ART).replace('\\','/').lower()+'/'):return
 if str(p)in artifacts:assert artifacts[str(p)]==h;return
 assert p.is_file(),str(p);assert sha(p)==h,str(p);artifacts[str(p)]=h
def walk(d,parent):
 if isinstance(d,dict):
  if isinstance(d.get('path'),str)and isinstance(d.get('sha256'),str)and hashre.fullmatch(d['sha256']):add(d['path'],d['sha256'])
  for k,v in d.items():
   if isinstance(v,str)and hashre.fullmatch(v):
    if k.endswith('_sha256')and isinstance(d.get(k[:-7]),str):
     text=d[k[:-7]]
     if Path(text).is_absolute():add(text,v)
    elif Path(k).is_absolute():add(k,v)
   elif isinstance(v,(dict,list)):walk(v,parent)
 elif isinstance(d,list):
  for v in d:walk(v,parent)
walk(world,worldpath.parent);walk(gui,guipath.parent)
save(HERE/'R7_FINAL_EVIDENCE_BINDINGS_01.json',{'primary':primary,'new_World_GUI_explicit_artifact_hashes':artifacts,'source':'Recorded private source/objects stay attributed to those snapshots. No compile or current-object rerun performed in preflight.'})
# Localize already-present R6 header drift; source prerequisites are not new picks.
drift_names={'CgsPerfMonGpu.h','CgsVectorFont.h','RwMathVectorTemplates.h','Curves.h','BrnCoronaManager.h','BrnGuiEventTypeDefs.h','ExternalPhysicsBody.h','Engine.h','BrnReplayBaseSerialiser.h','BrnReplayPropEntitySerialiser.h','programbuffer.h','types.h','rwcore_structs.h'}
driftpaths=[p for p in git('diff','--name-only',world['parent'],BASE,'--','*.h','*.hpp').splitlines()if Path(p).name in drift_names]
assert len(driftpaths)==13
drift=[{'path':p,'private_parent_blob':git('rev-parse',world['parent']+':'+p),'R6_blob':git('rev-parse',BASE+':'+p)}for p in driftpaths]
save(HERE/'R7_PREREQUISITE_INPUT_CONTEXT_01.json',{'base':BASE,'World_minimal_source_graph':[BASE,'340a698e5cfa823c1bc48579530ebc966d15219a'],'World_parent_is_private_ancestry_not_collectable':world['parent'],'all55_direct_preimages_match_R6':True,'private_compile_rebind_required':True,'recorded_project_header_drift13':drift,'input_drift_is_not_source_conflict_or_missing_prerequisite':True,'Matrix_rule':'48b937 already published,7d96 private alias not replayed.3 Matrix blobs exact;types.h later genuine Vector2 constructor preserves Matrix union. Never overwrite types.h.','Root_GUI_graph':['a3633b1049ee5a5e66376dcd7a503025886a2fa9','75bbef15366ccfcffbcb0f06f7a1fc7d58a2bb7b','7a3b9fe6178b95534973434dd034b7496f05b14c'],'Root_GUI_private_aliases_skip':gui['prerequisite_aliases'],'extra_private_ancestor_commits_to_collect':[],'scope':'R6 integrated compile must replace private-parent-only36/36 attribution. No assumption of regression from header drift.'})
meta=read(HERE.parent/'repair-r6/native-parallel-wave8-r6-full-stage1-patch.json');baseline=read(HERE.parent/'repair-r6/full-stage1/compile-results.json');index={r['source']:r for r in baseline['results']};native=set(meta['native_inventory']);dependencies=meta['source_snapshot']['dependencies'];changed=set(scan['changed_headers']);reasons=defaultdict(list)
def choose(p,why):
 if p in native:reasons[p].append(why)
for p in scan['owned_CPP_sources']:choose(p,'Owned changed CPP')
for p,children in dependencies.items():
 if p in native:
  for h in changed.intersection(children):choose(p,'Direct stored R6 include edge to '+h)
worldgate=read(ART/'parallel/wave8_scene_terra/volume-baseline/FINAL_COHERENT_GATE.json')
for r in worldgate['rows']:choose(r['source'],'World whole36 provider/direct-caller gate; must rebuild on R6 inputs')
for p in gui['compile']['final_rows']:choose(p,'Root GUI whole12 provider/caller gate; historical StuntBridge fail remains visible')
for p in native:
 if Path(p).name in ['BrnMainX360.cpp','BrnGameMainFlowStates.cpp','BrnGameModule.cpp','BrnWorldModule.cpp','BrnWorldEntityModule.cpp','BrnRaceCarEntityModule.cpp','BrnRaceCarEntityModule_Render.cpp','BrnReplayBaseSerialiser.cpp','BrnReplayEffectsSerialiser.cpp','BrnReplayEffectsSerialiserStaticLayout.cpp','BrnReplayRaceCarEntitySerialiser.cpp','BrnReplayTrafficEntitySerialiser.cpp']:choose(p,'Native Main/World/RCE/typed replay protection')
selected=sorted(reasons);assert set(scan['owned_R6_pass_sources'])<=set(selected)
reverse=defaultdict(set)
for p,children in dependencies.items():
 for h in children:reverse[h].add(p)
pending=deque(changed);affected=set(changed)
while pending:
 for p in reverse[pending.popleft()]:
  if p not in affected:affected.add(p);pending.append(p)
save(HERE/'R7_SELECTED_CONSUMER_UNION_01.json',{'sources':selected,'requested':len(selected),'R6_passed':sum(index[p]['passed']for p in selected),'R6_failed':sum(not index[p]['passed']for p in selected),'reasons':dict(reasons),'owned_R6_pass_protected':scan['owned_R6_pass_sources'],'stored_R6_header_transitive_native_closure_count':len(native&affected),'scope':'Future exact whole-source gate proposal only. Stored literal include graph plus actual owner product/direct-caller sets; not a preprocessor trace or new compile result. New Mugshot producer/cache/HUD paths are explicitly owned/included. Full native sweep still required for wider fanout.'})
required=[
 'Root review of this exact20-original finite graph before any source collection/compile. Reuse stored16+2 targeted scans; do not replay private ancestry or add later StuntBridge/CPP followups.',
 'Preserve all107 planned postimages surgically against R6; current preimage conflicts are zero. Only new file is genuine BrnGuiMugshotControlEvent.h; no new/retired CPP. Native inventory3615/code inputs6299 prospective, current R6 result unchanged.',
 'Compile exact selected provider/caller union on stable integrated tree; protect all30 changed R6PASS CPPs and every selected baseline R6PASS. Rebuild all World36 despite private-parent36/36, because13 included headers differ already in R6. Never relabel private objects as R6-integrated evidence.',
 'Fresh genuine typed/header/layout gates: Camera full64-bit state at+140 and BitArray head; EffectsModule/EffectsSerialiser fields plus R5 void Construct/Base18/1C semantics; Volume/GP/query/member-pointer shapes; CacheSphere native30/al16 offsets0/10/24/28/2c; Mesh known prefix/pointers+30/+34 only. No fabricated full mesh size/tail or parent-cache extent.',
 'Keep all38RCE/40World/Render signature/nativeMain/MainFlow protections and independent WorldEntity79E0vs79F0 negative. Cache natural alignment16 is new integrated input; remeasure actual parent containment and do not pad or disable guards to preserve a pass.',
 'Bind exact current-object owner comparisons within their original domains: native vector/ctor zero lanes, real current ModeManager params flag, sign-in XDK compile/listing contracts,64-bit camera flags/ICE gameplay copy, effects VMX/unordered-NaN/RNG/junkyard/replay paths, World finite collision/line/GP/SAT/quartic/fine-query bounds, GUI three mugshot methods324 +Road264. No overall gameplay/whole-runtime extrapolation.',
 'New raw325 payload/cache/member/road-sign18 ABI guards must pass; final HUD sizeof7920 vs original allocation7940 remains an expected independent negative, never padded/aligned away. Root GUI raw COFF timestamp-only equality failure stays preserved alongside separate874 runtime-section/data/relocation match.',
 'EventInfo originala363→75bb exactly once, skip918541/12aa aliases; authenticate six real CRT thunk results50b5437b, never infer values from zero BSS. f3 EffectsDebug follows25ea header/module shape. Preserve old placeholder/BSS-negative and author limits.',
 'Preserve all World negatives/limits: quartic old58/64 and corrected decoder64/64 attributed separately;192 torus composed and32 fat-cylinder cases only2 entering quartic; incomplete ClusteredMesh populated hits/full extent; unvalidated cache event arithmetic; Scene helper-vs-inline call shape; unresolved Prop82FAD600 initializer; software-estimate/nonfinite/hardware/allocation/runtime limits.',
 'Preserve evidence corrections: f917 explicit final7/7 hash2bbdb0ce rather than duplicate-basename receipt index; native-lane fresh ef640 rebindf46a4bf6 with old incomplete/discovery preserved; d883 compile/object-only and later fa6/c038 scoped proofs attributed separately. No retrospective source hash or fake full behavior seal.',
 'After authorized collection/selected/named gates and stable source only, ONE full3615/6299 diagnostic comparing all3122R6passes and3400publishedpasses, no omitted failures. Bind all source/artifact/include/compiler/submodule inputs, actual exits and raw/historical hashes. No formal review/publication until legitimate retained-pass and whole-goal gates permit.'
]
limits={'World':world['limits']+world['extra_limits'],'RootGUI':gui['limits'],'Other_ready_evidence':status['metadata_corrections'],'Remaining_goal':'R6 still3122PASS/493FAIL;347published regressions. Header/compile witnesses do not prove full hardware ABI, strict link,image,boot or game.'}
plan={'state':'PINNED_FINITE_R7_PROPOSAL_FOR_ROOT_REVIEW','base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','collectable':False,'collect_now':[],'proposed_originals':scan['proposed_originals'],'candidate_count':20,'candidates':scan['candidates'],'unique_paths':scan['unique_paths'],'new_paths':scan['new_paths'],'removed_paths':[],'owned_CPP_sources':scan['owned_CPP_sources'],'changed_CPP':76,'changed_headers':31,'expected_native_inventory':3615,'expected_native_text_inputs':6299,'owned_R6_pass_sources':scan['owned_R6_pass_sources'],'owned_R6_failed_sources':scan['owned_R6_failed_sources'],'planned_source_images':scan['planned_source_images'],'preimage_conflicts':[],'selected_union_native_sources':selected,'selected_requested':len(selected),'selected_R6_passed':sum(index[p]['passed']for p in selected),'selected_R6_failed':sum(not index[p]['passed']for p in selected),'original_alias_map':scan['aliases'],'source_scan':pin(HERE/'R7_FINAL_SOURCE_SCAN_01.json'),'prerequisite_context':pin(HERE/'R7_PREREQUISITE_INPUT_CONTEXT_01.json'),'evidence_bindings':pin(HERE/'R7_FINAL_EVIDENCE_BINDINGS_01.json'),'consumer_union':pin(HERE/'R7_SELECTED_CONSUMER_UNION_01.json'),'read_only_status_history':[pin(HERE/n)for n in ['R7_READY_SCAN_01.json','R7_TARGETED_STATUS_01.json','R7_RECEIVED_SEALS_APPENDIX_01.json','R7_RECEIVED_SEALS_APPENDIX_02.json']],'baseline_metadata':pin(HERE.parent/'repair-r6/native-parallel-wave8-r6-full-stage1-patch.json'),'baseline_report':pin(HERE.parent/'repair-r6/full-stage1/compile-results.json'),'baseline_audit':pin(HERE.parent/'repair-r6/full-stage1-terminal-audit.json'),'parent_gitlink_index':git('ls-files','--stage','b5-decomp',cwd=PARENT),'required_gates':required,'retained_limits':limits,'excluded':['Any later Root GameBridgeGameStateToX_StuntGuiEvents.cpp successor','Any later Luna18CPP/five-CPP/source cleanup batch not among20','Unlisted World/GUI/model/provider changes or private ancestry','All DAC/Voice/audio/storage/MSAA/private native-layout work outside this exact graph','Forced unresolved link/fake APIs/padding/stubs/disabled guards/production ledger changes'],'source_changed':False,'compiler_started':False,'formal_review_requested':False,'publication_allowed':False,'goal':'ACTIVE; all native C/provider/strictlink/image/launch incomplete','created_utc':datetime.now(timezone.utc).isoformat()}
assert git('rev-parse','HEAD')==BASE and not git('status','--porcelain=v1');save(HERE/'R7_PREFLIGHT_PLAN_01.json',plan)
summary={'state':plan['state'],'base':BASE,'originals':20,'paths':107,'CPPs':76,'headers':31,'new_header':scan['new_paths'],'native_inventory_prospective':3615,'code_inputs_prospective':6299,'preimage_conflicts':0,'changed_R6_pass_protected':30,'selected_requested':len(selected),'selected_R6_passed':plan['selected_R6_passed'],'selected_R6_failed':plan['selected_R6_failed'],'World_private_parent_input_drift':13,'private_ancestor_picks':0,'pending_unsealed_in_this_cutoff':0,'source_changed':False,'compiler_started':False,'plan_sha256':sha(HERE/'R7_PREFLIGHT_PLAN_01.json')};save(HERE/'R7_PREFLIGHT_SUMMARY_01.json',summary)
text=('# R7 finite preflight for Root review\n\n'+f"Clean frozen base `{BASE}`; canonical remains published3534. **No source collection or compiler started.**\n\n"+'Exact cutoff: **20 originals /107 paths:76 CPPs,31 headers, one new genuine Mugshot header.** No new/retired native CPPs. Prospective inventory3615 and code inputs6299. All current/planned preimages match, including new-node ordering; thirty changed CPPs already pass in R6 and must retain. No global gain is predicted.\n\n'+f"Proposed selected union: **{len(selected)} whole native sources** ({plan['selected_R6_passed']} R6PASS/{plan['selected_R6_failed']} R6FAIL). It combines all76 owned CPPs, all World36 and RootGUI12 direct/provider cases, direct changed-header users, and nativeMain/World/RCE/replay protections. The existing transitive header closure remains for the later full sweep.\n\n"+'Dependency rule: ready18 original source nodes first; World340a and RootGUI7a are the only final additions. World55 and RootGUI10 paths are disjoint from each other and ready18. World parent7d96/private history is not collected; Matrix48 is already present with the later genuine Vector2 types.h union retained. Root GUI requires originala363→75bb once; skip private918541/12aa. EventInfo CPP belongs to those predecessors, not Root10.\n\n'+'Important rebind: World author36/36 is private-parent evidence. Thirteen included headers differ already in R6; all World products must rebuild on the integrated current tree. There is no missing Matrix/private prerequisite to replay. Root GUI11/12 private report keeps historical StuntBridge failure; its new later repair is excluded.\n\n'+'Named safeguards: preserve38RCE/40World/Render/Main and WorldEntity79E0-vs79F0 negative; measure Camera64-bit flags, Effects/replay type shapes, CacheSphere30/al16 and known Mesh prefix only, raw325 payload/cache/road-sign guards. Keep HUD7920-vs7940 allocation negative, unresolved Prop82FAD600, Scene helper call-shape differences, incomplete cache arithmetic and ClusteredMesh extent/populated path. Never add guessed padding/alignment or disable a gate.\n\n'+'Evidence scope: authenticate six real EventInfo CRT thunks (not BSS zeros); retain raw timestamp-only COFF failure plus separate874 runtime-record match. Keep quartic old58/64 negative and decoder correction distinct. f917 final7/7 index correction, fresh native-lane4/4 source/header rebind, and ICE compile-vs-scoped-behavior attribution are pinned. Owner software-model/service/nonfinite/hardware/runtime limits remain, not whole gameplay or ABI proof.\n\n'+'After Root approval only: exact20-node collection, selected union and named current-object/header/negative gates, then one stable full3615/6299 diagnostic protecting all3122R6 passes and comparing3400published passes. R6 still3122PASS/493FAIL with347published regressions. No formal review/publication or link/image/launch claim.\n\n'+f"Plan SHA256 `{sha(HERE/'R7_PREFLIGHT_PLAN_01.json')}`. The JSON pins exact original IDs,107 postimages, source/alias/input-drift/evidence/consumer supplements and every required gate.\n")
with(HERE/'R7_PREFLIGHT_REVIEW.md').open('x',encoding='utf-8')as f:f.write(text)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath);o.update(phase='wave8_repair_r7_final_read_only_plan_pinned_root_review',frozen_head=BASE,intake_head=BASE,live_session=None,live_process_id=None,campaign_root=str(HERE),plan=str(HERE/'R7_PREFLIGHT_PLAN_01.json'),plan_sha256=sha(HERE/'R7_PREFLIGHT_PLAN_01.json'),preflight_review=str(HERE/'R7_PREFLIGHT_REVIEW.md'),preflight_review_sha256=sha(HERE/'R7_PREFLIGHT_REVIEW.md'),plan_status='Exact20 originals/107paths; allseals present; Root review required before any collection/compile',source_freeze_status='R6 remains clean frozenef640; R7 has no source mutation or compiler',updated_utc=datetime.now(timezone.utc).isoformat());ownerpath.write_text(json.dumps(o,indent=2)+'\n');ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text('# R7 final read-only plan pending Root review\n\n'+text.split('\n\n',1)[1],encoding='utf-8')
print(json.dumps(summary,indent=2));print('REVIEW_SHA256',sha(HERE/'R7_PREFLIGHT_REVIEW.md'))
