"""Current R7 COFF witnesses; owner models unchanged except data/output paths."""
from concurrent.futures import ThreadPoolExecutor,as_completed
from datetime import datetime,timezone
import ast,importlib.util,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
s=importlib.util.spec_from_file_location('r7_world_data_adapter',HERE.parent/'repair-r5/rebind_owner_witnesses.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
m.HERE=HERE;m.SOURCE_REPORT=HERE/'selected-successor1/compile-results.json';m.META=json.loads((HERE/'native-parallel-wave8-r7-selected-successor1-patch.json').read_text());m.RAW=json.loads(m.SOURCE_REPORT.read_text());m.OUT=HERE/'current-world-owner-witnesses'
GATE=json.loads((ART/'parallel/wave8_scene_terra/volume-baseline/FINAL_COHERENT_GATE.json').read_text())
NAMES=[Path(r['source']).name for r in GATE['rows']]
SCRIPTS=ART/'parallel/wave8_scene_terra/scripts'
SPECS=[(n,SCRIPTS/f,NAMES,[])for n,f in [
 ('line-simple','selected_validate_lineseg_simple_record_v2.py'),
 ('line-fatcylinder','selected_validate_lineseg_fatcylinder_controlled_v2.py'),
 ('line-torus-controlled','selected_validate_torus_controlled_v2.py'),
 ('torus-linked-quartic','validate_torus_quartic_composed_v1.py'),
 ('fatcylinder-linked-torus-quartic','validate_fatcylinder_torus_quartic_v2.py'),
 ('fine-empty','selected_validate_fine_empty_v1.py'),
 ('fine-controlled','validate_fine_primitive_controlled_v2.py'),
 ('fine-hit-boundary','validate_fine_primitive_hit_boundary_v4.py'),
 ('fine-two-hits','validate_fine_primitive_twohits_v1.py'),
 ('prism-leaves','nan_validate_prism_leaves_v6_special_table_wip.py'),
 ('prism-dispatch','nan_validate_prism_dispatch_controlled_v6_table_wip.py'),
 ('prism-large','nan_validate_prism_large_v6_128_table_wip.py'),
 ('primitive-batch','nan_validate_primitive_batch_whole_v5.py'),
 ('primitive-dispatch','nan_validate_gp_member_dispatch_1xn_v5.py')]]
for name,file in [('caller-box','validate_box.py'),('caller-triangle','validate_triangle.py'),('caller-triangle-alias','validate_triangle_alias.py'),('caller-primitive','validate_primitive_final.py')]:
 SPECS.append((name,HERE/'caller-repair-terra'/file,['CgsAABBoxBuilder.cpp','CgsPrimitivePairListBuilder.cpp'],[]))
SPECS.append(('ice-gameplay-space',ART/'ice-space-guard-terra/validate.py',['ICECameraSpaceHandler.cpp'],[]))

def run(sp):
 launcher,b=m.prepare(sp);p=launcher.parent/'adapted_model.py';tree=ast.parse(p.read_text())
 class Bind(ast.NodeTransformer):
  def visit_Call(self,n):
   self.generic_visit(n)
   if isinstance(n.func,ast.Attribute)and n.func.attr=='open'and n.args and isinstance(n.args[0],ast.Constant)and isinstance(n.args[0].value,str)and any(c in n.args[0].value for c in 'wxa'):
    n.func.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.func.value],keywords=[])
   return n
 tree=Bind().visit(tree);ast.fix_missing_locations(tree);p.write_text(ast.unparse(tree),encoding='utf-8')
 # Old author filenames are data aliases to the single current whole provider.
 row=next(r for r in m.RAW['results']if Path(r['source']).name=='CgsFineIntersectionTestModule_wSQ1.cpp')
 aliases={'CgsFine_wSQ1':row['object'],'CgsFine-hit-v2':row['object']}
 text=launcher.read_text();before="_r7_previous_object = _r5_object\n_r7_aliases="+repr(aliases)+"\ndef _r5_object(p):\n p=Path(p)\n return Path(_r7_aliases[p.stem]) if p.stem in _r7_aliases else _r7_previous_object(p)\n"
 launcher.write_text(text.replace('exec(compile(',before+'exec(compile(',1),encoding='utf-8')
 b['adapted_AST_sha256']=m.sha(p);b['adapter_sha256']=m.sha(launcher);b['explicit_object_filename_aliases']=aliases
 (launcher.parent/'binding.json').write_text(json.dumps(b,indent=2))
 r=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=1200,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
 log=launcher.parent/'model.log';log.write_bytes(r.stdout+r.stderr)
 reports={str(p):m.sha(p)for p in launcher.parent.glob('*.json')if p.name not in ['binding.json','actual-row-selection.json']}
 issues=[]
 for rp in reports:
  j=json.loads(Path(rp).read_text())
  if isinstance(j,dict):
   for key in ('mismatches','errors','exceptions','bad','failures'):
    if j.get(key):issues.append({'report':rp,'key':key,'value':j[key]})
 result={**b,'returncode':r.returncode,'report_issues':issues,'log':str(log),'log_sha256':m.sha(log),'reports':reports}
 print(sp[0],r.returncode,'report_issues',len(issues),flush=True);return result

if __name__=='__main__':
 m.check_source();assert not m.OUT.exists();m.OUT.mkdir();results=[]
 with ThreadPoolExecutor(max_workers=4)as pool:
  jobs={pool.submit(run,sp):sp[0]for sp in SPECS}
  for job in as_completed(jobs):
   try:results.append(job.result())
   except Exception as e:results.append({'name':jobs[job],'adapter_error':repr(e)})
 m.check_source()
 result={'head':m.META['head'],'raw_report_sha256':m.sha(m.SOURCE_REPORT),'results':results,'source_inputs_rechecked':len(m.META['source_snapshot']['hashes']),
  'scope':'Actual current R7 rebuilt objects with unchanged owner arithmetic/models, only data/object/report locations rebound. Original controls, finite/software estimate domains and old negative reports remain; no hardware/whole runtime extrapolation.',
  'created_utc':datetime.now(timezone.utc).isoformat()}
 with(m.OUT/'completion.json').open('x')as f:json.dump(result,f,indent=2)
 print('COMPLETION',m.sha(m.OUT/'completion.json'),flush=True)
 raise SystemExit(0 if all(r.get('returncode',1)==0 and not r.get('report_issues')for r in results)else 1)
