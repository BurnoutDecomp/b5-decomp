"""Retry only failed report-destination binding; preserve the first run/log."""
import ast,hashlib,importlib.util,json,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('r5_models',HERE/'rebind_owner_witnesses.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.check_source();m.OUT=HERE/'current-object-witnesses-output-v2';m.OUT.mkdir()
config=next(s for s in m.SPECS if s[0]=='bloom-passes');launcher,binding=m.prepare(config)
p=launcher.parent/'adapted_model.py';tree=ast.parse(p.read_text())
class OutputOnly(ast.NodeTransformer):
 def visit_Call(self,n):
  self.generic_visit(n)
  if isinstance(n.func,ast.Attribute) and n.func.attr=='open' and n.args and isinstance(n.args[0],ast.Constant) and isinstance(n.args[0].value,str) and any(c in n.args[0].value for c in 'wxa'):
   n.func.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.func.value],keywords=[])
  return n
tree=OutputOnly().visit(tree);ast.fix_missing_locations(tree);p.write_text(ast.unparse(tree),encoding='utf-8')
binding['adapted_AST_sha256']=m.sha(p);binding['data_only_adaptations'].append({'kind':'report_open_x_redirect','reason':'Initial actual416 comparisons passed; only final report open(x) targeted an existing historical report, causing FileExistsError. Original report and failed-adapter log preserved.'})
(launcher.parent/'binding.json').write_text(json.dumps(binding,indent=2))
r=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=180);(launcher.parent/'model.log').write_bytes(r.stdout+r.stderr)
m.check_source();result={**binding,'returncode':r.returncode,'initial_binding_completion_sha256':m.sha(HERE/'current-object-witnesses/completion.json'),'reports':{str(p):m.sha(p) for p in launcher.parent.glob('*.json')}}
(m.OUT/'completion.json').write_text(json.dumps(result,indent=2));print(r.returncode,m.sha(m.OUT/'completion.json'));raise SystemExit(r.returncode)
