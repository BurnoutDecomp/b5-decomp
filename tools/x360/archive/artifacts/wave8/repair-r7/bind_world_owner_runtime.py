"""Bind private World provider proofs to freshly rebuilt current objects where exact."""
from collections import Counter
from datetime import datetime,timezone
import ast,hashlib,json,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
loader=HERE.parent/'repair-r6/bind_full_runtime_retention.py';nodes=[n for n in ast.parse(loader.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='coff'];exec(compile(ast.Module(body=nodes,type_ignores=[]),str(loader),'exec'),globals())
gatepath=ART/'parallel/wave8_scene_terra/volume-baseline/FINAL_COHERENT_GATE.json';gate=read(gatepath);reportpath=HERE/'selected-successor1/compile-results.json';report=read(reportpath);current={r['source']:r for r in report['results']};rows=[]
for old in gate['rows']:
 new=current[old['source']];assert new['passed']and sha(old['object'])==old['object_sha256']
 a,_=coff(old['object']);b,_=coff(new['object']);aa=Counter(json.dumps(s,sort_keys=True)for s in a);bb=Counter(json.dumps(s,sort_keys=True)for s in b)
 rows.append({'source':old['source'],'owner_object':old['object'],'owner_object_sha256':sha(old['object']),'current_object':new['object'],'current_object_sha256':sha(new['object']),'exact_all_runtime_sections_and_named_relocations':aa==bb,'owner_sections':len(a),'current_sections':len(b),'only_owner':[json.loads(s)for s in(aa-bb).elements()],'only_current':[json.loads(s)for s in(bb-aa).elements()]})
result={'head':read(HERE/'native-parallel-wave8-r7-selected-successor1-patch.json')['head'],'owner_final_gate':str(gatepath),'owner_final_gate_sha256':sha(gatepath),'current_report_sha256':sha(reportpath),'rows':rows,'exact_objects':sum(r['exact_all_runtime_sections_and_named_relocations']for r in rows),'total_objects':len(rows),'scope':'All36 providers/direct callers actually recompiled on R7 current headers. Exact runtime identity can carry only the corresponding bounded owner proofs; any differing object remains for current-object rerun, never relabeled private-parent compilation. Raw debug/timestamp equality not claimed.','created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'world-owner-current-runtime-binding.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps({'exact':result['exact_objects'],'total':len(rows),'different_sources':[r['source']for r in rows if not r['exact_all_runtime_sections_and_named_relocations']],'sha256':sha(HERE/'world-owner-current-runtime-binding.json')}))
