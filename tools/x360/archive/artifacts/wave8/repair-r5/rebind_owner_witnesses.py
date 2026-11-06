"""Rebind sealed owner models to exact selected R5 COFF; do not alter arithmetic."""
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
import ast
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WORK = Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
SOURCE_REPORT = HERE / 'selected-stage1/compile-results.json'
META = json.loads((HERE/'native-parallel-wave8-r5-selected-stage1-patch.json').read_text())
RAW = json.loads(SOURCE_REPORT.read_text())
OUT = HERE/'current-object-witnesses'

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def check_source():
    assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'rev-parse','HEAD']).decode().strip() == META['head']
    assert not subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'status','--porcelain=v1']).strip()
    for p,h in META['source_snapshot']['hashes'].items(): assert sha(WORK/p)==h,p

BASE = Path('D:/Reverse/BP-X360-Experimental-Artifacts')
SPECS = [
 ('program-initialize',Path('D:/Reverse/programbuffer-terra-build/final2/initialize-witness.py'),['programbuffer.cpp'],[]),
 ('program-caller',Path('D:/Reverse/programbuffer-terra-build/final2/caller-witness.py'),['rwgpfxhelper.cpp'],[]),
 ('bloom-constructor',BASE/'native-bloom-root/validate_constructor_final.py',['BrnPostFxBloom.cpp','vertexdescriptorparameters.cpp','BaseResourceDescriptor.cpp','rwgpfxhelper.cpp'],[]),
 ('bloom-passes',BASE/'native-bloom-root/validate_passes_final.py',['BrnPostFxBloom.cpp'],[]),
 ('survivor',BASE/'native-survivor-root/validate_arithmetic.py',['BrnSurvivor.cpp'],[]),
 ('replay-providers',BASE/'replay_base_sol_r3/validate_provider_closure_v1.py',['BrnReplayBaseSerialiser.cpp','CgsStrStream.cpp','BrnReplayDirectorBridgeSerialiser.cpp','BrnReplayRaceCarEntitySerialiser.cpp','BrnReplayGameModuleSerialiser.cpp','BrnReplayPropEntitySerialiser.cpp','BrnReplayTrafficEntitySerialiser.cpp','BrnReplayEffectsSerialiser.cpp','BrnReplaySoundSerialiser.cpp','BrnReplayGuiModuleSerialiser.cpp'],['r5-selected']),
 ('replay-module',BASE/'replay_base_sol_r3/validate_module_replay_v1.py',['BrnRaceCarEntityModule.cpp','BrnReplayBaseSerialiser.cpp','BrnReplayRaceCarEntitySerialiser.cpp','CgsStrStream.cpp'],['r5-selected']),
 ('replay-director-post-ice-negative',BASE/'replay_base_sol_r3/validate_director_ctor_negative_v1.py',['BrnDirectorModule.cpp','BrnReplayBaseSerialiser.cpp','CgsStrStream.cpp'],['r5-selected']),
 ('audio-encode',BASE/'audio-fma-r3-terra/validate_encode.py',['XasEnc.cpp','Xas1Enc.cpp'],[]),
 ('audio-designer',BASE/'audio-fma-r3-terra/validate_butterworth_final_designer.py',['Butterworth.cpp'],[]),
 ('audio-caller-designer',BASE/'audio-fma-r3-terra/validate_butterworth_caller_designer.py',['Butterworth.cpp','HighPassButterworth.cpp','LowPassButterworth.cpp'],[]),
 ('audio-composed-negative',BASE/'audio-fma-r3-terra/validate_butterworth_final_composed.py',['Butterworth.cpp','HighPassButterworth.cpp','LowPassButterworth.cpp'],[]),
 ('debris',BASE/'wave9/debris_colour_native_luna_2b4/compare_native_object.py',['BrnEffectsDebrisColourRandomiser.cpp'],[]),
 ('dlc',BASE/'dlc-provider-terra/validate.py',['BrnDLCManager.cpp'],[]),
 ('ice-menus',BASE/'ice-geometry-a946-terra/validate_geometry_menus_v1.py',['ICEControllerMenus.cpp'],[]),
 ('ice-controller',BASE/'ice-geometry-a946-terra/validate_controller_construct_v1.py',['ICEController.cpp'],[]),
]

