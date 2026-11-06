"""Pin R6 finite READ ONLY proposal and actual evidence; no source/build commands."""
from datetime import datetime,timezone
import hashlib,json,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
ART=Path('D:/Reverse/BP-X360-Experimental-Artifacts')
BASE='04ed276fcd79c0d91e5e91db9cd08438c4e0689d'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=WORK)
def save(p,d):
 with Path(p).open('x',encoding='utf-8') as f:json.dump(d,f,indent=2);f.write('\n')
def pin(p):return {'path':str(p),'sha256':sha(p)}
scan=read(HERE/'R6_CANDIDATE_SCAN_02.json');meta=read(HERE.parent/'repair-r5/native-parallel-wave8-r5-full-stage1-patch.json');rawpath=HERE.parent/'repair-r5/full-stage1/compile-results.json';raw=read(rawpath);index={r['source']:r for r in raw['results']};native=set(meta['native_inventory'])-set(scan['removed_paths'])
assert scan['candidate_count']==14 and not scan['first_touch_or_repeated_preimage_conflicts']
PRIMARY=[
 ('XCam',ART/'wave8/xcam-native-terra/DELIVERY.json','a96cb768cba79335b77a458213fe50ed71c0403211c5d83ac8695e2249852503'),
 ('XAudio',ART/'wave8/xaudio-xenon-native-terra/DELIVERY.json','6716f4662fead4d9993370499ce178dae37283ed79232ccaabbadbf3f8b235b8'),
 ('MassiveSocket',ART/'wave8/massive-socket-native-terra/DELIVERY.json','26930595a1061876cb806b8e0ef47954773c4d7fde589c4eb29fc95a671cfebb'),
 ('GPU',ART/'perfmon-gpu-native-r4/DELIVERY.json','9a42f327e956fa335a38ef683dcf734f5bf87fae3a9b2a53210ac98fe83af1f3'),
 ('GPUArtifactManifest',ART/'perfmon-gpu-native-r4/artifact-manifest.json','dee93549513aa6a7b1980cdc3ecfb6bc973ba4cf6095be61f8bf584bbc13073f'),
 ('GPUFrequency',ART/'perfmon-gpu-surface-r4/DELIVERY.json','1d303fafc8f612ab87ebae0aeca2e73ccdf29b6a2045e198e2966d30122d36f3'),
 ('ButterworthFilter',ART/'audio-fma-r3-terra/BUTTERWORTH_FILTER_DELIVERY.json','470bfc1a019ce74a2eea1aee236017b920fd655b13aa41a060d98a91f52d2e91'),
 ('GUIBoot',ART/'bootlegal-final-caller/PRIVATE_DELIVERY.json','a7ff95174fbf3cbd653db745a57846d7591863846e5936b6cf7935a12213eed2'),
 ('GUIWholeEventProvider',ART/'gui-event-provider-final/PROVIDER_DELIVERY.json','2549456c0511b79844f7c6b83bb1e80ac2311ca3aa571f96ccb5b099c0231c0c'),
 ('GUIIntro',ART/'intro-native-log-removal/INTRO_DELIVERY.json','9a3ec0a05ef105d5f7bf783f9db8f3591ded675943187f82f20640b179fa27a7'),
 ('KeyboardTriangle',ART/'wave9/gui_triangle_cxx03_luna_2b4/DELIVERY.json','23280d142407b14a36285361623e75d9a269138d75c01fe0627ce8391512e294'),
 ('ExternalBodyPhysics',ART/'native-external-body-root/DELIVERY.json','7b86928d4e964730daab0faa580120d58a344bc05452adbd95f2e78a5a697c92')]
bindings=[];artifacts={};reports={};hexhash=re.compile(r'^[0-9a-fA-F]{64}$')
def resolve(text,parent):
 if text.replace('\\','/').startswith(('src/','vendor/')):return None
 p=Path(text)
 if p.suffix.lower() not in ['.json','.obj','.py','.h','.hpp','.cpp','.c','.md','.log','.asm','.txt','.inc','.inl']:return None
 if not p.is_absolute():p=parent/p
 return p
