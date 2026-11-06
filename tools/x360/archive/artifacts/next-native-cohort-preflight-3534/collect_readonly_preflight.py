"""Read-only Git/source/evidence inspection; creates only new preflight reports on D:."""
import collections
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
PARENT = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow")
CANON = PARENT / "b5-decomp"
INTAKE = Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE = "3534f7f16103a58ff904bbd1604e18e9934868f4"
ART = Path("D:/Reverse/BP-X360-Experimental-Artifacts")
W7 = ART / "wave7"
ENV = dict(os.environ, GIT_OPTIONAL_LOCKS="0")

def git(*args, root=CANON):
    return subprocess.check_output(["git", "-C", str(root), *args], env=ENV)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def write_new(name, data):
    with (HERE / name).open("x", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(data, indent=2) + "\n")

def tree(commit):
    return {r.split("\t", 1)[1]: r.split()[2] for r in git("ls-tree", "-r", commit).decode().splitlines()}

families = {
 "math": {"owner":"/root/vector_abi_sol; foreign Average owned by /root/sound_resume_terra", "status":"priority ABI candidate; RoadRunner/CrashNav caller migrations, RaceCarPhysics value-init semantics and genuine FPU8 Glass closure require separately bound recovery", "commits":["44c5afaf561822618018ac5f6949c9b3e6e1d9bf","63dc7ecb8bbbb6222f4914419d0f4c182c9a41e2","b32a1894ec0547b93b100af54fc78aeec96d2b8f","09964724f1f18b31b3b5454496479cdef18da95a","af4711b10fd2bcc7f28a1a71ca37444ae0d0699a","e0fbca6a2df8acdbb6564888317fc039b3c485a1","917521de878436ca131a85c90685d011a00970aa"]},
 "root_library": {"owner":"/root", "status":"private ready scope; not yet combined with Math", "commits":["2f58099a9af26fda42e62d1163e5d3a6114a7a9f","9244fa777509e185df96e2589e503f048fd00114","7ff587fa5b75467f2f7e4be73f6522a7c8c1501a","a6217968bf01fd2303a3ce43c160ad7e1ed8e008"]},
 "apt": {"owner":"/root routing original Luna delivery", "status":"two-commit cleanup/assert-continuation closure; ARTIST native authority", "commits":["159849412fd3587f053c9c4afd9cac938bff40a4","1a4673f65885b922d64a048e018708a0b6876751"]},
 "stream_pool": {"owner":"/root/audio_fma_luna", "status":"private whole caller/provider compile ready; no runtime proof", "commits":["606bf584b2a0a7fbac80f64fcb075cef5e65167a"]},
 "audio_system": {"owner":"/root/boot_resume_sol", "status":"independent System provider ready; DAC lifecycle separate", "commits":["00d2278e5433b2b8d6b72e7a70e0101bb1ee1dbc"]},
 "sun": {"owner":"/root/rendercore_sol", "status":"body/projection scope ready; shared Vector3 ABI must be rechecked after Math", "commits":["a6e6cdbe4ffe05d419a31a28320709d315ac89f2"]},
 "semaphore": {"owner":"/root/replay_resume_terra; independent CR0 model correction by /root", "status":"native Post plus corrected bounded composition ready; no contention/hardware", "commits":["838c51cea25fc3091edd6aa26dd41b68b900ec33"]},
 "scene_worker": {"owner":"/root/scene_layout_terra", "status":"optional coherent 375 source node; actual async thread-index/runtime depends on separate worker providers", "commits":["375fe77f4b8ddcef04835d94d51323052c7d6a3a"]},
 "relocator_job_entries": {"owner":"/root/world_resume_terra (first seven); /root (last)", "status":"source graph ready; Root final DELIVERY binding pending at preflight", "commits":["945a800068defacf261a7bc4139b84c9c5d184fe","6a97eaeec080fd2c167f4958680e4b8e6140eb78","c831b31c4ffb70f51d1ba6d828ecd9e147853f8e","8242dfe5e3e0486ba42e39107c70e5fcb6ff610b","bfeae53400aec94df4e448b73a3cf934bbca479d","2917526da2aea05522c0d30d29ea43acdebb00bc","199633c1ac15f9cba83ad588c1e652867e8be2e7","9acbf432092522daf57bfc087b42c9a0f9375f46"]},
}

