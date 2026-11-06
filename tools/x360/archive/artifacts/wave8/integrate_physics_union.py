"""Apply only sealed Physics components, keeping published native source as the base."""
import hashlib,json,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
WORK=Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
PHYS=HERE.parent/"racecar_math_combined"
STATE=HERE/"wave8-receipts.json"
state=json.loads(STATE.read_text(encoding="utf-8-sig"))
BASE=state["base"]
def git(*a):return subprocess.check_output(["git","--no-optional-locks","-C",str(WORK),*a])
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save():STATE.write_text(json.dumps(state,indent=2)+"\n",encoding="utf-8")
def record(source,paths,outcome,proof,before):
 head=git("rev-parse","HEAD").decode().strip()
 original=git("diff-tree","--no-commit-id","--name-only","-r",source).decode().splitlines()
 assert set(paths)<=set(original)
 row={"source_commit":source,"integrated_commit":head,"paths":original,"integrated_paths":paths,"outcome":outcome,"before":before,"proof":proof}
 state["receipts"].append(row);state["queue"].append(source);state["head"]=head;state["state"]="complete";save()
 print(source,head,outcome,flush=True)
def apply(source,paths,patch,expected_sha=None):
 assert state["head"]==git("rev-parse","HEAD").decode().strip() and not git("status","--porcelain=v1").strip()
 before=state["head"]
 if expected_sha:assert sha(patch)==expected_sha
 git("apply","--index","--whitespace=nowarn",str(patch))
 changed=git("diff","--cached","--name-only").decode().splitlines();assert set(changed)==set(paths),(changed,paths)
 for path in paths:assert git("rev-parse",":"+path).strip()==git("rev-parse",source+":"+path).strip(),path
 git("-c","core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol","diff","--cached","--check")
 git("commit","-C",source)
 assert not git("status","--porcelain=v1").strip()
 record(source,paths,"sealed-source-component",{"patch":str(patch),"patch_sha256":sha(patch),"native_union":"Vehicle portions from4f/af0 are intentionally superseded by the final b3 published-to-union delta"},before)
assert state["state"]=="complete" and not state["todo"] and len(state["receipts"])==46
assert sha(PHYS/"COMBINED_UNION_CHECKPOINT.json")=="926f3bad174489b58afbaeadf5db95affb0928e0e230be25c72dceae0e4e679a"
parts=HERE/"source-components";parts.mkdir()
for source,path,name in [
 ("4f2b0ec346702a11c54695dc3d2bfc457ef53955","src/GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h","4f-GripGraph-only.patch"),
 ("af0c992536704dc197057d357162f82da617ac5c","src/GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCarRenderParams.cpp","af0-RenderParams-only.patch")]:
 patch=parts/name
 with patch.open("xb") as f:f.write(git("show","--format=","--binary","--full-index",source,"--",path))
 apply(source,[path],patch)
vehicle="src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.cpp"
assert git("rev-parse","HEAD:"+vehicle).strip()==git("rev-parse",BASE+":"+vehicle).strip(),"Vehicle must still be exact published native source before union"
apply("b3dc329e42c0da3aac5e8fa4c1351d06bad1aa08",[vehicle],PHYS/"published-to-union.patch","c16303d83e36a34d6ec06fb956c3ff3f602286a688ad0ce373797a1aca72e717")
source="be2d84270626c19e95bcdc1498d963f016a9bfe8"
module="src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp"
if git("rev-parse","HEAD:"+module).strip()!=git("rev-parse",source+":"+module).strip():
 print(git("diff","--stat","HEAD",source,"--",module).decode(),flush=True)
 print(git("diff","--unified=2","HEAD",source,"--",module).decode()[:14000],flush=True)
 raise SystemExit("Module differs from sealed union; inspect instead of overwriting")
record(source,[module],"already-present-exact-postimage",{"git_blob":git("rev-parse","HEAD:"+module).decode().strip(),"sealed_module_commit":source,"reason":"Actual current module automatic merge already preserves all published and Math/FPU8 edits; exact full Gitblob equals owner final union."},state["head"])
subprocess.run([sys.executable,"-X","utf8",str(HERE/"integrate_wave8.py")],check=True)
