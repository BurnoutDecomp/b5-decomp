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
BASE="86a077ed1e9fa00e189280ecba7cedabda42950f"
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
meta=read(W8/"repair-r2/native-parallel-wave8-r2-full-stage1-patch.json")
assert len(meta["source_snapshot"]["hashes"])==6293
assert all(sha(WORK/p)==h for p,h in meta["source_snapshot"]["hashes"].items())
assert sha(W8/"repair-r2/full-stage1-terminal-audit.json")=="f28376434df6c92c081c33642935e5c208a2767084d32adcfa49e2462e665e0e"
assert sha(W8/"repair-r2/full-stage1/compile-results.json")=="9bc810b2038e370f6dde72cd780f425f651acbfd71df5e36b7b9cf129b7ca519"
groups=read(HERE/"named-groups.json");commits=[c for cs in groups.values() for c in cs];assert len(commits)==len(set(commits))==20
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
assert len(paths)==40 and len(adds)==4 and not any(p.endswith((".cpp",".c")) for p in adds)
links={
 "1ede3effb1a731b812399398b591e2afebb72f98":["890f015b3ccf043a388805b2089eb935b83e4f94"],
 "b9cb3fb5a50356654e73ca80986cc626f4b1b294":["eb129be7f69563c5f5f55312f5d676fc52e4f863"],
 "83808045743378684fa401392046db3c4fdf0a9a":["fd614f8228fa1aa9a945055124aee12bbda0c2e7"],
 "d8cd3cbcefed29e1239e1379600b47c5289a9709":["3ad0665b0c2886012068439e8937ccdd9a509da6"],
 "9e403fcc5ff716721fd69da8854d5a9019f255b6":["750828d6e4efef7e8b8355b3fbbe55d6f05ce6f8"],
 "8d5c7818975a58d3da22a722a5700e8d605e72b8":["7ba52ac4f"]}
aliases=[]
for original,als in links.items():
 op=pid(original)
 for alias in als:
  alias=full(alias);ap=pid(alias);aliases.append({"original":original,"local":alias,"same_patch_id":op==ap,"original_patch_id":op,"local_patch_id":ap,"collect_local":False})
