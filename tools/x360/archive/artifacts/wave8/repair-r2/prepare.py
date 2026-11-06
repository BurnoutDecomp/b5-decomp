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
BASE="a26ca20f812a737975e02d92d12156958a50866b"
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
meta=read(W8/"repair-r1/native-parallel-wave8-r1-full-stage1-patch.json")
assert len(meta["source_snapshot"]["hashes"])==6293
assert all(sha(WORK/p)==h for p,h in meta["source_snapshot"]["hashes"].items())
assert sha(W8/"repair-r1/full-stage1-terminal-audit.json")=="986aff3039df95593f63ebbace46455682b97a6e03abf782bca9650757d9a508"
assert sha(W8/"repair-r1/full-stage1/compile-results.json")=="1e96dd29b2d4c4ee1abc75e5f985c4124a9ec03fce32abcf3ee8cc356c754ec5"
groups=read(HERE/"named-groups.json");commits=[c for cs in groups.values() for c in cs];assert len(commits)==len(set(commits))==28
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
assert len(paths)==48 and len(adds)==0 and not any(p.endswith((".cpp",".c")) for p in adds)
links={
 "3a8f4929881e4d80795cc202c9142d976533dbc5":["631d14aac3b1ef28bf337c93224e1f69ffb28e24","ed703bcc4c99e11fea11f238d9b93816ee57b718","1ea3726895334bcb294c75f0201cc9a7ed3ad93a"],
 "9c5f8b9ab22179f30f514d949ce765c3f53148ba":["66543b45e5d7e9ef28e812ebd2268e453a5c4439","64d4afcb7bbb2c59c5a73b4a6a5910c5e3479275","b935679ebda985402e32dba6e41bf62d8aaf8490"],
 "1ec4f084653a13e9014b91f9c9bff1072cb8c389":["ecc10557634e996d7a9a731542f5ff5fa1b23e5b"],
 "5a72a305382398b4c01c2f4f698c785281dbbf28":["3f511f2b42bf8a5b358387eb63fc3e75a5eaa406"],
 "99a7d60742b8987002c15c8280327eb25a7a52a5":["2e049aadd7afbbe0ad62f84df07c1fe4c9022716"]}
aliases=[]
for original,als in links.items():
 op=pid(original)
 for alias in als:
  alias=full(alias);ap=pid(alias);aliases.append({"original":original,"local":alias,"same_patch_id":op==ap,"original_patch_id":op,"local_patch_id":ap,"collect_local":False})
