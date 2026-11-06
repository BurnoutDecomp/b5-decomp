"""Pin root-authorized R1 originals, aliases and evidence before any source mutation."""
from datetime import datetime,timezone
from collections import defaultdict,deque
import hashlib,json,os,re,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
W8=HERE.parent
PARENT=Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow")
WORK=Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANON=PARENT/"b5-decomp"
BASE="c0d955d94ef6d71a3c6e314c4bde7a7439ff697f"
PUBLISHED="3534f7f16103a58ff904bbd1604e18e9934868f4"
def g(*a,root=WORK):return subprocess.check_output(["git","--no-optional-locks","-C",str(root),*a])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding="utf-8-sig"))
def save(n,d):
 with (HERE/n).open("x",encoding="utf-8",newline="\n") as f:f.write(json.dumps(d,indent=2)+"\n")
def tree(c):return {r.split("\t",1)[1]:r.split()[2] for r in g("ls-tree","-r",c).decode().splitlines()}
def full(c):return g("rev-parse",c+"^{commit}").decode().strip()
def pid(c):return subprocess.check_output(["git","patch-id","--stable"],input=g("show","--format=",c)).decode().split()[0]
assert g("rev-parse","HEAD").decode().strip()==BASE and not g("status","--porcelain=v1").strip()
assert not g("branch","--show-current").strip()
assert g("rev-parse","HEAD",root=CANON).decode().strip()==PUBLISHED and not g("status","--porcelain=v1",root=CANON).strip()
meta=read(W8/"native-parallel-wave8-full-stage1-patch.json")
assert len(meta["source_snapshot"]["hashes"])==6287
assert all(sha(WORK/p)==h for p,h in meta["source_snapshot"]["hashes"].items())
assert sha(W8/"full-stage1-terminal-audit.json")=="8a9fb09544dcfd3b284f01ef89b55a22218bbdf3c7a3b64b9e5d7418cdac214c"
assert sha(W8/"full-stage1/compile-results.json")=="8ef65e27c7f51e23f2801c05493510ff40df4303fa45e41742a58b332e366a8b"
groups=read(HERE/"named-groups.json");commits=[c for cs in groups.values() for c in cs];assert len(commits)==len(set(commits))==29
base_tree=tree(BASE);first={};rows=[];paths=set();adds=set();final={}
for group,cs in groups.items():
 for c in cs:
  assert full(c)==c
  parent=g("rev-parse",c+"^").decode().strip();pt=tree(parent);ct=tree(c)
  changes=[r.split("\t",1) for r in g("diff-tree","--no-commit-id","--name-status","-r",c).decode().splitlines()]
  assert all(p.startswith(("src/","vendor/")) and "pc" not in [s.lower() for s in Path(p).parts] and status!="D" for status,p in changes)
  for status,p in changes:
   first.setdefault(p,{"commit":c,"parent_blob":pt.get(p),"current_base_blob":base_tree.get(p)})
   paths.add(p);final[p]=c
   if status=="A":adds.add(p)
  patch=g("show","--format=","--binary","--full-index",c)
  rows.append({"group":group,"commit":c,"parent":parent,"paths":[p for s,p in changes],"changes":changes,"message":g("show","-s","--format=%B",c).decode(),"patch_sha256":hashlib.sha256(patch).hexdigest(),"stable_patch_id":pid(c)})
assert len(paths)==75 and len(adds)==6 and not any(p.endswith((".cpp",".c")) for p in adds)
links={
 "3a8f4929881e4d80795cc202c9142d976533dbc5":["509e8543e","1ea3726895334bcb294c75f0201cc9a7ed3ad93a","631d14aac3b1ef28bf337c93224e1f69ffb28e24","ed703bcc4c99e11fea11f238d9b93816ee57b718","84704e1f4"],
 "9c5f8b9ab22179f30f514d949ce765c3f53148ba":["02ea2bed6","b935679ebda985402e32dba6e41bf62d8aaf8490","66543b45e5d7e9ef28e812ebd2268e453a5c4439","64d4afcb7bbb2c59c5a73b4a6a5910c5e3479275","225bb8637"],
 "2d9703305791847280047cfc4ba57dcf987fb24c":["52dd35f43"],
 "99a7d60742b8987002c15c8280327eb25a7a52a5":["2e049aadd7afbbe0ad62f84df07c1fe4c9022716"],
 "4c5c03ecbe757e18b0b1dd251fb6e880e79d394b":["9cad5c3e74366a92969f061d97a2475a6a6ae259","f0803dd82"],
 "a0656568742b2332c23323b19a530039b24c472a":["48bb5070aa47d52d26024abfab82c313ecfacd03","443a1dd4a"],
 "6ad868d7c20539cbe299ddfd74befa505fe180a1":["d431211f0"]}