receipts={
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/mask4_wave8_v1/MASK4_PRIVATE_CHECKPOINT.json":"f9111edeb4978923ed09fcbe2c3b847f53d155c3c76244706cd59bd2cccf2536",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/mask4_wave8_v1/MASK4_CGS_CLOSURE_V1.json":"1c2f1df7d84521f27e44a3f46feb29a6ed522bb8fa67c120bc82f9033366b6dd",
 W8.parent/"parallel/wave8_scene_terra/sphere13-pair/RECEIPT.json":"62dd07f64dd53fe9799d1c5fa339101a9d26aabab6227b9b500009369ce6aaac",
 W8.parent/"parallel/wave8_scene_terra/sphere13-line-abi/RECEIPT.json":"b4d5fbc4c5a3dcf1131e1815fc1af30613d620ea52b67adab62057e13ecbe5fa",
 W8.parent/"parallel/wave8_scene_terra/sphere13-polysoup/RECEIPT.json":"594ccff3ebf5945b255788225a2188413933ebe2b3e9673badacd3fb273c2b20",
 W8.parent/"parallel/wave8_scene_terra/mask4-consumer-v1/RECEIPT.json":"88fa2efe9eed1a831617d8a8c4389a96df82bb209a78c9572f426b89a3dac241",
 W8.parent/"parallel/wave8_scene_terra/bodypart-baseline/DELIVERY.json":"a83a0a2ffb3c8f530605a99278578b8aafeca3af0e0ac74337a6ef593fed2928",
 W8.parent/"parallel/wave8_scene_terra/deformation-next/DEBUG_DELIVERY.json":"72b97a03ba10e64bd2694157769e914b4185c45aeafcdafa9e0359af75c25d68",
 W8.parent/"parallel/wave8_scene_terra/deformation-next/BBOX_DELIVERY.json":"b08a9b0be933c4a607e230837ec65deac2a1684b5279863b16f02c2ad952ba35",
 W8.parent/"parallel/wave8_scene_terra/deformation-next/LIFECYCLE_DELIVERY.json":"2f742c0273eb62a2e423f5e3c42a8ac9beefdf4061b38b6ee46cc4ff9c53de54",
 W8.parent/"ai-inline-r1-terra/source-object-receipt-v1.json":"6c40a25139ecc110363b198c1f1f54c4e7107a97f78a0531a9c6283bae3f803e",
 W8/"model_native_luna_a26/DELIVERY.md":"8458c527e4a4a9b87231d09d439664ceaaf77780783fbcd1105cb1a54be93d53",
 W8.parent/"blobby-terra-traffic-final/compile-results.json":"4498c78b536a739ca6d06b1bc8dd22b81527b6ea69d221567345aef8fcccc026",
 W8.parent/"blobby-terra-traffic-final/addshadow-comparison.json":"6ffb24d3f9c25169970bae87979725a9bb9758cb3ae1a9d0376af2ad423f96fe",
 W8.parent/"blobby-terra-traffic-final/static-initializers-comparison.json":"7471349ef22ae5eab2b62bbe66eb23e29fa4b50b82db2009de96fb2a608fb2be",
 W8.parent/"blobby-terra-traffic-final/render-controlled-comparison.json":"d5a3c429dbe320c4ef23c6c35ebfb264fc36ba1bff584adc54192eef89bb5261",
 W8.parent/"blobby-terra-traffic-final/traffic-shadow-window-comparison.json":"d12d32692798afca3c5af8174aac01777decdcbdad830cbd0a84ecc62f1e5660",
 W8.parent/"blobby-terra-pvs-probe/pvs-cellsize-comparison.json":"0f08e67d6cbfc08dfbe1307ebb878d341fb0c6cbfab8b4853e0b913f76778b2f",
 W8/"r1-vecfloat-terra/renderparams-5a00-union/CHECKPOINT.json":"ded40a21c25195053f648ecedd05ac9b59b0645b550af4e3c63030d7e5a5874c",
 W8.parent/"native-shader-table-root/DELIVERY.json":"5034903dee9adbfcd1443222a38962d68391a1d20c5981284aa4275a2555fa22",
 W8.parent/"spiralling-r2-terra/source-object-receipt-v1.json":"6c987b15a03b219b37e571d55f6f8f8e9e393c1b695a2ce1237973d6e6503bba"}