def add(path,h,role):
 if path is None:return
 path=Path(path);h=h.lower()
 # Source working copies may have advanced; immutable source is pinned by Git images.
 if str(path).replace('\\','/').lower().startswith(('src/','vendor/')):return
 if not path.is_absolute() or not str(path).replace('\\','/').lower().startswith(str(ART).replace('\\','/').lower()+'/'):return
 key=str(path)
 if key in artifacts:assert artifacts[key]['expected_sha256']==h,(key,role);return
 exists=path.is_file();actual=sha(path) if exists else None
 item={'path':key,'expected_sha256':h,'actual_sha256':actual,'matches':actual==h,'role':role}
 if exists and path.suffix.lower()=='.obj':item['PPC_machine_01F2']=path.read_bytes()[:2]==bytes.fromhex('f201')
 artifacts[key]=item
 if exists and path.name=='compile-results.json':
  d=read(path);rows=d.get('results',[]) if isinstance(d,dict) else d
  reports[key]={'path':key,'sha256':actual,'rows':rows,'role':role}
def walk(d,parent,role):
 if isinstance(d,dict):
  if isinstance(d.get('path'),str) and isinstance(d.get('sha256'),str) and hexhash.fullmatch(d['sha256']):add(resolve(d['path'],parent),d['sha256'],role)
  for k,v in d.items():
   if isinstance(v,str) and hexhash.fullmatch(v):
    if k.endswith('_sha256'):
     other=d.get(k[:-7])
     if isinstance(other,str):add(resolve(other,parent),v,role)
    elif '/' in k or '\\' in k or Path(k).suffix in ['.json','.py','.obj','.md','.log','.asm']:
     add(resolve(k,parent),v,role)
   if isinstance(v,(dict,list)):walk(v,parent,role)
 elif isinstance(d,list):
  for v in d:walk(v,parent,role)
for role,path,expected in PRIMARY:
 actual=sha(path);assert actual==expected,(role,actual,expected)
 bindings.append({'role':role,'path':str(path),'sha256':actual,'expected_sha256':expected,'matches':True})
 walk(read(path),path.parent,role)
# Explicit extra artifact references whose receipts use a filename/hash spelling pair.
for p,h in [
 (ART/'wave9/gui_triangle_cxx03_luna_2b4/triangle_construct-original-vs-native-object.json','2d4fa22846c86d6d8026c91f731de8fb5f0fee0f2a8a3e42fb9c041dff06e05c'),
 (ART/'wave9/gui_triangle_cxx03_luna_2b4/keyboard-api-asm-comparison.json','3145d9f7bb395a8c0bd00850a4ed841df0703a37aef6440a4b1ae6d41474f336')]:add(p,h,'KeyboardTriangle')
bad=[x for x in artifacts.values() if not x['matches'] or x.get('PPC_machine_01F2') is False]
save(HERE/'R6_RECEIPT_BINDINGS_02.json',{'primary':bindings,'immutable_referenced_artifacts':list(artifacts.values()),'mismatches':bad,'scope':'Read-only primary+explicit referenced immutable artifact hash verification. Mutable author source paths are not relabeled current; exact original/prospective Git images are pinned separately. No compile/model execution.'})
assert not bad,bad
# Exact existing prerequisite/alias proof, including prior Debris present by cherry-pick.
r5receipts=read(HERE.parent/'repair-r5/repair-r5-receipts.json')['receipts']
old={r['source_commit']:r for r in r5receipts}
dlc='4de462d593db4983f3c8463290959bbb82eca9b1';private='1beb97e14d50d9402c26b5d59a7ed7f7ec18ae9a'
aliases=[]
for original,alias,kind in [(dlc,private,'GUI private DLC adoption; skip'),*[(r['source_commit'],r['integrated_commit'],'Already-present R5 integration; skip') for r in r5receipts if r['source_commit'].startswith(('4de','6f975','2149'))]]:
 paths=git('diff-tree','--no-commit-id','--name-only','-r',original).decode().splitlines()
 same=all(git('rev-parse',original+':'+p).strip()==git('rev-parse',alias+':'+p).strip()==git('rev-parse',BASE+':'+p).strip() for p in paths)
 assert same
 aliases.append({'original':original,'alias':alias,'classification':kind,'all_changed_path_postimages_equal_current_R5':same,'paths':paths})
