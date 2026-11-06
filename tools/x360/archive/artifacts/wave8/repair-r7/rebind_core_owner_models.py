"""R7 current-object owner checks, changing only data/object/report paths."""
from concurrent.futures import ThreadPoolExecutor,as_completed
from datetime import datetime,timezone
import ast,importlib.util,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
spec=importlib.util.spec_from_file_location('r7_data_only_adapter',HERE.parent/'repair-r5/rebind_owner_witnesses.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.HERE=HERE;m.SOURCE_REPORT=HERE/'selected-successor1/compile-results.json';m.META=json.loads((HERE/'native-parallel-wave8-r7-selected-successor1-patch.json').read_text());m.RAW=json.loads(m.SOURCE_REPORT.read_text());m.OUT=HERE/'current-core-owner-witnesses'
def s(name,folder,file,names,args=None):return(name,ART/folder/file,names,args or [])
SPECS=[s('completion','wave8/gamestate-c03-r5-terra','compare_completion_final.py',['BrnProgressionManager_Completion.cpp']),s('highscore','wave8/gamestate-c03-r5-terra','compare_highscore_final.py',['BrnGameStateStreetManager_wC_00.cpp']),s('camera-flags','camera-flags-terra','validate_original.py',['ICECamera.cpp']),s('hud-three-methods','race-hud-native-root','validate_mugshot_native_array.py',['BrnRaceMainHudState.cpp']),s('road-distance','roadrule-native-terra','compare-final.py',['BrnRoadRuleComponent.cpp'])]
for name in ['float','line','start_empty','start_spawn','stop']:SPECS.append(s('effects-junkyard-'+name,'effects-native-terra','compare_junkyard_'+name+'.py',['EffectsModule.cpp']))
SPECS += [s('effects-replay-contact','effects-native-terra','compare_replay_contact.py',['EffectsModule.cpp']),s('effects-jump-sparks','effects-native-terra','compare_jump_sparks.py',['JumpStateMachine.cpp'])]
def run(sp):
 launcher,b=m.prepare(sp);p=launcher.parent/'adapted_model.py';tree=ast.parse(p.read_text())
 class Bind(ast.NodeTransformer):
  def visit_Call(self,n):
   self.generic_visit(n)
   if isinstance(n.func,ast.Attribute)and n.func.attr=='open'and n.args and isinstance(n.args[0],ast.Constant)and isinstance(n.args[0].value,str)and any(c in n.args[0].value for c in 'wxa'):n.func.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.func.value],keywords=[])
   return n
 tree=Bind().visit(tree);ast.fix_missing_locations(tree);p.write_text(ast.unparse(tree),encoding='utf-8');b['adapted_AST_sha256']=m.sha(p);(launcher.parent/'binding.json').write_text(json.dumps(b,indent=2))
 r=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=900,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'});log=launcher.parent/'model.log';log.write_bytes(r.stdout+r.stderr);result={**b,'returncode':r.returncode,'log':str(log),'log_sha256':m.sha(log),'reports':{str(p):m.sha(p)for p in launcher.parent.glob('*.json')if p.name not in ['binding.json','actual-row-selection.json']}};print(sp[0],r.returncode,flush=True);return result
if __name__=='__main__':
 m.check_source();assert not m.OUT.exists();m.OUT.mkdir();results=[]
 with ThreadPoolExecutor(max_workers=4)as pool:
  jobs={pool.submit(run,sp):sp[0]for sp in SPECS}
  for job in as_completed(jobs):
   try:results.append(job.result())
   except Exception as e:results.append({'name':jobs[job],'adapter_error':str(e)})
 m.check_source();result={'head':m.META['head'],'source_metadata_sha256':m.sha(HERE/'native-parallel-wave8-r7-selected-successor1-patch.json'),'raw_report_sha256':m.sha(m.SOURCE_REPORT),'results':results,'scope':'Original owner algorithms/models with only current COFF/report row and output path adaptations; bounded scopes/services remain. No source changes or hardware/full runtime claims.','created_utc':datetime.now(timezone.utc).isoformat()}
 with(m.OUT/'completion.json').open('x')as f:json.dump(result,f,indent=2)
 print('COMPLETION',m.sha(m.OUT/'completion.json'),flush=True);raise SystemExit(0 if all(r.get('returncode',1)==0 for r in results)else 1)
