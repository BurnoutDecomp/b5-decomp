from pathlib import Path
import hashlib,json,os,struct,subprocess,sys
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\camera_utils_luna_4277")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277\successor-camera-shake-vector3-wzero")
pre=json.loads((art/"successor-preflight.json").read_text(encoding="utf-8"))
stage=art/"camera-shake-ice-native-gate"
rel="src/GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp"
def sha(p):
 h=hashlib.sha256()
 with Path(p).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD");status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).splitlines()
expected={str((repo/rel).resolve()).lower()}
changed={str((repo/l[2:].strip()).resolve()).lower() for l in status if l and l[:2]!="??"}
untracked=[l for l in status if l.startswith("??")]
if head!=pre["parent"] or changed!=expected or untracked:raise SystemExit(f"scope mismatch before one-file compile: {head=} {changed=} {untracked=}")
if stage.exists():raise SystemExit(f"stage already exists: {stage}")
stage.mkdir(parents=True);tmp=art/"temp";tmp.mkdir(exist_ok=True)
src_before=sha(repo/rel);sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
cmd=[sys.executable,"-X","utf8","-B",str(repo/"tools/build_x360.py"),"--sdk",str(sdk),"--out",str(stage),"--jobs","1","--compiler-arg=/showIncludes","--source",rel]
for vendor in ("EABase/include/Common","EASTL/include","EAThread/include","renderware/include","PPMalloc/include","coreallocator/include","dirtysdk/include","zlib/include","lua/src"):
 cmd.append("--compiler-arg=/I"+str(canonical/"vendor"/vendor))
(stage/"command.json").write_text(json.dumps(cmd,indent=2)+"\n",encoding="utf-8")
env=dict(os.environ);env["TEMP"]=str(tmp);env["TMP"]=str(tmp)
proc=subprocess.run(cmd,cwd=repo,env=env,stdin=subprocess.DEVNULL,capture_output=True,text=True,encoding="utf-8",errors="replace")
(stage/"driver.log").write_text(proc.stdout+proc.stderr,encoding="utf-8")
src_after=sha(repo/rel)
if src_before!=src_after:raise SystemExit("source changed during one-file compile")
report_path=stage/"compile-results.json"
if not report_path.is_file():raise SystemExit(f"missing compile report, rc={proc.returncode}")
report=json.loads(report_path.read_text(encoding="utf-8"))
if report.get("requested")!=1 or len(report.get("results",[]))!=1:raise SystemExit("expected exactly one source")
row=report["results"][0];log=Path(row["log"]);logtext=log.read_text(encoding="utf-8",errors="replace");errors=[x for x in logtext.splitlines() if "error " in x]
preheaders={str(Path(h["path"]).resolve()).lower():h["sha256"] for h in pre["headers"]};headers={}
for line in logtext.splitlines():
 marker="Note: including file:"
 if line.startswith(marker):
  p=Path(line[len(marker):].strip()).resolve();key=str(p).lower();d=sha(p)
  headers[key]={"path":str(p),"sha256":d,"preflight_sha256":preheaders.get(key),"preflight_bound":key in preheaders,"unchanged":preheaders.get(key)==d if key in preheaders else None}
obj=Path(row["object"]);ob=None
if obj.is_file():
 data=obj.read_bytes();machine=struct.unpack("<H",data[:2])[0] if len(data)>=2 else None
 ob={"path":str(obj.resolve()),"size":len(data),"machine":machine,"machine_hex":f"0x{machine:04X}" if machine is not None else None,"verified_ppc32be_coff":machine==0x01F2,"sha256":sha(obj)}
binding={"base_head":head,"target":"xbox360-ppc-be","sdk":str(sdk.resolve()),"jobs":1,"showIncludes":True,"command":cmd,"driver_returncode":proc.returncode,"source":rel,"source_parent_blob":pre["source"]["parent_blob"],"source_preflight_sha256":pre["source"]["pre_sha256"],"source_sha256_before_gate":src_before,"source_sha256_after_gate":src_after,"source_unchanged_during_gate":src_before==src_after,"requested":report.get("requested"),"passed":report.get("passed"),"failed":report.get("failed"),"actual_header_count":len(headers),"preflight_bound_header_count":sum(1 for h in headers.values() if h["preflight_bound"]),"preflight_unbound_headers":[h["path"] for h in headers.values() if not h["preflight_bound"]],"preflight_bound_headers_unchanged":all(h["unchanged"] for h in headers.values() if h["preflight_bound"]),"actual_headers":sorted(headers.values(),key=lambda h:h["path"].lower()),"result":{"passed":row["passed"],"returncode":row["returncode"],"machine":row["machine"],"diagnostic_count":len(errors),"diagnostics":errors,"log":str(log.resolve()),"log_sha256":sha(log),"object":ob},"compile_report":str(report_path.resolve()),"compile_report_sha256":sha(report_path),"driver_log":str((stage/"driver.log").resolve()),"driver_log_sha256":sha(stage/"driver.log")}
(stage/"source-object-header-binding.json").write_text(json.dumps(binding,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"requested":binding["requested"],"passed":binding["passed"],"failed":binding["failed"],"machine":row["machine"],"diagnostics":len(errors),"headers":len(headers),"preflight_bound":binding["preflight_bound_header_count"],"headers_unchanged":binding["preflight_bound_headers_unchanged"],"object_sha256":ob["sha256"] if ob else None,"binding":str(stage/"source-object-header-binding.json")},indent=2))
sys.exit(proc.returncode or (1 if report.get("failed") else 0))