save(HERE/'R6_ORIGINAL_ALIAS_MAP_01.json',{'new_originals':[c['commit'] for c in scan['candidates']],'new_nodes_are_originals_not_private_aliases':True,'skip_nodes':aliases,'author_correction':'Triangle owner corrected earlier Debris wording: original2149 is not an ancestor of04ed, but its exact source/behavior delta is present through76ba783. Pair85c1/e50 remains semantically independent; no Debris replay.'})
# Setter warning was historical/conservative: the measured GameModule file is identical.
game='src/GameSource/Game/BrnGameModule.cpp';hdr='src/GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h'
setter=next(c for c in scan['candidates'] if c['group']=='GPUFrequency')
assert all(x['preimage_matches_planned'] for x in setter['paths'])
context={'base':BASE,'GameModule_R5_blob':git('rev-parse',BASE+':'+game).decode().strip(),'GameModule_R4_blob':git('rev-parse','d81ba6aa:'+game).decode().strip(),'setter_parent_blob':git('rev-parse','498c2ae1^:'+game).decode().strip(),'GameModule_planned_postimage':scan['source_image_binding'][game]['planned_postimage'],'GPU_header_planned_postimage':scan['source_image_binding'][hdr]['planned_postimage'],'no_additional_owned_correction_needed':True,'a946_actual_Director_path':'src/GameSource/Director/DirectorModule/BrnDirectorModule.cpp','scope':'The GPU receipt future-R5-union warning is preserved as historical context. Actual R4/R5/setter-parent GameModule blobs equal7fb975; original a946 did not touch this path. Ordinary hunk application preserves the entire current file and all separate R5 typed-void files. No stale file adoption or fabricated conflict.'}
assert context['GameModule_R5_blob']==context['GameModule_R4_blob']==context['setter_parent_blob']
save(HERE/'R6_GPU_CONTEXT_SUPPLEMENT_01.json',context)
# Finite future selected union: all owned surviving CPPs + prior owner products +
# direct includes/API users + native Main/World/replay protection. Lexical only.
post=scan['source_image_binding'];texts={}
for p in native:
 texts[p]=(git('cat-file','blob',post[p]['planned_postimage']).decode(errors='replace') if p in post else (WORK/p).read_text(errors='replace'))
headers=scan['changed_headers'];header_names={Path(p).name:p for p in headers};direct={p:[] for p in headers}
reasons={}
def choose(p,reason):
 if p in native:reasons.setdefault(p,[]).append(reason)
for p in scan['changed_existing_CPP_paths_including_removed']:choose(p,'Owned surviving native CPP')
for path,entry in reports.items():
 for row in entry['rows']:
  if isinstance(row,dict):
   source=row.get('source',row.get('source_rel',''))
   if source in native:choose(source,'Owner product gate '+path)
for p,text in texts.items():
 for token in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)',text,re.M):
  if Path(token).name in header_names:
   h=header_names[Path(token).name];direct[h].append({'source':p,'include':token});choose(p,'Direct lexical include '+h)
 if re.search(r'\b(GuiOverlayRequest|GuiEventPlayVideo|GuiEventStopVideo|PerfMonGpu|MassiveAdClient3Socket|XAudioXenonRenderer|CXCamVideo|CXCamDecoder|CXCamEncoder|CXCamCameraConnector)\b',text):choose(p,'Direct typed API/implementation token user; lexical inventory')