def contains(node,suffix):
    return any(isinstance(n,ast.Constant) and isinstance(n.value,str) and n.value.endswith(suffix) for n in ast.walk(node))

def prepare(spec):
    name,model,names,args = spec
    output=OUT/name;output.mkdir()
    rows=[]
    for n in names:
        matches=[r for r in RAW['results'] if Path(r['source']).name==n and r['passed']]
        if n=='BrnDirectorModule.cpp':
            matches=[r for r in matches if '/DirectorModule/' in r['source']]
        assert len(matches)==1,(name,n,[r['source'] for r in matches])
        rows.append(matches[0])
    local_report=output/'actual-row-selection.json'
    local_report.write_text(json.dumps({**{k:v for k,v in RAW.items() if k!='results'},'results':rows,'requested':len(rows),'passed':len(rows),'failed':0,'raw_parent':str(SOURCE_REPORT),'raw_parent_sha256':sha(SOURCE_REPORT),'scope':'Data-only selection of actual unchanged current rows, not a new compiler run.'},indent=2))
    objects={Path(r['source']).stem:str(Path(r['object'])) for r in rows}
    source=model.read_text(encoding='utf-8-sig')
    tree=ast.parse(source)
    writes={n.func.value.id for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr in ('write_text','write_bytes') and isinstance(n.func.value,ast.Name)}
    changes=[]
    class Bind(ast.NodeTransformer):
        def visit_Constant(self,n):
            if name=='replay-director-post-ice-negative' and n.value==0x38a30:
                changes.append({'kind':'negative_expected_receiver_only','old':'0x38A30 (historical pre-ICE)','new':'0x38A40 (current post-ICE)'})
                return ast.copy_location(ast.Constant(0x38a40),n)
            return n
        def visit_Assign(self,n):
            self.generic_visit(n)
            targets=[t.id for t in n.targets if isinstance(t,ast.Name)]
            if name=='debris' and targets==['repo']:
                n.value=ast.Call(func=ast.Name('Path',ast.Load()),args=[ast.Constant(str(WORK))],keywords=[])
                changes.append({'kind':'source_root','new':str(WORK)})
            elif any(t in writes for t in targets) and contains(n.value,'.json') and not contains(n.value,'compile-results.json'):
                n.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.value],keywords=[])
            return n
        def visit_BinOp(self,n):
            self.generic_visit(n)
            if isinstance(n.op,ast.Div) and contains(n,'compile-results.json'):
                changes.append({'kind':'report_read','expression':ast.unparse(n)})
                return ast.copy_location(ast.Call(func=ast.Name('Path',ast.Load()),args=[ast.Constant(str(local_report))],keywords=[]),n)
            if isinstance(n.op,ast.Div) and contains(n,'.obj'):
                changes.append({'kind':'object_lookup','expression':ast.unparse(n)})
                return ast.copy_location(ast.Call(func=ast.Name('_r5_object',ast.Load()),args=[n],keywords=[]),n)
            return n
        def visit_Call(self,n):
            self.generic_visit(n)
            if isinstance(n.func,ast.Name) and n.func.id=='Path' and n.args and isinstance(n.args[0],ast.Constant) and isinstance(n.args[0].value,str) and n.args[0].value.endswith('.obj'):
                changes.append({'kind':'literal_object_lookup','expression':ast.unparse(n)})
                return ast.copy_location(ast.Call(func=ast.Name('_r5_object',ast.Load()),args=[n],keywords=[]),n)
            if isinstance(n.func,ast.Attribute) and n.func.attr in ('write_text','write_bytes'):
                n.func.value=ast.Call(func=ast.Name('_r5_output',ast.Load()),args=[n.func.value],keywords=[])
                changes.append({'kind':'report_write_redirect','expression':ast.unparse(n.func.value)})
            return n
    tree=Bind().visit(tree);ast.fix_missing_locations(tree)
    adapted=ast.unparse(tree)
    (output/'adapted_model.py').write_text(adapted,encoding='utf-8')
    prelude = '''import sys,json\nfrom pathlib import Path\nsys.dont_write_bytecode=True\nsys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')\n'''
    prelude += 'sys.path.insert(0,'+repr(str(model.parent))+')\n'
    prelude += '_r5_objects='+repr(objects)+'\n_r5_output_dir=Path('+repr(str(output))+')\n'
    prelude += '''def _r5_output(p):
    return _r5_output_dir/Path(p).name
def _r5_object(p):
    p=Path(p)
    stem=p.stem
    found=[v for k,v in _r5_objects.items() if stem==k or stem.startswith(k+'-')]
    assert len(found)==1,(str(p),found)
    return Path(found[0])
'''
    prelude += '__file__='+repr(str(model))+'\nsys.argv='+repr([str(model),*args])+'\n'
    launcher=output/'run.py'
    launcher.write_text(prelude+'exec(compile(Path('+repr(str(output/'adapted_model.py'))+').read_text(encoding="utf-8"),__file__,"exec"),globals())\n',encoding='utf-8')
    binding={'name':name,'model':str(model),'model_sha256':sha(model),'adapted_AST_sha256':sha(output/'adapted_model.py'),'adapter':str(launcher),'adapter_sha256':sha(launcher),'raw_report_sha256':sha(SOURCE_REPORT),'row_selection':str(local_report),'row_selection_sha256':sha(local_report),'objects':{str(r['object']):sha(r['object']) for r in rows},'data_only_adaptations':changes,'limits':'Original owner arithmetic/algorithm/model retained. Only compile-report row selection, current object/source lookup and report destinations rebound. Director negative expected displacement explicitly reflects the separate post-ICE stage; historical pre-ICE evidence remains immutable.'}
    (output/'binding.json').write_text(json.dumps(binding,indent=2))
    return launcher,binding