receipts.update({Path(p):h for p,h in read(HERE/"header-proof-bindings.json").items()})
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
plan={"campaign":"wave8-repair-r3","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"groups":groups,"named_originals":commits,"collect_now":commits,"candidates":rows,"candidate_paths":sorted(paths),"new_paths":sorted(adds),"first_touch_bindings":first,"first_touch_divergences":{p:v for p,v in first.items() if v["parent_blob"]!=v["current_base_blob"] and p not in adds},"final_source_commit_by_path":final,"aliases":aliases,"manifest_bindings":{str(p):h for p,h in receipts.items()},"expected_native_inventory":3616,"expected_native_text_inputs":6297,"owned_cpp_sources":owned_cpp,"preflight_old_graph_closure":closure,"preflight_closure_note":"Conservative c0d literal/basename graph before six new headers; not actual include trace, not a new global result. Actual postcollection graph/selected consumers are required.","required_gates":["Root authorized exactfinite20 originals includingsealed Shader andSpiralling only after planpinned; no aliases/published/R2duplicate replay.","Verify current86a ownedpreimage blobs, surgical sourcepatches only; World7eada realprovider header before compile-dependent3b855.","Run actualwholeCPP changedproducts/provider/directconsumer union, retaining oldfailed consumers; then onefreshfull3616 once source stable.","Bind all6297sourceinputs/actualobjects/listings/logs/externalcompiler+submodules and compare R2+published; formalreview/publication held until actualretention repaired."],"retained_debt":["Mask4 rawopaque maskbits/specificfinite geometry domains; unusedMask3/anyall/default exactsource spelling andhardware notcertified. RawSOA returnsVectorIntrinsic, notMask4.","Worldline/polygon scalarFP/NaN/reciprocal model and Volumehostdescriptor debt retained; fouradditional Mask4consumers remain baselinefailed, no wholebodyPASS claim.","BodyPartUpdateJoint/AddToSim/Debug/BBox/Lifecycle controls specified; TestJoint/Spy priorwindows only, ResetDeformation notend-to-end, ClearVariables/service fidelity unproved, Update.cpp excluded.","AIheader setter mapping/directsubtractVMX probes bounded; HardNoGoMap/RacingLineGenerator CPP failures retained, noWorld lifecycle.","Model only64 rawcases count1..4 actualfourV4+fourMatrix callerlayout, fifthread aliasesmatrixrow; nofabricatedfifthV4. Count0/5, adjacentgetterassert distinction andGPU remain outside.","Blobby/PVS/Traffic no replay/wheels/detachedparts/corona recovery; scalarfog, EndRendering globalcleanup, assertions/priorcontrolflow remain untested, GPUservices controlled.","RenderParams getter/setter validindices/scratch/modeled lanes only; ActiveRaceCar.cpp remains baselinefailed. Render.cpp andRCE containment held.","Shader Plusprovider/affinearrays/CPUShouldSerialise validcontrolledcases; existingPC serializedviews/fixup accommodations remain uncertified, separatecleanup excluded.","Spiralling provider+7representative callers8/8, NOT104measuredgain; constructor/attachment/proxy bounded, old Update sincos/sqrt/scalarorbit unproved."],"excluded":["WorldDeformUpdate activeWIP","helddd11/RCEcontainment/Render.cpp","laterShadernativepointercleanup","WorldBootunsealed/checkpoint","Corona/Curves/bufferproviders","allDAC/audio/Volume/rawkernels","all otherunnamedWIP"],"authorization":"Root explicitfinite20 collection+selected+onefull after exactplan/seals coherent; no formalreview/canonical publish while486publishedregressions remain.","parent_gitlink_index":g("ls-files","--stage","b5-decomp",root=PARENT).decode().strip(),"created_utc":datetime.now(timezone.utc).isoformat()}
save("intake-plan-01.json",plan)
config={"campaign":"wave8-repair-r3","base":BASE,"published_base":PUBLISHED,"workspace":str(WORK),"canonical":str(CANON),"artifact_root":str(HERE),"baseline_metadata":str(W8/"repair-r2/native-parallel-wave8-r2-full-stage1-patch.json"),"baseline_report":str(W8/"repair-r2/full-stage1/compile-results.json"),"baseline_artifact_manifest":str(W8/"repair-r2/full-stage1-terminal-audit.json"),"published_report":str(W8.parent/"wave7/full-successor1/compile-results.json"),"published_artifact_manifest":str(W8.parent/"wave7/native-parallel-wave7-final-artifact-manifest.json"),"baseline_passed":2954,"baseline_failed":662,"baseline_requested":3616,"current_plan":str(HERE/"intake-plan-01.json")}
for key in ("baseline_metadata","baseline_report","baseline_artifact_manifest","published_report","published_artifact_manifest"):config[key+"_sha256"]=sha(config[key])
save("campaign.json",config);save("repair-r3-receipts.json",{"workspace":str(WORK),"base":BASE,"head":BASE,"state":"complete","queue":[],"todo":[],"receipts":[]})
save("plan-freeze.json",{"plan_sha256":sha(HERE/"intake-plan-01.json"),"base":BASE,"originals":len(commits),"paths":len(paths),"owned_cpp":len(owned_cpp),"new_headers":len(adds),"expected_inventory":3616,"expected_input_hashes":6297,"conservative_old_graph_closure":len(closure),"source_unchanged_at_freeze":True})
print(json.dumps(read(HERE/"plan-freeze.json"),indent=2));print("FIRST TOUCH DIVERGENCES",list(plan["first_touch_divergences"]));print("NONEXACT ALIASES",[(r["original"],r["local"]) for r in aliases if not r["same_patch_id"]])