protect_names={'BrnMainX360.cpp','BrnGameMainFlowStates.cpp','BrnWorldModule.cpp','BrnWorldEntityModule.cpp','BrnRaceCarEntityModule.cpp','BrnRaceCarEntityModule_Render.cpp','BrnReplayBaseSerialiser.cpp','BrnReplayRaceCarEntitySerialiser.cpp','BrnReplayTrafficEntitySerialiser.cpp','HighPassButterworth.cpp','LowPassButterworth.cpp','CgsStrStream.cpp','CgsAssert.cpp'}
for p in native:
 if Path(p).name in protect_names:choose(p,'Native Main/World/RCE/replay/actual provider protection')
selected=sorted(reasons);assert set(scan['owned_R5_pass_sources'])<=set(selected)
save(HERE/'R6_SELECTED_CONSUMER_UNION_01.json',{'sources':selected,'requested':len(selected),'reasons':reasons,'direct_changed_header_includes':direct,'baseline_passed':sum(index[p]['passed'] for p in selected),'baseline_failed':sum(not index[p]['passed'] for p in selected),'owned_R5_pass_protected':scan['owned_R5_pass_sources'],'owner_compile_reports':[{'path':p,'sha256':v['sha256'],'role':v['role']} for p,v in reports.items()],'scope':'A proposed future whole-source selection only; no compiler/preprocessor execution. Direct textual include/API references are conservative lexical inventory, not actual include-trace or global ABI evidence. Old author products are attributed to their own snapshots; combined R6 evidence remains pending.'})
limits={role:read(path).get('limits',read(path).get('validation_limits',read(path).get('provenance',{}))) for role,path,_ in PRIMARY if role!='GPUArtifactManifest'}
required=[
 'No collection or compiler until Root reviews exact finite14-original plan; all current6299R5 inputs and clean04ed source unchanged in preflight.',
 'Preserve all original R5 source outside44 named paths. All first-touch/repeated preimages currently match; GPU19→498 header order explicit. GameModule parent=R4=R5, so no invented merge/correction; separate a946/Traffic typedVoid source remains unchanged.',
 'Retire only failed synthetic Boundary CPP after its real legal-state implementation lives in BootLegal; remove exact sole CMake entry. Native inventory3616→3615, native text inputs6299→6298; CMake delta separately hashed. No R5passing source removed.',
 'Compile exact selected native product/consumer union on one coherent future frozen tree; all12 changed R5PASS sources and every other selected R5PASS must retain. Whole EventTypeDefs provider and actual wrappers/callers must compile; do not substitute earlier source slices or prior45-source report.',
 'Fresh typed-header/layout/signature probes: overlay120/align8, real PopupParameter slots; play/stop raw30-byte payload plus channel40 wrappers; BootLegal actual AnimationComponent offsets/state fields; GPU record38/frequency inline; TriangleData by-value v1/v2/v3 constructor. Preserve all38RCE/40World guards, Render616 signature, native Main/MainFlow and independent WorldEntity79E0-vs79F0 C2118 negative.',
 'Bind current-object bounded owner witnesses within exact domains: XCam atomic/DecodeFrame, XAudio Capture/destructor with explicit normalization, Socket controlled services, GPU actual callbacks plus distinct64-case source-island frequency branch, Filter real three-object chain48/48, GUI stage-specific/wrapper/whole-provider cases, Physics selected methods/SetMass/part composition. Reuse models only with data/object/report-path adaptations; preserve all earlier negatives.',
 'TriangleData vertex bytes6/6 is only a raw input-store contract: fullB0 scratch0/6 remains signed-zero negative. XCam extra base-vptr store remains destructor/ABI debt. Filter old R5 28/48 failure remains immutable; only new00af private proof/current combined actual object can claim the bounded48-case fix.',
 'GPU receipt Main=Game/BrnGameModule.cpp, not native entry MainX360: its privateR4 whole failure23 diagnostics and source-island64 result are separate. Recheck actual R5/R6 diagnostics; no layout-guard bypass or whole Main claim from island.',
 'After review/collection/selected stability only, ONE future full3615 native diagnostic6298 inputs. Compare all3105R5passes and3400publishedpasses; exact allowed removed source is failed Boundary only, zero additions. Hash all current/historical artifacts,6298code inputs, separate CMake,compiler/includes/submodules; no formal review/publication while352published regressions remain.'
]
excluded=['All Volume/SAT/LineSeg/GP mutable geometry work','All further GPU/RootPhysics followups beyond19e7/498/fdf','Any GUI extra commit beyond e40 or UI provider followups not in named14','All other audio/DAC/Voice/plugin chains except exactly00af Filter','Any unlisted Main/body/layout/storage/math candidate','Forced unresolved link, fake APIs/stubs/padding or disabled size gates']
for p,h in meta['source_snapshot']['hashes'].items():assert sha(WORK/p)==h,p
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
plan={'state':'PINNED_READ_ONLY_R6_PROPOSAL_FOR_ROOT_REVIEW','base':BASE,'published_base':'3534f7f16103a58ff904bbd1604e18e9934868f4','collectable':False,'collect_now':[],'proposed_originals':[c['commit'] for c in scan['candidates']],'candidate_count':14,'candidates':scan['candidates'],'unique_paths':scan['unique_paths'],'new_paths':[],'removed_paths':scan['removed_paths'],'changed_CPP_including_retired':34,'surviving_changed_CPP':33,'changed_headers':9,'tracked_CMake_files_changed':1,'expected_native_inventory':3615,'expected_native_text_inputs':6298,'baseline_R5_native_inventory':3616,'baseline_R5_inputs_rechecked':6299,'baseline_R5_passes':3105,'all_R5_passing_sources_remain_in_inventory':True,'owned_R5_pass_sources':scan['owned_R5_pass_sources'],'owned_R5_failed_sources':scan['owned_R5_failed_sources'],'retired_R5_failed_sources':scan['retired_R5_failed_sources'],'selected_union_native_sources':selected,'selected_requested':len(selected),'selected_R5_passed':sum(index[p]['passed'] for p in selected),'selected_R5_failed':sum(not index[p]['passed'] for p in selected),'expected_postimage_binding':scan['source_image_binding'],'baseline_binding':pin(HERE/'R6_BASELINE_BINDING_02.json'),'candidate_scan':pin(HERE/'R6_CANDIDATE_SCAN_02.json'),'receipt_binding':pin(HERE/'R6_RECEIPT_BINDINGS_02.json'),'alias_map':pin(HERE/'R6_ORIGINAL_ALIAS_MAP_01.json'),'GPU_context_supplement':pin(HERE/'R6_GPU_CONTEXT_SUPPLEMENT_01.json'),'selected_consumer_union':pin(HERE/'R6_SELECTED_CONSUMER_UNION_01.json'),'R5_raw_report':pin(rawpath),'R5_terminal_audit':pin(HERE.parent/'repair-r5/full-stage1-terminal-audit.json'),'parent_gitlink_index':read(HERE/'R6_BASELINE_BINDING_02.json')['parent_gitlink_index'],'required_gates':required,'owner_limits':limits,'excluded':excluded,'source_changed':False,'compiler_started':False,'formal_review_requested':False,'publication_allowed':False,'goal':'ACTIVE; all native C, real provider/strict link/image/launch not complete','created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/'R6_PREFLIGHT_PLAN_01.json',plan)
summary={k:plan[k] for k in ['state','base','candidate_count','changed_CPP_including_retired','surviving_changed_CPP','changed_headers','expected_native_inventory','expected_native_text_inputs','selected_requested','selected_R5_passed','selected_R5_failed','source_changed','compiler_started']};summary.update(paths=len(scan['unique_paths']),removed_paths=scan['removed_paths'],protected_changed_R5_passes=len(scan['owned_R5_pass_sources']),primary_receipts=len(bindings),referenced_immutable_hashes=len(artifacts),plan_sha256=sha(HERE/'R6_PREFLIGHT_PLAN_01.json'));save(HERE/'R6_PREFLIGHT_SUMMARY_01.json',summary)
text=('# R6 finite read-only preflight\n\n'+f"Base `{BASE}` remains clean and unchanged; canonical stays published3534. No source collection, compile, formal review or publication.\n\n"+f"Exactly14 originals,44 paths:33 surviving changed CPPs,1 failed Boundary CPP retired,9 headers and1 CMake list. No new files. Native inventory3615 and text inputs6298 are prospective; current R5 remains3105PASS/511FAIL/3616. All3105 passes remain in scope;12 changed prior passes are protected.\n\n"+f"Proposed selected union: **{len(selected)} whole native products/consumers** ({summary['selected_R5_passed']} R5PASS/{summary['selected_R5_failed']} R5FAIL). This is a plan, not a new measured compile count. It includes all owned CPPs, available owner product gates, direct changed-header/API users and Main/World/RCE/replay protection.\n\n"+'All14 original preimages fit the measured current/planned predecessor images. GUI DLC private alias1beb is already present through R5 original4de integration; skip it. Debris2149 is likewise present via R5 cherry-pick despite original ancestry being absent. GPUheader19→498 ordering is required. Actual GameModule R4/R5/setter-parent blob is7fb975; its two GPU-frequency calls require no extra merge. a946 changed nested DirectorModule, a separate file retained untouched.\n\n'+f"{len(bindings)} primary manifests and{len(artifacts)} explicit immutable artifact hashes are bound without mismatch. Owner gates are private snapshot evidence. Earlier GUI45-source retention precedes final corrections; the newer whole EventTypeDefs5/5 proof replaces only the old provider-slice limitation, not global retention.\n\n"+'Known limits remain visible: TriangleData vertices6/6 but completeB0 scratch0/6 signed-zero mismatch; XCam additional base-vptr store; controlled XAudio vtable/services and Socket network; GPU frequency64 source-island cases with whole GameModule still historically negative; model/performance/store-scheduling limits in physics; no complete GUI state-machine/boot or GPU/concurrency claim. Filter48/48 fix belongs only to new00af; R5 old28/48 negative remains immutable. WorldEntity79E0/79F0 negative, all38RCE/40World guards and Render/Main protection remain.\n\n'+'After Root review only: collect exactly this finite graph, run selected+named header/ABI/negative checks, then one stable full3615/6298 diagnostic. Expected sole inventory removal is failed Boundary; no pass retirement. No unlisted Volume/SAT/LineSeg, extra GPU/physics, GUI or DAC/audio work.\n\n'+f"Plan SHA256 `{sha(HERE/'R6_PREFLIGHT_PLAN_01.json')}`. All exact source paths, postimages, alias mappings, reports, limits and future gates are in the JSON plan and its pinned supplements.\n")
with (HERE/'R6_PREFLIGHT_REVIEW.md').open('x',encoding='utf-8') as f:f.write(text)
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';owner=read(ownerpath);owner.update(phase='wave8_repair_r6_read_only_preflight_pinned_wait_root_review',frozen_head=BASE,intake_head=BASE,live_session=None,live_process_id=None,campaign_root=str(HERE),plan=str(HERE/'R6_PREFLIGHT_PLAN_01.json'),plan_sha256=sha(HERE/'R6_PREFLIGHT_PLAN_01.json'),preflight_review=str(HERE/'R6_PREFLIGHT_REVIEW.md'),preflight_review_sha256=sha(HERE/'R6_PREFLIGHT_REVIEW.md'),plan_status='READ ONLY exact14 original proposal; Root review required before any collection/compile',source_freeze_status='R5 source remains clean/frozen04ed; R6 no source mutation or build',updated_utc=datetime.now(timezone.utc).isoformat());ownerpath.write_text(json.dumps(owner,indent=2)+'\n')
ownerpath.parent.joinpath('wave5_integrator_sol/CURRENT_OPERATION.md').write_text('# R6 read-only preflight pinned\n\n'+text.split('\n\n',1)[1],encoding='utf-8')
print(json.dumps(summary,indent=2));print('REVIEW_SHA256',sha(HERE/'R6_PREFLIGHT_REVIEW.md'))