def run(spec):
    launcher,binding=prepare(spec)
    p=subprocess.run([sys.executable,'-X','utf8',str(launcher)],capture_output=True,timeout=900,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
    directory=launcher.parent
    (directory/'model.log').write_bytes(p.stdout+p.stderr)
    value={**binding,'returncode':p.returncode,'log':str(directory/'model.log'),'log_sha256':sha(directory/'model.log'),'reports':{str(p):sha(p) for p in directory.glob('*.json') if p.name not in ('binding.json','actual-row-selection.json')}}
    print(binding['name'],p.returncode,flush=True)
    return value

if __name__=='__main__':
    check_source();assert not OUT.exists();OUT.mkdir()
    results=[]
    with ThreadPoolExecutor(max_workers=4) as pool:
        jobs={pool.submit(run,s):s[0] for s in SPECS}
        for task in as_completed(jobs):
            try: results.append(task.result())
            except Exception as e: results.append({'name':jobs[task],'runner_error':str(e)})
    check_source()
    result={'head':META['head'],'source_metadata_sha256':sha(HERE/'native-parallel-wave8-r5-selected-stage1-patch.json'),'raw_report_sha256':sha(SOURCE_REPORT),'results':results,'source_inputs_rechecked':len(META['source_snapshot']['hashes']),'created_utc':datetime.now(timezone.utc).isoformat(),'scope':'Current selected COFF bounded owner-model reruns, distinct from full compiler sweep and full runtime proof.'}
    (OUT/'completion.json').write_text(json.dumps(result,indent=2))
    print('COMPLETION',sha(OUT/'completion.json'),flush=True)
    raise SystemExit(1 if any(r.get('returncode',1)!=0 for r in results) else 0)
