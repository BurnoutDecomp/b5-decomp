"""Compile minimal real-header probes under exact combined R2 flags, with outputs only on D:."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,os,struct
HERE=Path(__file__).resolve().parent
WORK=Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake")
BASE="86a077ed1e9fa00e189280ecba7cedabda42950f"
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(["git","--no-optional-locks","-C",str(WORK),*a])
assert git("rev-parse","HEAD").decode().strip()==BASE and not git("status","--porcelain=v1").strip()
out=HERE/"header-probes";assert not out.exists();out.mkdir();tmp=out/"compiler-temp";tmp.mkdir()
report=json.loads((HERE/"selected-stage1/compile-results.json").read_text())
assert report["state"]=="complete"
row=report["results"][0]
flags=[a for a in row["command"][1:-1] if not a.startswith(("/Fo","/Fa"))]+["/FAcs","/showIncludes"]
probes={
 "TriangleEdges":'#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle.h"\nvoid TriangleEdges(const CgsGeometric::Triangle& t, Vector3& a, Vector3& b) { a=t.CalcEdge01(); b=t.CalcEdge12(); }\n',
 "TagReferences":'#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"\nvoid ProbeTag(const BrnPhysics::Deformation::TagPoint& p, Vector3& out) { out=p.GetOffsetFromInitialPosition(); }\nconst Vector3* ProbeSpecA(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetOffsetFromDeformationSensorA(); }\nconst Vector3* ProbeSpecB(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetOffsetFromDeformationSensorB(); }\nconst Vector3* ProbeSpecInitial(const BrnPhysics::Deformation::TagPointSpec& p) { return &p.GetInitialPosition(); }\n',
 "DrivenPoint":'#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"\nusing namespace BrnPhysics::Deformation;\nvoid DrivenSet(IKDrivenPoint& p, Vector3 v) { p.SetPosition(v); }\nvoid DrivenOffset(const IKDrivenPoint& p, Vector3& out) { out=p.GetOffsetFromInitialPosition(); }\nconst Vector3* DrivenPosition(const IKDrivenPoint& p) { return &p.GetPosition(); }\nconst Vector3* DrivenDirection(const IKDrivenPoint& p) { return &p.GetDirection(); }\n'}
env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp));rows=[]
for name,text in probes.items():
 src=out/(name+".cpp");src.write_text(text,encoding="utf-8");obj=out/(name+".obj");asm=out/(name+".asm");log=out/(name+".log")
 cmd=[row["command"][0],*flags,"/Fo"+str(obj),"/Fa"+str(asm),str(src)]
 run=subprocess.run(cmd,cwd=WORK,env=env,capture_output=True,timeout=180);log.write_bytes(run.stdout+run.stderr)
 passed=run.returncode==0 and obj.is_file() and struct.unpack('<H',obj.read_bytes()[:2])[0]==0x1f2
 rows.append({"name":name,"source":str(src),"source_sha256":sha(src),"command":cmd,"returncode":run.returncode,"passed":passed,"object":str(obj),"assembly":str(asm),"log":str(log),"artifact_sha256":{str(p):sha(p) for p in [log,obj,asm] if p.is_file()}})
assert git("rev-parse","HEAD").decode().strip()==BASE and not git("status","--porcelain=v1").strip()
result={"head":BASE,"requested":3,"passed":sum(r["passed"] for r in rows),"rows":rows,"scope":"Actual combined real-header type/ABI compile probes only. Tag probe intentionally omits unsealed PhysicalBodyPart dependency. No new production source or hardware/runtime/parity claim.","created_utc":datetime.now(timezone.utc).isoformat()}
with (out/"probe-results.json").open("x",encoding="utf-8") as f:json.dump(result,f,indent=2)
print(json.dumps({"head":BASE,"passed":result["passed"],"requested":3,"report":str(out/"probe-results.json"),"report_sha256":sha(out/"probe-results.json")},indent=2))
raise SystemExit(0 if result["passed"]==3 else 1)
