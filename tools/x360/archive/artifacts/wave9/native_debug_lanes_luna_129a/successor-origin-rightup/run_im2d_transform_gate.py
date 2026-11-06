from pathlib import Path
import hashlib, json, os, struct, subprocess, sys
repo=Path(r"D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a")
canonical=Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
sdk=canonical/"build/x360/sdk-6534.4/portable/XDK"
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\native_debug_lanes_luna_129a\successor-origin-rightup")
stage=art/"im2d-transform-native-gate"
rel="src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.cpp"
pre=json.loads((art/"successor-preflight.json").read_text(encoding="utf-8"))
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""): h.update(chunk)
    return h.hexdigest()
def git(*args): return subprocess.check_output(["git",*args],cwd=repo,text=True).strip()
head=git("rev-parse","HEAD")
status=subprocess.check_output(["git","status","--porcelain"],cwd=repo,text=True).splitlines()
expected={str((repo/rel).resolve()).lower()}
changed={str((repo/line[2:].strip()).resolve()).lower() for line in status if line and line[:2]!="??"}
untracked=[line for line in status if line.startswith("??")]
if head!=pre["parent"] or changed!=expected or untracked:
    raise SystemExit(f"successor compile scope mismatch: head={head}, changed={changed}, untracked={untracked}")
if stage.exists(): raise SystemExit(f"gate stage already exists: {stage}")
stage.mkdir(parents=True)
tmp=art/"temp";tmp.mkdir(exist_ok=True)
source_before=sha(repo/rel)
command=[sys.executable,"-X","utf8","-B",str(repo/"tools/build_x360.py"),"--sdk",str(sdk),
         "--out",str(stage),"--jobs","1","--compiler-arg=/showIncludes","--source",rel]
for vendor in ("EABase/include/Common","EASTL/include","EAThread/include","renderware/include",
               "PPMalloc/include","coreallocator/include","dirtysdk/include","zlib/include","lua/src"):
    command.append("--compiler-arg=/I"+str(canonical/"vendor"/vendor))
(stage/"command.json").write_text(json.dumps(command,indent=2)+"\n",encoding="utf-8")
env=dict(os.environ);env["TEMP"]=str(tmp);env["TMP"]=str(tmp)
proc=subprocess.run(command,cwd=repo,env=env,stdin=subprocess.DEVNULL,capture_output=True,
                    text=True,encoding="utf-8",errors="replace")
(stage/"driver.log").write_text(proc.stdout+proc.stderr,encoding="utf-8")
source_after=sha(repo/rel)
if source_before!=source_after: raise SystemExit("source changed during compile")
report_path=stage/"compile-results.json"
if not report_path.is_file(): raise SystemExit(f"build produced no report (exit {proc.returncode})")
report=json.loads(report_path.read_text(encoding="utf-8"))
if report.get("requested")!=1 or len(report.get("results",[]))!=1:
    raise SystemExit(f"expected exactly one compiled source: {report}")
row=report["results"][0]
log=Path(row["log"]);logtext=log.read_text(encoding="utf-8",errors="replace")
errors=[line for line in logtext.splitlines() if "error " in line]
pre_headers={str(Path(h["path"]).resolve()).lower():h["sha256"] for h in pre["headers"]}
headers={}
for line in logtext.splitlines():
    marker="Note: including file:"
    if line.startswith(marker):
        p=Path(line[len(marker):].strip()).resolve()
        if not p.is_file(): raise SystemExit(f"include path missing: {p}")
        key=str(p).lower();digest=sha(p)
        headers[key]={"path":str(p),"sha256":digest,"preflight_sha256":pre_headers.get(key),
                      "preflight_bound":key in pre_headers,
                      "unchanged":pre_headers.get(key)==digest if key in pre_headers else None}
obj=Path(row["object"]);objbinding=None
if obj.is_file():
    data=obj.read_bytes();machine=struct.unpack("<H",data[:2])[0] if len(data)>=2 else None
    objbinding={"path":str(obj.resolve()),"size":len(data),"machine":machine,
                "machine_hex":f"0x{machine:04X}" if machine is not None else None,
                "verified_ppc32be_coff":machine==0x01F2,"sha256":sha(obj)}
result={"source":row["source"],"passed":row["passed"],"returncode":row["returncode"],
        "machine":row["machine"],"diagnostic_count":len(errors),"diagnostics":errors,
        "log":str(log.resolve()),"log_sha256":sha(log),"object":objbinding}
binding={"base_head":head,"target":"xbox360-ppc-be","sdk":str(sdk.resolve()),"jobs":1,
         "command":command,"driver_returncode":proc.returncode,"driver_log":str((stage/"driver.log").resolve()),
         "driver_log_sha256":sha(stage/"driver.log"),"compile_report":str(report_path.resolve()),
         "compile_report_sha256":sha(report_path),"source_parent_blob":pre["source"]["parent_blob"],
         "source_preflight_sha256":pre["source"]["pre_sha256"],
         "source_sha256_before_gate":source_before,"source_sha256_after_gate":source_after,
         "source_unchanged_during_gate":source_before==source_after,
         "requested":report.get("requested"),"passed":report.get("passed"),"failed":report.get("failed"),
         "actual_header_count":len(headers),
         "preflight_bound_header_count":sum(1 for h in headers.values() if h["preflight_bound"]),
         "preflight_unbound_headers":[h["path"] for h in headers.values() if not h["preflight_bound"]],
         "preflight_bound_headers_unchanged":all(h["unchanged"] for h in headers.values() if h["preflight_bound"]),
         "actual_headers":sorted(headers.values(),key=lambda h:h["path"].lower()),"result":result}
out=stage/"source-object-header-binding.json"
out.write_text(json.dumps(binding,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"head":head,"requested":binding["requested"],"passed":binding["passed"],
                  "failed":binding["failed"],"machine":row["machine"],"diagnostics":len(errors),
                  "actual_headers":len(headers),"preflight_bound":binding["preflight_bound_header_count"],
                  "headers_unchanged":binding["preflight_bound_headers_unchanged"],
                  "object_sha256":objbinding["sha256"] if objbinding else None,
                  "binding":str(out)},indent=2))
sys.exit(proc.returncode or (1 if report.get("failed") else 0))