aliases=[]
for original,als in links.items():
 op=pid(original)
 for alias in als:
  alias=full(alias);ap=pid(alias);aliases.append({"original":original,"local":alias,"same_patch_id":op==ap,"original_patch_id":op,"local_patch_id":ap,"collect_local":False})
receipts={
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/consumer_wave8_v1/CONSUMER_AUTHOR_CHECKPOINT.json":"31083867f8add16327107fd2e9aed397f46de7f5ed89d11dfb6de4c32d0d3c91",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/consumer_wave8_v1/MASK_SCALAR_PRIVATE_CHECKPOINT.json":"081c27aea7466e46d9557f2fb953e2aa141d03d42ad56456d0d904ae225e5879",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/consumer_wave8_v1/SOURCE_RECIPE_SNAPSHOTS_V1.json":"96623d07e169f95a3e2f949d450c9a949e371e18999a2d3c36ec4c4965669232",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/consumer_wave8_v1/SHADER_SCALAR_DEBT_ROUTING_V1.json":"c9ccc17090c25a5f1dab67bcf4be35274bf6d6108524041b7e87d0e72f12e164",
 W8.parent/"parallel/wave8_scene_terra/geometry-mask-closure/RECEIPT.json":"082b8942694a4938f0d964c8e5048eda797fba6d7f6971861a0a2956232d1d7d",
 W8.parent/"gamestate-gui-c0d-terra/receipt.json":"fb3fba06369683eb37819ea1304adbdc710107deb64e9cd55a870651093265c5",
 W8/"racecar_consumers/wave8-final-batch-771505/CHECKPOINT.json":"3d85927ae276d81d080ce2673b3022e75bba477b5ca0fd0149ced5defead7986",
 W8/"debugui_std_luna_c0d955/DELIVERY.md":"23050bd070ef216d71ce9c1a022214300c08efae75d8fe77e3739d0c3c85d1f4",
 W8.parent/"native-brnpostfx-root/DELIVERY.json":"b29bed0b78400336d464f0e2b55787e2b6b6ecaf509b642bf06290bb75478282",
 W8.parent/"native-brnpostfx-root/c0d-rebind/C0D_REBIND_CHECKPOINT.json":"c32863b7f5485a5752322ab1dc77af697d3f3da937c3498d1328e91dfe9110ca"}
for p,h in receipts.items():assert sha(p)==h,(str(p),sha(p),h)
# Conservative old-graph closure is a preflight reach estimate, never actual compile retention.
rev=defaultdict(set)
for p,ds in meta["source_snapshot"]["dependencies"].items():
 for d in ds:rev[d].add(p)
seen=set(paths);q=deque(seen)
while q:
 for p in rev[q.popleft()]:
  if p not in seen:seen.add(p);q.append(p)
