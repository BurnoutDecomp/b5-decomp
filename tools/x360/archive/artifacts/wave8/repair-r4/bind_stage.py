"""Bind actual Wave8 terminal source/artifacts; diagnostic failures remain immutable."""
import argparse,hashlib,importlib.util,json,sys
from datetime import datetime,timezone
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser();parser.add_argument("--name",required=True);args=parser.parse_args()
config=json.loads((HERE/"campaign.json").read_text())
spec=importlib.util.spec_from_file_location("actual_native_terminal",HERE.parent.parent/"wave5-delivery/wave5_delivery.py")
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e);e.BASE=config["base"]
meta_path=HERE/("native-parallel-wave8-r4-"+args.name+"-patch.json");meta=e.read(meta_path);e.check_current(meta)
report_path=HERE/args.name/"compile-results.json";report,index,artifacts=e.common.report_check(report_path)
assert set(index)==set(meta["selected_sources"])
exit_path=HERE/(args.name+"-exit.json");terminal=e.read(exit_path)
assert terminal["head"]==meta["head"] and terminal["report_sha256"]==e.sha(report_path)
assert terminal["exit_code"]==(1 if report["failed"] else 0)
prior=e.read(config["baseline_report"]);old={r["source"]:r for r in prior["results"]}
assert e.sha(config["baseline_report"])==config["baseline_report_sha256"]
external=e.external_inputs(report);before=e.read(HERE/(args.name+"-external-inputs-before.json"))["inputs"]
for key in ("include_trees","submodules","compiler_and_runtime"):assert external[key]==before[key]
if meta["selection"]=="full":
 assert len(index)==len(meta["native_inventory"])==3616 and set(index)==set(meta["native_inventory"])
 assert set(index)=={p for p in e.common.inventory() if Path(p).suffix.lower() in (".c",".cpp")}
 summary=e.compare(report,prior)
 assert not summary["removed_sources"] and not summary["new_sources"]
 manifest_path=Path(config["baseline_artifact_manifest"]);assert e.sha(manifest_path)==config["baseline_artifact_manifest_sha256"]
 manifest=e.read(manifest_path);e.verify_manifest(prior,manifest["artifact_manifest"])
 assert manifest["report_sha256"]==config["baseline_report_sha256"]
 published_prior=e.read(config["published_report"])
 published_manifest=e.read(config["published_artifact_manifest"])
 assert e.sha(config["published_report"])==config["published_report_sha256"]
 assert e.sha(config["published_artifact_manifest"])==config["published_artifact_manifest_sha256"]
 e.verify_manifest(published_prior,published_manifest["artifact_manifest"])
 published_comparison=e.compare(report,published_prior)
else:
 summary={"selected_requested":len(index),"passed":report["passed"],"failed":report["failed"],"regressions":[s for s,r in index.items() if old.get(s,{}).get("passed") and not r["passed"]],"timeouts":[s for s,r in index.items() if r["returncode"]==-1],"NOT_A_GLOBAL_RESULT":True}
assert len(meta["source_snapshot"]["hashes"])==6299
result={"published_comparison":published_comparison if meta["selection"]=="full" else None,"head":meta["head"],"base":meta["base"],"patch_sha256":meta["sha256"],"metadata":str(meta_path),"metadata_sha256":e.sha(meta_path),"report":str(report_path),"report_sha256":e.sha(report_path),"exit_receipt":str(exit_path),"exit_receipt_sha256":e.sha(exit_path),"summary":summary,"artifact_manifest":artifacts,"source_snapshot":meta["source_snapshot"],"external_inputs":external,"receipts_sha256":meta["receipt_sha256"],"goal":"ACTIVE; strict link/image/launch unproved","verifier_sha256":e.sha(__file__),"created_utc":datetime.now(timezone.utc).isoformat()}
e.save_new(HERE/(args.name+"-terminal-audit.json"),result)
print(json.dumps({"audit_sha256":e.sha(HERE/(args.name+"-terminal-audit.json")),"head":meta["head"],"report_sha256":e.sha(report_path),"requested":report["requested"],"passed":report["passed"],"failed":report["failed"],"regressions":len(summary["regressions"]),"new_gains":len(summary.get("new_passes",[])),"published_regressions":len(published_comparison["regressions"]) if meta["selection"]=="full" else None,"artifacts":sum(len(v) for v in artifacts.values()),"full":meta["selection"]=="full"},indent=2))
