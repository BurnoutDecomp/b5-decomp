"""Rebind four additional bounded whole-provider owner models to current COFF."""
from concurrent.futures import ThreadPoolExecutor,as_completed
import importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('world_auxiliary_adapter',HERE/'rebind_world_owner_models.py');w=importlib.util.module_from_spec(s);s.loader.exec_module(w)
w.m.OUT=HERE/'current-world-auxiliary-witnesses';b=w.ART/'parallel/wave8_scene_terra'
SPECS=[('cylinder-feature',b/'gpcylinder-maximum-feature-sol/validate_features_v1.py',['GPCylinder.cpp'],['r7-selected']),
 ('cylinder-intervals',b/'gpcylinder-maximum-feature-sol/validate_intervals_v1.py',['GPCylinder.cpp'],['r7-selected']),
 ('volume-constructors',b/'scripts/validate_volume_ctor_whole_wip_final_v4.py',['BoxVolume.cpp'],[]),
 ('box-volume-bbox',b/'scripts/validate_bbox_whole_wip_final_v4.py',['BoxVolume.cpp'],[])]
if __name__=='__main__':
 w.m.check_source();assert not w.m.OUT.exists();w.m.OUT.mkdir();results=[]
 with ThreadPoolExecutor(max_workers=4)as pool:
  jobs={pool.submit(w.run,sp):sp[0]for sp in SPECS}
  for job in as_completed(jobs):
   try:results.append(job.result())
   except Exception as e:results.append({'name':jobs[job],'adapter_error':repr(e)})
 w.m.check_source();result={'head':w.m.META['head'],'raw_report_sha256':w.m.sha(w.m.SOURCE_REPORT),'results':results,
  'scope':'Fresh current actual whole-provider object executions with owner model domains/controls unchanged. Path/report data adapted only; store scheduling/estimate/special-value/model/temporary-field caveats remain as stated in original reports. No hardware or full collision/gameplay claim.'}
 with(w.m.OUT/'completion.json').open('x')as f:json.dump(result,f,indent=2)
 print('COMPLETION',w.m.sha(w.m.OUT/'completion.json'),flush=True)
 raise SystemExit(0 if all(r.get('returncode',1)==0 and not r.get('report_issues')for r in results)else 1)