closure=sorted(set(meta["native_inventory"])&seen)
owned_cpp=sorted(p for p in paths if p.endswith((".cpp",".c")))
plan={"campaign":"wave8-repair-r1","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"groups":groups,"named_originals":commits,"collect_now":commits,"candidates":rows,"candidate_paths":sorted(paths),"new_paths":sorted(adds),"first_touch_bindings":first,"first_touch_divergences":{p:v for p,v in first.items() if v["parent_blob"]!=v["current_base_blob"] and p not in adds},"final_source_commit_by_path":final,"aliases":aliases,"manifest_bindings":{str(p):h for p,h in receipts.items()},"expected_native_inventory":3616,"expected_native_text_inputs":6293,"owned_cpp_sources":owned_cpp,"preflight_old_graph_closure":closure,"preflight_closure_note":"Conservative c0d literal/basename graph before six new headers; not actual include trace, not a new global result. Actual postcollection graph/selected consumers are required.","required_gates":["Root authorized finite29 originals and these exact prerequisites only; aliases once, no stale final-file adoption.","Freeze exact actual source/patch/input hashes after collection; compile every ownedCPP plus actual header/provider/caller sentinels across all six combined cohorts.","Shared globalVecFloat/MaskScalar broad API changes require one fresh complete3616source diagnostic after selected source union stable; no narrowed-subset or overlay called freshfull.","Actual result must compare both c0d2812pass snapshot and published3534's3400passes; no canonical publication/formalreview until true retention makes review meaningful."],"retained_debt":["Debug AddVertex999/1000 threshold, scalar/trig/culling/dispatch unchanged; Sound graph clamp/assert/endpoints/generalFMA unproved.","World Sphere13 direct baseline callers remain failed; Box finitecardinal comparisons/scalarDot rounding and NaN/order limits, Loose allocation/layout uncertainty retained.","Director GameState nearestjunkyard scalar sqrt vsretailVMX refinement remains; axis-aligned nontie tests only, APT VM notproven.","Racecar raw particle-row helper uses original-data contract, not directARTISTexecution; scalarwater/frustum/UV/fold/matrix and PCdiagnostics retained, hardware/GPU unproved.","Luna oldVectorFontCPP stillfails and isnotchanged; twoCPPfix plus other assigned consumers/provider gates only.","PostFx11/12 c0d privatebatch retains EnvironmentManager70diagnostics, its CPP unmodified; Mask/World union not privately tested, nextactualcombinedgate required."],"excluded":["entire in-flight audio/DAC/Voice chain","newCamera/Rig/laterDirector work","ShaderConstantTable unusedScalar overload debt (no nameddirectconsumer)","RootEnvironmentManagerCPP WIP","all other unsealed work"],"parent_gitlink_index":g("ls-files","--stage","b5-decomp",root=PARENT).decode().strip(),"authorization":"Root explicitly authorized this finiteR1 original set/gates/onefull after exact plan pinned. No canonical publication or formalreview yet.","created_utc":datetime.now(timezone.utc).isoformat()}
save("intake-plan-01.json",plan)
config={"campaign":"wave8-repair-r1","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"canonical":str(CANON),"artifact_root":str(HERE),"baseline_metadata":str(W8/"native-parallel-wave8-full-stage1-patch.json"),"baseline_report":str(W8/"full-stage1/compile-results.json"),"baseline_artifact_manifest":str(W8/"full-stage1-terminal-audit.json"),"published_report":str(W8.parent/"wave7/full-successor1/compile-results.json"),"published_artifact_manifest":str(W8.parent/"wave7/native-parallel-wave7-final-artifact-manifest.json"),"baseline_passed":2812,"baseline_failed":804,"baseline_requested":3616,"current_plan":str(HERE/"intake-plan-01.json")}
for key in ("baseline_metadata","baseline_report","baseline_artifact_manifest","published_report","published_artifact_manifest"):config[key+"_sha256"]=sha(config[key])
save("campaign.json",config);save("repair-r1-receipts.json",{"workspace":str(WORK),"base":BASE,"head":BASE,"state":"complete","queue":[],"todo":[],"receipts":[]})
save("plan-freeze.json",{"plan_sha256":sha(HERE/"intake-plan-01.json"),"base":BASE,"originals":29,"paths":len(paths),"owned_cpp":len(owned_cpp),"new_headers":len(adds),"expected_inventory":3616,"expected_input_hashes":6293,"conservative_old_graph_closure":len(closure),"source_unchanged_at_freeze":True})
print(json.dumps(read(HERE/"plan-freeze.json"),indent=2));print("FIRST TOUCH DIVERGENCES",list(plan["first_touch_divergences"]));print("NONEXACT ALIASES",[(r["original"],r["local"]) for r in aliases if not r["same_patch_id"]])
