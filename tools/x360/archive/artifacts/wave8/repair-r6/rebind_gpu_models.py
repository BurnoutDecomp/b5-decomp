"""Rebind the sealed GPU models/helpers to current COFF, with data paths only changed."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime,timezone
import ast,hashlib,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
OUT=HERE/'current-gpu-witnesses';assert not OUT.exists();OUT.mkdir()
RAW=HERE/'selected-stage1/compile-results.json';NAMED=HERE/'named-native-gates/compile-results.json';rows=json.loads(RAW.read_text())['results']+json.loads(NAMED.read_text())['results']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def select(names):
 out=[]
 for n in names:
  hit=[r for r in rows if Path(r['source']).name==n and r['passed']];assert len(hit)==1,(n,hit);out+=hit
 return out
gpu=select(['CgsPerfMonGpuPS3.cpp','CgsPerfMonGpu.cpp','CgsStrStream.cpp']);freq=select(['CgsPerfMonGpu.cpp','CgsPerfMonCpu.cpp'])
views={}
for name,rs in [('gpu',gpu),('frequency',freq)]:
 p=OUT/(name+'-actual-row-selection.json');p.write_text(json.dumps({'results':rs,'state':'complete','scope':'Actual current rows only; data view, not new compilation.','parent_reports':{str(RAW):sha(RAW),str(NAMED):sha(NAMED)}},indent=2));views[name]=str(p)
sdk=ART/'perfmon-gpu-native-r4/sdk-u64tod.obj';assert sdk.is_file() and sdk.read_bytes()[:2]==bytes.fromhex('f201')
objects={Path(r['source']).stem:str(r['object'])for r in gpu+freq};objects['actual-branch-v1']=str(HERE/'named-native-gates/ObservedMainFrequencyBranch.obj');objects['sdk-u64tod']=str(sdk)
helper=ART/'perfmon-gpu-native-r4/gpu_fixture.py'
def contains(n,s):return any(isinstance(a,ast.Constant)and isinstance(a.value,str)and a.value.endswith(s)for a in ast.walk(n))
class Bind(ast.NodeTransformer):
 def visit_BinOp(self,n):
  self.generic_visit(n)
  if isinstance(n.op,ast.Div) and contains(n,'.obj'):return ast.copy_location(ast.Call(func=ast.Name('_r6_object',ast.Load()),args=[n],keywords=[]),n)
  if isinstance(n.op,ast.Div) and contains(n,'branch-extraction.json'):return ast.copy_location(ast.Call(func=ast.Name('Path',ast.Load()),args=[ast.Constant(str(HERE/'named-native-gates/gate-results.json'))],keywords=[]),n)
  return n
 def visit_Call(self,n):
  self.generic_visit(n)
  if isinstance(n.func,ast.Attribute):
   if n.func.attr=='read_text' and contains(n.func.value,'compile-results.json'):n.func.value=ast.Call(func=ast.Name('_r6_report',ast.Load()),args=[n.func.value],keywords=[])
   elif n.func.attr in ['write_text','write_bytes'] or (n.func.attr=='open' and n.args and isinstance(n.args[0],ast.Constant)and isinstance(n.args[0].value,str)and any(c in n.args[0].value for c in 'wxa')):n.func.value=ast.Call(func=ast.Name('_r6_output',ast.Load()),args=[n.func.value],keywords=[])
  return n
def adapted(path,dest):
 tree=Bind().visit(ast.parse(path.read_text(encoding='utf-8-sig')));ast.fix_missing_locations(tree);dest.write_text(ast.unparse(tree),encoding='utf-8')
def run(name,model):
 out=OUT/name;out.mkdir();h=out/'gpu_fixture_adapted.py';p=out/'model_adapted.py';adapted(helper,h);adapted(model,p)
 prelude='''import sys,types,json,hashlib
from pathlib import Path
sys.dont_write_bytecode=True
'''
 prelude+='_r6_objects='+repr(objects)+'\n_r6_views='+repr(views)+'\n_r6_out=Path('+repr(str(out))+')\n'
 prelude+='''def _r6_output(p):return _r6_out/Path(p).name
def _r6_object(p):
 stem=Path(p).stem;hits=[v for k,v in _r6_objects.items()if stem==k or stem.startswith(k+'-')]
 assert len(hits)==1,(str(p),hits)
 return Path(hits[0])
def _r6_report(p):
 return Path(_r6_views['frequency' if 'successor-native-v1' in str(p) else 'gpu'])
'''
 prelude+='sys.path.insert(0,'+repr(str(helper.parent))+')\n'
 prelude+='_helper=types.ModuleType("gpu_fixture");_helper.__dict__.update({k:v for k,v in globals().items()if k.startswith("_r6_")});_helper.__file__='+repr(str(helper))+'\n'
 prelude+='exec(compile(Path('+repr(str(h))+').read_text(encoding="utf-8"),_helper.__file__,"exec"),_helper.__dict__);sys.modules["gpu_fixture"]=_helper\n'
 prelude+='__file__='+repr(str(model))+'\nexec(compile(Path('+repr(str(p))+').read_text(encoding="utf-8"),__file__,"exec"),globals())\n'
 launcher=out/'run.py';launcher.write_text(prelude)
 binding={'name':name,'original_model':str(model),'original_model_sha256':sha(model),'original_helper':str(helper),'original_helper_sha256':sha(helper),'adapted_model_sha256':sha(p),'adapted_helper_sha256':sha(h),'launcher_sha256':sha(launcher),'actual_objects':{v:sha(v)for v in set(objects.values())},'parent_reports':{str(RAW):sha(RAW),str(NAMED):sha(NAMED)},'carried_actual_SDK_u64tod_object':{str(sdk):sha(sdk)},'current_frequency_source_binding':{str(HERE/'named-native-gates/gate-results.json'):sha(HERE/'named-native-gates/gate-results.json')},'data_only_adaptations':['Compile-report views/current object paths','Report destinations on D','Frequency extraction provenance points at fresh current-source check'],'arithmetic_and_models_unchanged':True}
 (out/'binding.json').write_text(json.dumps(binding,indent=2))
 r=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=900,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'});log=out/'model.log';log.write_bytes(r.stdout+r.stderr)
 result={**binding,'returncode':r.returncode,'log':str(log),'log_sha256':sha(log),'reports':{str(p):sha(p)for p in out.glob('*.json')if p.name!='binding.json'}}
 print(name,r.returncode,flush=True);return result
with ThreadPoolExecutor(max_workers=2)as pool:results=list(pool.map(lambda s:run(*s),[('gpu-native',ART/'perfmon-gpu-native-r4/validate_gpu_native.py'),('gpu-frequency-island',ART/'perfmon-gpu-surface-r4/validate_frequency_branch.py')]))
result={'head':json.loads((HERE/'native-parallel-wave8-r6-selected-stage1-patch.json').read_text())['head'],'results':results,'scope':'Current selected/named objects executed by original bounded owner GPU models. Frequency source island remains diagnostic; whole GameModule failure remains visible. No physical GPU/callback/scheduling/frame/link proof.','created_utc':datetime.now(timezone.utc).isoformat()}
with (OUT/'completion.json').open('x')as f:json.dump(result,f,indent=2)
print('COMPLETION',sha(OUT/'completion.json'),flush=True);raise SystemExit(0 if all(r['returncode']==0 for r in results)else 1)