receipts={
 W8.parent/"native-environment-root/DELIVERY.json":"6d2362baaf4656589cdaee7939ec26f400dce00cf8ff89979eb08b1200721ce1",
 W8.parent/"native-loading-screen-root/DELIVERY.json":"bdf271a9e5f6b6cc8aa087d1e8439d7fd3c78472d9073ef930ff1bad23ac9fd7",
 W8.parent/"native-zone-root/DELIVERY.json":"14b3460c595f3af7a1cd4b9a9165a98a54d9b5ab11f9bd54328c4e1c0bebc85d",
 W8/"vectorfont_provider_luna_4e1/DELIVERY.md":"8a7f70ed79c5126830d45e70e32297511e8422beb000719ccb4dc8ffac79a1bc",
 W8/"r1-vecfloat-terra/checkpoint-1f48/CHECKPOINT.json":"7a63244cda031421f8fff8923f6473b9e35e686ff2b9223316fd60e7890565d7",
 W8/"r1-vecfloat-terra/checkpoint-a3bf/CHECKPOINT.json":"57ba69b6d7944d7384969e5b4cbc590df9d847ede460e20e25574f53f1382a88",
 W8/"racecar_consumers/traffic-jobs-d8cd/CHECKPOINT.json":"4c7324a5a329edf5ffd6f01233253423408a4ca800cb3b32d0ea284a692e0536",
 W8.parent/"camera-three-c0d-terra/final-query-v1/source-object-binding.json":"96eccaba7a99122c68ae9502410fbe2dab4a2631a84df5b6a91c9a995846c007",
 W8.parent/"camera-three-c0d-terra/final-six-v1/source-object-binding.json":"4fc502aa1352913ce93a485394c8ae104dd190c31958e8e5054765008d79cb5b",
 W8.parent/"camera-three-c0d-terra/camera-final-v1/source-object-binding.json":"f532bbaefffd991e950369656a8c013ea3b8f20c1291b465233af79cbcb0baa8",
 W8.parent/"camera-three-c0d-terra/rig-keyanim-receipt.json":"14b77e76d776b3e25981832060dbac3b6cfdddc02c5f61f4eb14e34005115698",
 W8.parent/"parallel/wave8_scene_terra/scene-query-combined/RECEIPT.json":"cb8d560d68ac6e844724347b6e331ed55c37758882b686c6d25e8474b6d26b6e",
 W8.parent/"parallel/wave8_scene_terra/triangle-edge/comparison.json":"2f5c5e9571c53401843326e3df8d2cc17834bdc5a2211fd9e59bb63b3e9cb8b0",
 W8.parent/"parallel/wave8_scene_terra/deformation-header-v1/tag-offset-comparison.json":"83ec4def0ef22e5ca6d7becc69db1a91546e3b7388b59f386b7a29b4e2fb2087",
 W8.parent/"parallel/wave8_scene_terra/driven-point-header/comparison.json":"a52d9c877e076765e835e063d954d0b87c68daf7df53728205655202fc2d9021"}
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
plan={"campaign":"wave8-repair-r2","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"groups":groups,"named_originals":commits,"collect_now":commits,"candidates":rows,"candidate_paths":sorted(paths),"new_paths":sorted(adds),"first_touch_bindings":first,"first_touch_divergences":{p:v for p,v in first.items() if v["parent_blob"]!=v["current_base_blob"] and p not in adds},"final_source_commit_by_path":final,"aliases":aliases,"manifest_bindings":{str(p):h for p,h in receipts.items()},"expected_native_inventory":3616,"expected_native_text_inputs":6293,"owned_cpp_sources":owned_cpp,"preflight_old_graph_closure":closure,"preflight_closure_note":"Conservative c0d literal/basename graph before six new headers; not actual include trace, not a new global result. Actual postcollection graph/selected consumers are required.","required_gates":["Root authorized finite28 original set including sealedWheel and optionalTriangle/Tag/IK, only after exact graph pinned; aliases/prerequisites alreadyinR1 are not replayed.","Collect surgically from exact a26 source; all first-touch blobs must match, no blind final-file adoption.","Freeze new actual source/patch/input map, run whole changedCPP/directconsumers plus actualsharedheader probes; preserve failure reports.","One fresh full3616 diagnostic once stable; compare R1, c0d and publishedWave7 rather than call an incremental subset globalretention."],"retained_debt":["Env copy/scale/defaultW and Loading overlay-prefix cases are bounded; malformed/uninitW/parser/serialized bridges and other scalar/trig/normalization algorithms remain uncertified.","Zone finitepoints/centres/neighbours, softwareestimates/cachehint control, no invalidcounts/allocation/wholeworld proof.","Font22scoped cases, notfullUI; nativeV2/3a/9c/4e1 prerequisites alreadyinR1.","Traffic current union/Random declaration fromDWARF, actualconstructor/centre plus getter/axle data-contract proofs; UpdateEffects/partial-latch/refinement remain incomplete.","Engine getter sret/call boundary tested, old FPU divide/assert/zero calculation notcertified; Wheel uses seededmasks/VMXestimates and controlled saves, notallphysics/hardware.","Camera/queries finite controlled services/realqueue belowcapacity; Rig20 initializers, KeyAnim onlyslices; Gyro wholebehavior/orbit scalar sqrtdiv epsilonNaN/Aftertouch RotationX estimates/otherCameraUtils untestedmethods remain.","Triangle subtract, Tag offset/reference andIK setters/reference identity are bounded actualheader probes; no general deformation/model behavior claim."],"excluded":["allDAC/audio","Deformable3b855 and unsealedPhysicalBodyPart7eada/provider","unsealedVolume/Mask4 plusconsumers","all laterunsealedCamera/otherWIP"],"authorization":"Root approvedfinite R2 graph plussealedWheel andoptionalTriangle/Tag/IK, thenselected+onefull whenstable; no formalreview/canonical publication yet.","parent_gitlink_index":g("ls-files","--stage","b5-decomp",root=PARENT).decode().strip(),"created_utc":datetime.now(timezone.utc).isoformat()}
save("intake-plan-01.json",plan)
config={"campaign":"wave8-repair-r2","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"canonical":str(CANON),"artifact_root":str(HERE),"baseline_metadata":str(W8/"repair-r1/native-parallel-wave8-r1-full-stage1-patch.json"),"baseline_report":str(W8/"repair-r1/full-stage1/compile-results.json"),"baseline_artifact_manifest":str(W8/"repair-r1/full-stage1-terminal-audit.json"),"published_report":str(W8.parent/"wave7/full-successor1/compile-results.json"),"published_artifact_manifest":str(W8.parent/"wave7/native-parallel-wave7-final-artifact-manifest.json"),"baseline_passed":2886,"baseline_failed":730,"baseline_requested":3616,"current_plan":str(HERE/"intake-plan-01.json")}
for key in ("baseline_metadata","baseline_report","baseline_artifact_manifest","published_report","published_artifact_manifest"):config[key+"_sha256"]=sha(config[key])
save("campaign.json",config);save("repair-r2-receipts.json",{"workspace":str(WORK),"base":BASE,"head":BASE,"state":"complete","queue":[],"todo":[],"receipts":[]})
save("plan-freeze.json",{"plan_sha256":sha(HERE/"intake-plan-01.json"),"base":BASE,"originals":29,"paths":len(paths),"owned_cpp":len(owned_cpp),"new_headers":len(adds),"expected_inventory":3616,"expected_input_hashes":6293,"conservative_old_graph_closure":len(closure),"source_unchanged_at_freeze":True})
print(json.dumps(read(HERE/"plan-freeze.json"),indent=2));print("FIRST TOUCH DIVERGENCES",list(plan["first_touch_divergences"]));print("NONEXACT ALIASES",[(r["original"],r["local"]) for r in aliases if not r["same_patch_id"]])
