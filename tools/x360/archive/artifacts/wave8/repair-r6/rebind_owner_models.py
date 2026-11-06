"""Current R6 COFF owner checks; path/report rebinding only, original models preserved."""
from concurrent.futures import ThreadPoolExecutor,as_completed
from datetime import datetime,timezone
import ast,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
spec=importlib.util.spec_from_file_location('r5_data_only_model_adapter',HERE.parent/'repair-r5/rebind_owner_witnesses.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.HERE=HERE;m.SOURCE_REPORT=HERE/'selected-stage1/compile-results.json';m.META=json.loads((HERE/'native-parallel-wave8-r6-selected-stage1-patch.json').read_text());m.RAW=json.loads(m.SOURCE_REPORT.read_text());named=HERE/'named-native-gates/compile-results.json';m.RAW['results']+=json.loads(named.read_text())['results'];m.OUT=HERE/'current-object-witnesses'
def add(name,folder,file,names,args=None):return(name,ART/folder/file,names,args or [])
SPECS=[
 add('xcam-atomics','wave8/xcam-native-terra','compare_atomic_loops.py',['XCamCameraConnector.cpp','XCamDecoder.cpp','XCamEncoder.cpp','XCamVideo.cpp']),
 add('xcam-decoder','wave8/xcam-native-terra','compare_decoder.py',['XCamDecoder.cpp']),
 add('xaudio-capture','wave8/xaudio-xenon-native-terra','compare_capture_final.py',['XAudioXenonRenderer.cpp']),
 add('xaudio-destructor','wave8/xaudio-xenon-native-terra','compare_destructor.py',['XAudioXenonRenderer.cpp']),
 add('socket','wave8/massive-socket-native-terra','compare_socket.py',['MassiveAdClient3Socket.cpp']),
 add('filter-real-chain','audio-fma-r3-terra','validate_butterworth_filter_chain.py',['Butterworth.cpp','HighPassButterworth.cpp','LowPassButterworth.cpp']),
 add('gui-overlay-whole','gui-event-provider-final','validate-overlay.py',['BrnGuiEventTypeDefs.cpp']),
 add('gui-worstcase','gui-event-provider-final','validate-worstcase.py',['BrnGuiEventTypeDefs.cpp']),
 add('gui-soft-linked','gui-event-provider-final','validate-enter-soft-linked.py',['BrnBootLegal.cpp','BrnGuiEventTypeDefs.cpp']),
 add('gui-events','gui-event-provider-final','validate-events.py',['BrnBootLegal.cpp']),
 add('gui-stage6-9','gui-event-provider-final','validate-stage6-9.py',['BrnBootLegal.cpp']),
 add('gui-stage7-8','gui-event-provider-final','validate-stage7-8.py',['BrnBootLegal.cpp']),
 *[add('gui-'+tag,'bootlegal-final-caller','validate-'+tag+'.py',['BrnBootLegal.cpp'])for tag in ['enter','leave','stage','stage0','stage1','stage5']],
 *[add('gui-'+tag,'bootlegal-overlay-retention','validate-'+tag+'.py',['CgsGuiStateInterface_OutputGuiEvent_Inst.cpp'] if tag in ['wrapper','music']else['BrnGuiMovieManager.cpp'])for tag in ['wrapper','music','copy','prepare','receiver']],
 add('physics-methods','native-external-body-root/sol-native','validate_native_v5.py',['ExternalPhysicsBody.cpp','vecfloat.cpp'],['r6-current']),
 add('physics-mass-bits','native-external-body-root/sol-native','validate_mass_bits.py',['ExternalPhysicsBody.cpp']),
 add('physics-part-mass','native-external-body-root','validate_part_mass_final.py',['BrnPhysicalBodyPart_Construct.cpp','ExternalPhysicsBody.cpp','vecfloat.cpp']),
 add('triangle-contract-negative','wave9/gui_triangle_cxx03_luna_2b4','compare_triangle_construct.py',['CgsTriangleBox.cpp']),
]
PARENTS={str(p):m.sha(p)for p in [m.SOURCE_REPORT,named]}
def prepare(spec):
 launcher,binding=m.prepare(spec);model=spec[1];p=launcher.parent/'adapted_model.py';tree=ast.parse(p.read_text())
 class ExtraData(ast.NodeTransformer):
  def visit_Assign(self,n):
   self.generic_visit(n)
   if spec[0]=='triangle-contract-negative' and any(isinstance(t,ast.Name)and t.id=='repo'for t in n.targets):
    n.value=ast.Call(func=ast.Name('Path',ast.Load()),args=[ast.Constant(str(m.WORK))],keywords=[])
   return n
  def visit_Call(self,n):
   self.generic_visit(n)
   if isinstance(n.func,ast.Attribute)and n.func.attr=='open'and n.args and isinstance(n.args[0],ast.Constant)and isinstance(n.args[0].value,str)and any(x in n.args[0].value for x in 'wax'):
    n.func.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.func.value],keywords=[])
   return n
 tree=ExtraData().visit(tree);ast.fix_missing_locations(tree);p.write_text(ast.unparse(tree),encoding='utf-8')
 view=launcher.parent/'actual-row-selection.json';data=json.loads(view.read_text());data.pop('raw_parent',None);data.pop('raw_parent_sha256',None);data['actual_parent_reports']=PARENTS;data['scope']='Data-only unchanged rows from actual selected277 and additional2 real current named providers; not an extra selected/global run.';view.write_text(json.dumps(data,indent=2))
 binding.update(adapted_AST_sha256=m.sha(p),row_selection_sha256=m.sha(view),actual_parent_reports=PARENTS)
 binding['data_only_adaptations'].append({'kind':'writable_open_redirect_and_current_triangle_source_root','arithmetic_or_model_change':False})
 (launcher.parent/'binding.json').write_text(json.dumps(binding,indent=2));return launcher,binding
def run(spec):
 launcher,binding=prepare(spec);r=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=900,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
 log=launcher.parent/'model.log';log.write_bytes(r.stdout+r.stderr)
 result={**binding,'returncode':r.returncode,'log':str(log),'log_sha256':m.sha(log),'reports':{str(p):m.sha(p)for p in launcher.parent.glob('*.json')if p.name not in ['binding.json','actual-row-selection.json']}}
 print(binding['name'],r.returncode,flush=True);return result
if __name__=='__main__':
 m.check_source();assert not m.OUT.exists();m.OUT.mkdir();results=[]
 with ThreadPoolExecutor(max_workers=4)as pool:
  jobs={pool.submit(run,s):s[0]for s in SPECS}
  for task in as_completed(jobs):
   try:results.append(task.result())
   except Exception as e:results.append({'name':jobs[task],'adapter_error':str(e)})
 m.check_source();completion={'head':m.META['head'],'actual_parent_reports':PARENTS,'results':results,'source_inputs_rechecked':6298,'source_metadata_sha256':m.sha(HERE/'native-parallel-wave8-r6-selected-stage1-patch.json'),'scope':'Current-object owner models with only row/object/source/report-path adaptations. Each original model and limits retained; Triangle full scratch negative explicitly expected. No production source edits or hardware/full-game proof.','created_utc':datetime.now(timezone.utc).isoformat()}
 with (m.OUT/'completion.json').open('x')as f:json.dump(completion,f,indent=2)
 print('COMPLETION',m.sha(m.OUT/'completion.json'),flush=True)
 raise SystemExit(1 if any(r.get('returncode',1)!=0 for r in results)else 0)
