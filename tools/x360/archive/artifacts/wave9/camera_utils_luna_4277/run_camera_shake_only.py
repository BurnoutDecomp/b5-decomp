from pathlib import Path
import hashlib,json,os,struct,subprocess,sys
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277")
pre=json.loads((art/"preflight-input-binding.json").read_text(encoding="utf-8"))
prior_path=art/"final-five-source-gate"/"source-object-header-binding.json"
prior=json.loads(prior_path.read_text(encoding="utf-8"))
rel="src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"
sources=list(pre["sources"])
def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD")
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).splitlines()
expected={str((repo/s).resolve()).lower() for s in sources}
changed={str((repo/line[2:].strip()).resolve()).lower() for line in status if line and line[:2]!="??"}
untracked=[line for line in status if line.startswith("??")]
if head!=pre["head"] or changed!=expected or untracked:
 raise SystemExit(f"single-file rerun scope mismatch: head={head}, changed={changed}, untracked={untracked}")
prior_sources={s:sha(repo/s) for s in sources}
for s in sources:
 if s!=rel and prior_sources[s]!=prior["sources_after"][s]:raise SystemExit(f"unchanged source drifted since five-file gate: {s}")
if prior_sources[rel]==prior["sources_after"][rel]:raise SystemExit("expected only the camera-shake source to have changed after the five-file gate")
stage=art/"camera-shake-ice-one-file-rerun"
if stage.exists():raise SystemExit(f"stage exists: {stage}")
stage.mkdir(parents=True);tmp=art/"temp";tmp.mkdir(exist_ok=True)
sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
source_before=sha(repo/rel)
command=[sys.executable,"-X","utf8","-B",str(repo/"tools/build_x360.py"),"--sdk",str(sdk),"--out",str(stage),"--jobs","1","--compiler-arg=/showIncludes","--source",rel]
for vendor in ("EABase/include/Common","EASTL/include","EAThread/include","renderware/include","PPMalloc/include","coreallocator/include","dirtysdk/include","zlib/include","lua/src"):
 command.append("--compiler-arg=/I"+str(canonical/"vendor"/vendor))
(stage/"command.json").write_text(json.dumps(command,indent=2)+"\n",encoding="utf-8")
env=dict(os.environ);env["TEMP"]=str(tmp);env["TMP"]=str(tmp)
proc=subprocess.run(command,cwd=repo,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding="utf-8",errors="replace")
(stage/"driver.log").write_text(proc.stdout+proc.stderr,encoding="utf-8")
source_after=sha(repo/rel)
if source_before!=source_after:raise SystemExit("camera source changed during compile")
report_path=stage/"compile-results.json"
if not report_path.is_file():raise SystemExit(f"build produced no compile report (exit {proc.returncode})")
report=json.loads(report_path.read_text(encoding="utf-8"))
if report.get("requested")!=1 or len(report.get("results",[]))!=1:raise SystemExit("expected exactly one camera source in report")
row=report["results"][0];log=Path(row["log"]);logtext=log.read_text(encoding="utf-8",errors="replace");errors=[x for x in logtext.splitlines() if "error " in x]
pre_headers={str(Path(p).resolve()).lower():h for p,h in pre["headers"].items()};headers={}
for line in logtext.splitlines():
 marker="Note: including file:"
 if line.startswith(marker):
  p=Path(line[len(marker):].strip()).resolve();key=str(p).lower();d=sha(p)
  headers[key]={"path":str(p),"sha256":d,"preflight_sha256":pre_headers.get(key),"preflight_bound":key in pre_headers,"unchanged":pre_headers.get(key)==d if key in pre_headers else None}
obj=Path(row["object"]);ob=None
if obj.is_file():
 data=obj.read_bytes();machine=struct.unpack("<H",data[:2])[0] if len(data)>=2 else None
 ob={"path":str(obj.resolve()),"size":len(data),"machine":machine,"machine_hex":f"0x{machine:04X}" if machine is not None else None,"verified_ppc32be_coff":machine==0x01F2,"sha256":sha(obj)}
binding={"base_head":head,"target":"xbox360-ppc-be","sdk":str(sdk.resolve()),"jobs":1,"command":command,"driver_returncode":proc.returncode,"source":rel,"source_sha256_before_gate":source_before,"source_sha256_after_gate":source_after,"source_unchanged_during_gate":source_before==source_after,"previous_five_file_gate":str(prior_path.resolve()),"previous_five_file_gate_sha256":sha(prior_path),"requested":report.get("requested"),"passed":report.get("passed"),"failed":report.get("failed"),"actual_header_count":len(headers),"preflight_bound_header_count":sum(1 for h in headers.values() if h["preflight_bound"]),"preflight_unbound_headers":[h["path"] for h in headers.values() if not h["preflight_bound"]],"preflight_bound_headers_unchanged":all(h["unchanged"] for h in headers.values() if h["preflight_bound"]),"actual_headers":sorted(headers.values(),key=lambda h:h["path"].lower()),"result":{"source":row["source"],"passed":row["passed"],"returncode":row["returncode"],"machine":row["machine"],"diagnostic_count":len(errors),"diagnostics":errors,"log":str(log.resolve()),"log_sha256":sha(log),"object":ob}}
(stage/"source-object-header-binding.json").write_text(json.dumps(binding,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"source":rel,"requested":binding["requested"],"passed":binding["passed"],"failed":binding["failed"],"machine":row["machine"],"diagnostics":len(errors),"actual_headers":len(headers),"preflight_bound":binding["preflight_bound_header_count"],"headers_unchanged":binding["preflight_bound_headers_unchanged"],"object_sha256":ob["sha256"] if ob else None,"binding":str(stage/"source-object-header-binding.json")},indent=2))
sys.exit(proc.returncode or (1 if report.get("failed") else 0))