from pathlib import Path
import hashlib,json,os,struct,subprocess,sys
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
pre_path=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\preflight-input-binding.json")
art=pre_path.parent;stage=art/"final-five-source-gate"
sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
sources=["src/GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.cpp","src/GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp","src/GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp","src/GameSource/Director/Camera/Utils/BrnPositionLag.cpp","src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"]
pre=json.loads(pre_path.read_text(encoding="utf-8"))
def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=repo,text=True).strip()
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).splitlines()
expected={str((repo/s).resolve()).lower() for s in sources}
changed={str((repo/s).resolve()).lower() for s in [line[2:].strip() for line in status if line and line[:2]!="??"]}
untracked=[line for line in status if line.startswith("??")]
if head!=pre["head"] or changed!=expected or untracked:raise SystemExit(f"baseline gate requires clean R8: {head} {changed} {untracked}")
if stage.exists():raise SystemExit(f"gate exists already: {stage}")
stage.mkdir(parents=True);tmp=art/"temp";tmp.mkdir(exist_ok=True)
sources_before={s:sha(repo/s) for s in sources}
command=[sys.executable,"-X","utf8","-B",str(repo/"tools/build_x360.py"),"--sdk",str(sdk),"--out",str(stage),"--jobs","1","--compiler-arg=/showIncludes"]
for s in sources:command.extend(["--source",s])
for vendor in ("EABase/include/Common","EASTL/include","EAThread/include","renderware/include","PPMalloc/include","coreallocator/include","dirtysdk/include","zlib/include","lua/src"):
 command.append("--compiler-arg=/I"+str(canonical/"vendor"/vendor))
(stage/"command.json").write_text(json.dumps(command,indent=2)+"\n",encoding="utf-8")
env=dict(os.environ);env["TEMP"]=str(tmp);env["TMP"]=str(tmp)
proc=subprocess.run(command,cwd=repo,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding="utf-8",errors="replace")
(stage/"driver.log").write_text(proc.stdout+proc.stderr,encoding="utf-8")
sources_after={s:sha(repo/s) for s in sources}
if sources_before!=sources_after:raise SystemExit("source changed during baseline compile")
report_path=stage/"compile-results.json"
if not report_path.is_file():raise SystemExit(f"no compile report, exit={proc.returncode}")
report=json.loads(report_path.read_text(encoding="utf-8"));pre_headers={str(Path(p).resolve()).lower():h for p,h in pre["headers"].items()};headers={};results=[];objects={}
for row in report["results"]:
 log=Path(row["log"]);text=log.read_text(encoding="utf-8",errors="replace")
 for line in text.splitlines():
  marker="Note: including file:"
  if line.startswith(marker):
   q=Path(line[len(marker):].strip()).resolve();digest=sha(q);key=str(q).lower();headers[key]={"path":str(q),"sha256":digest,"preflight_sha256":pre_headers.get(key),"preflight_bound":key in pre_headers,"unchanged":pre_headers.get(key)==digest if key in pre_headers else None}
 obj=Path(row["object"]);ob=None
 if obj.is_file():
  data=obj.read_bytes();machine=struct.unpack("<H",data[:2])[0] if len(data)>=2 else None;ob={"path":str(obj.resolve()),"size":len(data),"machine":machine,"machine_hex":f"0x{machine:04X}" if machine is not None else None,"verified_ppc32be_coff":machine==0x01F2,"sha256":sha(obj)};objects[row["source"]]=ob
 errors=[line for line in text.splitlines() if "error " in line]
 results.append({"source":row["source"],"passed":row["passed"],"returncode":row["returncode"],"machine":row["machine"],"diagnostic_count":len(errors),"diagnostics":errors,"log":str(log.resolve()),"log_sha256":sha(log),"object":ob})
binding={"base_head":head,"target":"xbox360-ppc-be","sdk":str(sdk.resolve()),"jobs":1,"command":command,"driver_returncode":proc.returncode,"preflight_manifest":str(pre_path.resolve()),"preflight_manifest_sha256":sha(pre_path),"sources_before":sources_before,"sources_after":sources_after,"source_unchanged_during_gate":sources_before==sources_after,"requested":report.get("requested"),"passed":report.get("passed"),"failed":report.get("failed"),"actual_header_count":len(headers),"preflight_bound_header_count":sum(1 for h in headers.values() if h["preflight_bound"]),"preflight_unbound_headers":[h["path"] for h in headers.values() if not h["preflight_bound"]],"preflight_bound_headers_unchanged":all(h["unchanged"] for h in headers.values() if h["preflight_bound"]),"actual_headers":sorted(headers.values(),key=lambda h:h["path"].lower()),"objects":objects,"results":results}
(stage/"source-object-header-binding.json").write_text(json.dumps(binding,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"requested":report.get("requested"),"passed":report.get("passed"),"failed":report.get("failed"),"headers":len(headers),"preflight_bound":binding["preflight_bound_header_count"],"unbound":len(binding["preflight_unbound_headers"]),"binding":str(stage/"source-object-header-binding.json")},indent=2))
for row in results:
 print(("PASS " if row["passed"] else "FAIL ")+row["source"]+f" diagnostics={row['diagnostic_count']} machine={row['machine']}")
 for e in row["diagnostics"][:14]:print("  "+e)
sys.exit(0 if report.get("failed")==0 and proc.returncode==0 else 1)