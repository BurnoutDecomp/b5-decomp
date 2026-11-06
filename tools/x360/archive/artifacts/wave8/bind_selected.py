"""Bind actual selected diagnostics without inventing a global result."""
from datetime import datetime,timezone
import hashlib,importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
config=json.loads((HERE/"campaign.json").read_text())
spec=importlib.util.spec_from_file_location("native_readonly_binding",HERE.parent/"wave5-delivery/wave5_delivery.py")
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);e.BASE=config["base"]
meta_path=HERE/"native-parallel-wave8-selected-stage1-patch.json";meta=e.read(meta_path)
e.check_current(meta)
report_path=HERE/"selected-stage1/compile-results.json";report,index,artifacts=e.common.report_check(report_path)
assert set(index)==set(meta["selected_sources"]) and len(index)==109
exit_path=HERE/"selected-stage1-exit.json";receipt=e.read(exit_path)
assert receipt["head"]==meta["head"] and receipt["report_sha256"]==e.sha(report_path) and receipt["exit_code"]==1
before=e.read(HERE/"selected-stage1-external-inputs-before.json")["inputs"]
after=e.external_inputs(report)
for key in ("include_trees","submodules","compiler_and_runtime"):assert before[key]==after[key]
prior=e.read(config["baseline_report"]);old={r["source"]:r for r in prior["results"]}
regressions=sorted(s for s,r in index.items() if s in old and old[s]["passed"] and not r["passed"])
gains=sorted(s for s,r in index.items() if r["passed"] and (s not in old or not old[s]["passed"]))
summary={"selected_requested":109,"passed":report["passed"],"failed":report["failed"],"baseline_passes_tested":sum(old.get(s,{}).get("passed",False) for s in index),"regressions":regressions,"selected_gains":gains,"timeouts":[s for s,r in index.items() if r["returncode"]==-1],"NOT_A_GLOBAL_RESULT":True}
result={"head":meta["head"],"base":meta["base"],"patch_sha256":meta["sha256"],"metadata_sha256":e.sha(meta_path),"report":str(report_path),"report_sha256":e.sha(report_path),"exit_receipt_sha256":e.sha(exit_path),"summary":summary,"source_snapshot":meta["source_snapshot"],"artifact_manifest":artifacts,"external_inputs":after,"verifier_sha256":e.sha(__file__),"scope":"Complete actual109selected whole-product gate only. Raw FAIL and19published-pass regressions preserved; no whole inventory, link/image/launch claim.","created_utc":datetime.now(timezone.utc).isoformat()}
e.save_new(HERE/"selected-stage1-terminal-audit.json",result)
print(json.dumps({"audit_sha256":e.sha(HERE/"selected-stage1-terminal-audit.json"),"artifacts":sum(len(v) for v in artifacts.values()),"source_inputs":len(meta["source_snapshot"]["hashes"]),"summary":summary},indent=2))