patch = read(W7 / "native-parallel-wave7-final-patch.json")
raw = read(W7 / "full-successor1/compile-results.json")
assert raw["state"] == "complete" and len(raw["results"]) == 3615
assert raw["passed"] == 3400 and raw["failed"] == 215
baseline_tree = tree(BASE)
current = {}
for name, root in [("canonical", CANON), ("intake", INTAKE)]:
    head = git("rev-parse", "HEAD", root=root).decode().strip()
    status = git("status", "--porcelain=v1", root=root).decode()
    mismatch = [p for p,h in patch["source_snapshot"]["hashes"].items() if sha(root/p) != h]
    assert head == BASE and not status and not mismatch, (name, head, status, mismatch)
    current[name] = {"path":str(root),"head":head,"status_porcelain":status,"rechecked_input_count":len(patch["source_snapshot"]["hashes"]),"input_mismatches":mismatch}
remote = git("ls-remote", "origin", "refs/heads/codex/experimental-x360-byte-match").decode().strip()
assert remote.split()[0] == BASE

all_paths = set()
for key, fam in families.items():
    items = []; first = {}; additions = set(); deletions = set()
    for c in fam["commits"]:
        meta = git("show", "-s", "--format=%H%n%P%n%B", c).decode()
        head,parent,_ = meta.split("\n",2)
        assert head == c
        changes = [r.split("\t",1) for r in git("diff-tree","--no-commit-id","--name-status","-r",c).decode().splitlines()]
        pt = tree(parent)
        for s,p in changes:
            first.setdefault(p, {"commit":c,"parent_blob":pt.get(p),"published_blob":baseline_tree.get(p)})
            if s == "A": additions.add(p)
            if s == "D": deletions.add(p)
        content = git("show","--format=","--binary","--full-index",c)
        pid = subprocess.check_output(["git","patch-id","--stable"],input=content,env=ENV).decode().split()[0]
        items.append({"commit":c,"parent":parent,"message":meta.split("\n",2)[2],"patch_sha256":hashlib.sha256(content).hexdigest(),"stable_patch_id":pid,"changes":[{"status":s,"path":p} for s,p in changes]})
    fam["nodes"] = items
    fam["paths"] = sorted(first)
    fam["new_paths"] = sorted(additions)
    fam["deleted_paths"] = sorted(deletions)
    fam["new_native_cpp"] = sorted(p for p in additions if Path(p).suffix.lower() in {".cpp",".c"})
    fam["first_touch_binding"] = first
    fam["first_parent_divergences"] = {p:v for p,v in first.items() if v["parent_blob"] != v["published_blob"] and p not in additions}
    all_paths.update(first)

aliases=[]
for original, alias, note in [
 ("f5e4412b090387fcefcdbc9c75f03824704f51ca","1f20b33943f6f0684f0bb387806771d02063b632","RoadRunner identity dependency already published; current file equals private alias"),
 ("09964724f1f18b31b3b5454496479cdef18da95a","2f95879b4290f8c46e8f35c1d275a7959c214a5c","099 is first durable checkpoint of then-uncommitted Sound Average Record only; no older Sound commit;2f958 exact private alias, not panning f76 or24-path held Sound diff"),
 ("d6025c03d7a1dd3297456d61eae03888c997e812","b328a06e3385a9b4a2e7132a7835d0d024f47df3","Shared assert-buffer adoption; both blobs already supplied by375; do not collect either duplicate"),
 ("00d2278e5433b2b8d6b72e7a70e0101bb1ee1dbc","59b5cd65864f83c58a9237149627240cd1a1c0cf","System original vs DAC private adoption; collect only original if chosen"),
]:
    patches=[git("show","--format=",c) for c in [original,alias]]
    pids=[subprocess.check_output(["git","patch-id","--stable"],input=p,env=ENV).decode().split()[0] for p in patches]
    assert pids[0]==pids[1]
    aliases.append({"node":original,"alias":alias,"same_patch_id":pids[0],"byte_identical_patch":patches[0]==patches[1],"note":note})
