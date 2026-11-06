"""Retry only five data/model-loader failures; preserve first witness run."""
import ast,importlib.util,json,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
s=importlib.util.spec_from_file_location('world_original_adapter',HERE/'rebind_world_owner_models.py');w=importlib.util.module_from_spec(s);s.loader.exec_module(w)
w.m.OUT=HERE/'current-world-owner-retry1'
prepare=w.m.prepare
def data_prepare(sp):
 launcher,b=prepare(sp)
 rows={Path(r['source']).stem:r['object']for r in w.m.RAW['results']if r['passed']}
 aliases={'AABBox':rows['CgsAABBoxBuilder'],'Primitive-final':rows['CgsPrimitivePairListBuilder']}
 text=launcher.read_text();text=text.replace('def _r5_object(p):','_r7_caller_aliases='+repr(aliases)+'\ndef _r5_object(p):',1).replace('    stem=p.stem\n','    stem=p.stem\n    if stem in _r7_caller_aliases:return Path(_r7_caller_aliases[stem])\n',1)
 launcher.write_text(text,encoding='utf-8');b['extra_explicit_object_filename_aliases']=aliases
 if sp[0]=='torus-linked-quartic':
  p=launcher.parent/'adapted_model.py';tree=ast.parse(p.read_text())
  for i,n in enumerate(tree.body):
   if isinstance(n,ast.For) and isinstance(n.target,ast.Name) and n.target.id=='n':
    # The old loader searches ast.Name('CorrectLoadCPU'), but the definition is
    # ast.ClassDef. Select the exact original helper prefix through that class;
    # do not execute its separate historical64-case report writer.
    replacement=ast.parse("_helper_nodes=ast.parse(p.read_text()).body\n_class_end=next(i for i,n in enumerate(_helper_nodes) if isinstance(n,ast.ClassDef) and n.name=='CorrectLoadCPU')\nnodes=_helper_nodes[:_class_end+1]").body
    tree.body[i:i+1]=replacement;break
  else: raise AssertionError('Expected helper-prefix loader not found')
  ast.fix_missing_locations(tree);p.write_text(ast.unparse(tree),encoding='utf-8')
  b['loader_only_correction']='Select unchanged helper prefix through ast.ClassDef CorrectLoadCPU, replacing the ineffective ast.Name sentinel. No arithmetic, CPU instruction decoder, RNG, inputs or expected comparison altered. The initial loader failure remains immutable.'
 return launcher,b
w.m.prepare=data_prepare
if __name__=='__main__':
 w.m.check_source();assert not w.m.OUT.exists();w.m.OUT.mkdir();results=[]
 for sp in w.SPECS:
  if sp[0] in ['caller-box','caller-triangle','caller-triangle-alias','caller-primitive','torus-linked-quartic']:
   results.append(w.run(sp))
 w.m.check_source()
 result={'head':w.m.META['head'],'raw_report_sha256':w.m.sha(w.m.SOURCE_REPORT),'results':results,
   'initial_attempt':str(HERE/'current-world-owner-witnesses/completion.json'),'initial_attempt_sha256':w.m.sha(HERE/'current-world-owner-witnesses/completion.json'),
   'scope':'Only missing filename aliases and helper AST loader selection corrected. Model arithmetic/instructions/inputs/services unchanged; initial adapter failures preserved; current product source unchanged.'}
 with(w.m.OUT/'completion.json').open('x')as f:json.dump(result,f,indent=2)
 print('COMPLETION',w.m.sha(w.m.OUT/'completion.json'),flush=True)
 raise SystemExit(0 if all(r['returncode']==0 and not r['report_issues']for r in results)else 1)
