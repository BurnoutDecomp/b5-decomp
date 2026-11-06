"""Freeze root-authorized Wave8 originals before any intake mutation."""
from datetime import datetime,timezone
import hashlib,json,os,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
PARENT=Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow")
WORK=Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
CANON=PARENT/"b5-decomp"
BASE="3534f7f16103a58ff904bbd1604e18e9934868f4"
W7=HERE.parent/"wave7"
def git(*args,root=WORK):return subprocess.check_output(["git","--no-optional-locks","-C",str(root),*args])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding="utf-8-sig"))
def save(name,d):
 with (HERE/name).open("x",encoding="utf-8",newline="\n") as f:f.write(json.dumps(d,indent=2)+"\n")
for w in [WORK,CANON]:assert git("rev-parse","HEAD",root=w).decode().strip()==BASE and not git("status","--porcelain=v1",root=w).strip()
assert not git("branch","--show-current").strip(), "Intake must remain detached"
snapshot=read(W7/"native-parallel-wave7-final-patch.json")["source_snapshot"]["hashes"]
assert len(snapshot)==6280
for w in [WORK,CANON]:assert all(sha(w/p)==h for p,h in snapshot.items())
groups=read(HERE/"named-groups-47.json")
commits=[c for cs in groups.values() for c in cs]
assert len(set(commits))==len(commits)==47
rows=[];paths=set();adds=set()
for group,cs in groups.items():
 for c in cs:
  assert git("cat-file","-t",c).strip()==b"commit"
  lines=[r.split("\t",1) for r in git("diff-tree","--no-commit-id","--name-status","-r",c).decode().splitlines()]
  assert all(p.startswith(("src/","vendor/")) and "pc" not in [x.lower() for x in Path(p).parts] for s,p in lines)
  patch=git("show","--format=","--binary","--full-index",c)
  rows.append({"group":group,"commit":c,"parent":git("rev-parse",c+"^").decode().strip(),"message":git("show","-s","--format=%B",c).decode(),"paths":[p for s,p in lines],"changes":lines,"patch_sha256":hashlib.sha256(patch).hexdigest()})
  paths.update(p for s,p in lines);adds.update(p for s,p in lines if s=="A")
assert len(paths)==123 and len(adds)==7
physics=set(groups["physics"])
manifest_paths=[
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/secondary_math_v1/COHORT_CLOSURE_V1.json",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/secondary_math_v1/MATRIX33_PRIVATE_CHECKPOINT.json",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/secondary_math_v1/PLUS_PRIVATE_CHECKPOINT.json",
 PARENT/"scratch/parallel-native/vector_abi_sol/layers/v4_pipeline_v1/PRIVATE_CHECKPOINT.json",
 HERE.parent/"gui-v34-final-retention/GUI_FINAL_PRIVATE_HANDOFF.json",
 HERE.parent/"gui-v34-final-retention/six-guard-comparison.json",
 HERE.parent/"director-math-917/receipt.json",HERE.parent/"traffic-plus-abi-terra/receipt.json",
 HERE.parent/"racecar_math_917/PHYSICS_PROVIDER_CHECKPOINT.json",
 HERE.parent/"rendercore-sun-retail-917/SUN_RETAIL_ABI_READINESS.json",
 HERE.parent/"rendercore-sun-retail-917/DELIVERY_SEAL.json",
 HERE.parent/"native-job-entries-root/DELIVERY.json",HERE.parent/"native-tintblend-root/DELIVERY.json",
 HERE.parent/"wave7/triangle_cache_luna_3534/delivery.json",
 Path("D:/Reverse/BP-Decomp_Workflow/scratch/parallel-native/audio_monitor_luna_3534/native-bindings.md")]
prior=read(HERE.parent/"next-native-cohort-preflight-3534/PRE_CUTOFF_PREFLIGHT.json")
manifest_bindings={p:sha(p) for p in prior["evidence_pins"]}
assert manifest_bindings==prior["evidence_pins"], "Historical preflight evidence changed"
manifest_bindings.update({str(p):sha(p) for p in manifest_paths})
assert manifest_bindings[str(HERE.parent/"native-tintblend-root/DELIVERY.json")]=="18fc7ad9fdef27dfad686aa726c7ae15102215ad9e256aae2433fa3c18ffe1f0"
plan={"base":BASE,"workspace":str(WORK),"groups":groups,"named_originals":commits,"collect_now":[c for c in commits if c not in physics],"hold_physics":sorted(physics),"candidates":rows,"candidate_paths":sorted(paths),"new_paths":sorted(adds),"manifest_bindings":manifest_bindings,"alias_source":str(HERE.parent/"next-native-cohort-preflight-3534/PRE_CUTOFF_PREFLIGHT.json"),"expected_native_inventory":3616,"expected_native_text_inputs":6287,"parent_gitlink_index":git("ls-files","--stage","b5-decomp",root=PARENT).decode().strip(),"authorization":"Root authorized47 named originals plus exact mandatory Physics successor/predecessors. Collect44 independent now; hold Physics union until immutable receipt. No DAC/Voice, later PostFx, forced link or canonical publication.","published_union_rules":["Preserve published RaceCar module cleanup/max spelling and Microphone accessors.","Manager7380 preserves all11 added-line anchors from published2c5e; do not replay2c5e.","Preserve native26916 VehiclePhysics VMX/deleted PC diagnostics; stale4f CPP must be superseded by freshly sealed published-to-union delta.","e0 Glass VPU2 locals are superseded by cc152 trueFPU8.","No private aliases or old campaign receipts replayed."],"created_utc":datetime.now(timezone.utc).isoformat()}
save("intake-plan-01.json",plan)
config={"campaign":"wave8","base":BASE,"workspace":str(WORK),"canonical":str(CANON),"artifact_root":str(HERE),"baseline_metadata":str(W7/"native-parallel-wave7-final-patch.json"),"baseline_report":str(W7/"full-successor1/compile-results.json"),"baseline_artifact_manifest":str(W7/"native-parallel-wave7-final-artifact-manifest.json"),"baseline_passed":3400,"baseline_failed":215,"baseline_requested":3615,"current_plan":str(HERE/"intake-plan-01.json")}
for k in ["baseline_metadata","baseline_report","baseline_artifact_manifest"]:config[k+"_sha256"]=sha(config[k])
save("campaign.json",config)
save("wave8-receipts.json",{"workspace":str(WORK),"base":BASE,"head":BASE,"state":"complete","queue":[],"todo":[],"receipts":[]})
archive=HERE/"previous-bookkeeping";archive.mkdir()
for p in [PARENT/"scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json",PARENT/"scratch/x360_sdk/current-native-checkpoint.json",PARENT/"scratch/x360_sdk/parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md"]:
 with (archive/p.name).open("xb") as f:f.write(p.read_bytes())
save("plan-freeze.json",{"plan":str(HERE/"intake-plan-01.json"),"plan_sha256":sha(HERE/"intake-plan-01.json"),"baseline":BASE,"originals":47,"collect_now":44,"paths":123,"new_paths":len(adds),"expected_native_inventory":3616,"expected_text_inputs":6287,"source_unchanged":True})
print(json.dumps(read(HERE/"plan-freeze.json"),indent=2))