shared_paths=["src/GameShared/Jobs/CgsJobAssertBuffer.h","src/GameShared/Jobs/DecompressionJob/CgsDecompressor.cpp"]
shared={p:git("rev-parse","375fe77f4b8ddcef04835d94d51323052c7d6a3a:"+p).decode().strip() for p in shared_paths}
for p,h in shared.items(): assert h==git("rev-parse","d6025c03d7a1dd3297456d61eae03888c997e812:"+p).decode().strip()
road="src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp"
assert baseline_tree[road] == git("rev-parse","1f20b33943f6f0684f0bb387806771d02063b632:"+road).decode().strip()

# Conservative graph describes possible reachability; it is not an actual preprocessor trace.
reverse=collections.defaultdict(set)
for a,bs in patch["source_snapshot"]["dependencies"].items():
    for b in bs: reverse[b].add(a)
seen=set(families["math"]["paths"]); q=collections.deque(seen)
while q:
    for p in reverse[q.popleft()]:
        if p not in seen: seen.add(p); q.append(p)
closure=set(patch["native_inventory"]) & seen
rows={r["source"]:r for r in raw["results"]}
math_diag=read(PARENT/"scratch/parallel-native/vector_abi_sol/layers/v34_homes_v1/remaining-two-owned-zero-consumer-failures-v1.json")
failures=[]
for x in math_diag["failures"]:
    counter=collections.Counter((r["path"].split("/vector_abi_affine/")[-1],r["code"]) for r in x["diagnostics"])
    failures.append({"source":x["source"],"published_passed":rows[x["source"]]["passed"],"private_log":x["log"],"private_log_sha256":x["log_sha256"],"diagnostic_groups":[{"path":p,"code":k,"count":n} for (p,k),n in sorted(counter.items())]})

pins=[W7/"wave7-final-publish-receipt.json",W7/"WAVE7_FINAL_REVIEW_RESULT.md",W7/"native-parallel-wave7-final-review-result.json",W7/"full-successor1/compile-results.json",W7/"full-successor1-terminal-audit.json",W7/"native-parallel-wave7-final-patch.json"]
pins += [PARENT/"scratch/parallel-native/vector_abi_sol/layers"/rel for rel in ["shared_headers_v1/PRIVATE_CHECKPOINT.json","sdk_homes_v1/BUILD_TRANSFORM_PRIVATE_CHECKPOINT.json","native_sdk_vecfloat_v1/PRIVATE_CHECKPOINT.json","affine_copy_v1/PRIVATE_CHECKPOINT.json","vector2_v1/PRIVATE_CHECKPOINT.json","v34_homes_v1/PRIVATE_CHECKPOINT.json"]]
pins += [PARENT/"scratch/parallel-native/vector_abi_sol/layers"/rel for rel in ["affine_copy_v1/foreign-sound-header.json","sdk_homes_v1/value-init-audit.json"]]
pins += [ART/rel for rel in ["native-contact-stunt-root/DELIVERY.json","native-particle-description-root/DELIVERY.json","native-formatter-root/DELIVERY.json","native-osglobal-root/DELIVERY.json","wave7/stream_pool_luna_9388/delivery.json","audio-system-native-9388/DELIVERY.json","rendercore-suncorona-a449/NEXT_NATIVE_RENDERCORE_DELIVERY.json","rendercore-suncorona-a449/sun-checkpoint.json","semaphore-cr0-model-root/composed-word-cr0-comparison.json","wave6/replay-provider-terra/PROVIDER_HANDOFF.md","parallel/relocator_job_terra/WORKER_PROVIDER_RECEIPT.json","parallel/job_entries_author_terra/RECEIPT.json","parallel/relocator_job_terra/combined-792/COMBINED_RECEIPT.json"]]
pins += [Path("D:/Reverse/BP-Decomp_Workflow/scratch/parallel-native/apt_communicator_luna_9388")/r for r in ["native-bindings.md","guard-followup/native-assert-evidence.md"]]
math_dir=PARENT/"scratch/parallel-native/vector_abi_sol/layers/v34_homes_v1"
math_manifest=read(math_dir/"PRIVATE_CHECKPOINT.json")
math_bound=read(math_dir/"v34-final-cohort-v1-bound.json")
math_work=Path(math_manifest["worktree"])
assert git("rev-parse","HEAD",root=math_work).decode().strip()==math_manifest["commit"]
assert not git("status","--porcelain=v1",root=math_work).decode()
math_bad_inputs=[p for p,h in math_bound["source_inputs"].items() if sha(math_work/p)!=h]
math_artifacts=0
for row in math_bound["results"]:
    if not row["passed"]: continue
    for k,h in row["sha256"].items():
        assert sha(row[k])==h,(row["source"],k)
        math_artifacts+=1
for name,h in math_manifest["artifacts"].items(): assert sha(math_dir/name)==h,name
assert not math_bad_inputs and math_artifacts==111
private_math={"head":math_manifest["commit"],"status_porcelain":"","report":str(math_dir/"v34-final-cohort-v1-bound.json"),"report_sha256":sha(math_dir/"v34-final-cohort-v1-bound.json"),"inputs":len(math_bound["source_inputs"]),"input_mismatches":math_bad_inputs,"successful_artifacts_hashed":math_artifacts,"pinned_evidence_files_hashed":len(math_manifest["artifacts"]),"selected":39,"passed":37,"failed":2,"real_product_cpp":29,"real_product_cpp_passed":27,"header_probes":10,"header_probes_passed":10,"prior31_retained":31,"not_a_full_retention_gate":True}
report={
 "phase":"READ_ONLY_PRE_CUTOFF; NO_SOURCE_COLLECTION_OR_BUILD_AUTHORIZED_YET",
 "created_utc":datetime.now(timezone.utc).isoformat(),
 "owner":"/root/wave5_integrator_sol",
 "baseline":{"head":BASE,"branch":"codex/experimental-x360-byte-match","native_inventory":3615,"passed":3400,"failed":215,"text_input_count":6280,"retention_required":3400,"current_checkouts":current,"remote":remote,"goal":"ACTIVE; strict link/image/launch unproved"},
 "candidate_families":families,
 "private_math_actual_verification":private_math,
 "already_published_sun_prerequisites":[{"original":"a4491a2a22f8a8e19053b924f63234413bdf7f0d","published_alias":"b2cd93002f9c7f6d3005e8b5620ced4b59aec5da"},{"original":"790d9599074979c76a68a1f5030935567ddbe319","published_alias":"d8174afb5806174ece6d51a723f5ef1d09bcb839"}],
 "candidate_inventory_scenarios":{"math_only":{"commits":7,"paths":60,"native_inventory":3615,"text_input_files":6280},"math_and_initial_later_registry":{"commits":18,"paths":79,"native_inventory":3615,"text_input_files":6281},"all_inventoried_including_relocator_entries":{"commits":26,"paths":len(all_paths),"native_inventory":3616,"text_input_files":6284},"note":"Candidate counts are not a chosen cutoff or forecast of passes. d602/b328 aliases are excluded. Additional genuine mandatory caller fixes may change paths/commits but must be separately checkpointed."},
 "math_conservative_reach":{"native_inputs":len(closure),"published_passes":sum(rows[p]["passed"] for p in closure),"published_failures":sum(not rows[p]["passed"] for p in closure),"method":patch["source_snapshot"]["method"],"not_a_preprocessor_trace":True},
 "math_known_published_pass_caller_failures":failures,
 "aliases":aliases,
 "shared_assert_buffer_blobs_supplied_by375":shared,
 "evidence_pins":{str(p):sha(p) for p in pins},
 "dependencies":[
  "Math44c ->63dc ->b32 ->foreign Sound099 ->af471 ->e0fb ->917; preserve already-published RoadRunner f5 equivalent instead of alias1f20.",
  "Root2f ->9244 ->7ff ->a621;3a070 prerequisite already published.",
  "Apt159849 ->1a4673; native ARTIST assertion continuation required.",
  "Sun a6e requires a449/790 source providers already published; rebind vector-entry ABI with real Math, do not infer it from old equal-value normalization.",
  "Scene375 requires published a254 and HardwareInit JobManager; supplies d602 shared buffer itself. Actual async runtime still needs true thread-ID and worker entry providers.",
  "Scene375/shared-buffer ->World945a ->6a97 ->c831 ->8242 ->bfea ->2917 ->199633 ->Root9ac. Do not replay b328 or World/Root adoption aliases.",
  "Independent System00d may be chosen alone; DAC alias59b is not another source node. DAC full Voice composition is held, despite22/22 private selected gates."
 ],
 "publication_requirements":[
  "Root must first bind exact cutoff and original/dependency order; then integrate only named original patches with new receipts onD. Preserve published RaceCar native cleanup/max spelling and Microphone accessors at the two Math overlaps.",
  "Freeze exact head/patch/input map and selected providers/callers, then run a fresh complete native inventory sweep for broad Math/header API changes; retain every3400 published pass and every old failed source row.",
  "Use genuine typed constructors/accessors/VMX contracts to repair compiler regressions; do not add scalar compatibility fields/overloads or omit affected products.",
  "Bind all actual source/input hashes, successful PPC01F2 objects/listings/logs, failures, SDK and cl/c1.dll/c1xx/c2/runtime hashes, all canonical fallback include roots and three clean pinned submodules.",
  "Obtain exact-head verdict from same sole reviewer /root/wave2_review_sol, preserve any failed frozen verdict and negatives, then run original mandatory audit unchanged.",
  "Only after exact PASS/zero published-pass regressions may canonical FF and experimental-only push/remote source-resource hash verification occur. No strict link/image/launch claim from these compile/provider scopes."
 ],
 "open_items":[
  "Genuine FPU8 Glass/provider/RenderParams/caller graph must be bound; e0 contains two supersedable module VPU2 local constructor hunks. Private32aa8d26 deliberately omits them and is NOT an exact e0 alias; do not substitute aliases or reintroduce VPU16 over native FPU8.",
  "PublishedPASS RaceCarPhysics.cpp has eight Vector3() expressions on lines880,964(two),1206,1221,1290,1345,1537 outside917. Native empty defaults must not silently erase their actual zero/argument stores; recover and witness original semantics.",
  "Root final native-job-entries DELIVERY binding pending; source9ac and World graph already read-only pinned.",
  "RoadRunner/CrashNavMap need native typed caller closure; other published-pass regressions remain unknown until actual complete header migration sweep.",
  "Root chooses the exact cutoff; no collection, compile, external-app write or publication has occurred for this preflight."
 ],
 "retained_limits":[
  "Math checkpoint37/39/prior31 retained is selected-only; global VecFloat alias/Director and separate Matrix homes and unused scalar APIs remain uncertified. Native default vector constructors stay empty; explicit zero/identity sites must remain genuine.",
  "Loose original allocation requests16 but member effective alignment128 is inferred from two ARTIST contexts; stronger real allocation alignment remains unproved. JobData type remains64/0x800; shared assert original array extent beyond0x2000 stream capacity is unproved.",
  "Relocator existing +04..0F prefix declarations unknown; Command[16]/Result[16] model observed storage spans with proven stride, not proved original source bounds; no counts/clamps inferred. Polygon64 is observed global array storage only, not proved original type/member alignment.",
  "Sun equal-value old vector-argument body tests are not restored native ABI proof; no GPU/frame claim. Semaphore corrected CR0 fixture preserves old18 failures, no contention/hardware proof. Audio System counter lowword is equivalent but12vs8 bytes; StreamPool evidence compile/symbol-only.",
  "DAC/Voice lifecycle is explicitly incomplete:18 descriptor IDs still use the manual switch; Route migration is in progress. Do not collect the partial broad PlugIn ABI solely because22selected inputs compile.",
  "All Wave7/Wave6 raw failures, exact FAIL/PASS verdicts, source bindings, object history and publication receipts remain immutable."
 ],
}
assert len(all_paths)==95
write_new("PRE_CUTOFF_PREFLIGHT.json",report)
write_new("PREFLIGHT_HASH.json",{"report":"PRE_CUTOFF_PREFLIGHT.json","sha256":sha(HERE/"PRE_CUTOFF_PREFLIGHT.json"),"collector_sha256":sha(__file__)})
print(json.dumps({"report":str(HERE/"PRE_CUTOFF_PREFLIGHT.json"),"sha256":sha(HERE/"PRE_CUTOFF_PREFLIGHT.json"),"base":BASE,"candidate_counts":report["candidate_inventory_scenarios"],"math_reach":report["math_conservative_reach"]},indent=2))
