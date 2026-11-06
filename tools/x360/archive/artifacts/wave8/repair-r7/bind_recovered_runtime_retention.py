"""Bind current selected witnesses to completed R7 diagnostic inventory objects."""
from collections import Counter
from datetime import datetime,timezone
import ast,hashlib,json,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
loader=HERE.parent/'repair-r6/bind_full_runtime_retention.py'
nodes=[n for n in ast.parse(loader.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='coff']
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(loader),'exec'),globals())
selected_path=HERE/'selected-successor1/compile-results.json';full_path=HERE/'full-recovered/compile-results.json'
selected=read(selected_path);full=read(full_path);assert full['state']=='complete'and len(full['results'])==3615
assert full['provenance']['retained_recorded_rows']==1700 and full['provenance']['newly_compiled_missing_rows']==1915
index={r['source']:r for r in full['results']};rows=[]
plan=read(HERE/'intake-plan-approved-successor1.json');owned=set(plan['owned_CPP_sources'])
for s in selected['results']:
 if not s['passed']:continue
 t=index[s['source']]
 if not t['passed']:
  rows.append({'source':s['source'],'selected_pass_lost_in_full':True});continue
 a,_=coff(s['object']);b,_=coff(t['object']);aa=Counter(json.dumps(x,sort_keys=True)for x in a);bb=Counter(json.dumps(x,sort_keys=True)for x in b)
 rows.append({'source':s['source'],'selected_object':s['object'],'selected_object_sha256':sha(s['object']),'full_object':t['object'],'full_object_sha256':sha(t['object']),
  'runtime_sections':len(a),'owned_changed_product':s['source']in owned,'exact_runtime_data_and_named_relocations':aa==bb,
  'only_selected_sections':[json.loads(x)for x in(aa-bb).elements()],'only_full_sections':[json.loads(x)for x in(bb-aa).elements()]})
mainrow=next(r for r in full['results']if Path(r['source']).name=='BrnMainX360.cpp');main=None
if mainrow['passed']:
 _,defs=coff(mainrow['object']);main={'source':mainrow['source'],'object':mainrow['object'],'object_sha256':sha(mainrow['object']),'machine':'01F2','strong_external_main':[d for d in defs if d['name']=='main']};assert len(main['strong_external_main'])==1
result={'head':read(HERE/'native-parallel-wave8-r7-full-recovered-patch.json')['head'],'selected_raw_sha256':sha(selected_path),'full_raw_sha256':sha(full_path),
 'rows':rows,'all_selected_runtime_sections_retained':all(r.get('exact_runtime_data_and_named_relocations',False)for r in rows),
 'all_owned_changed_product_objects_exact':all(r.get('exact_runtime_data_and_named_relocations',False)for r in rows if r.get('owned_changed_product')),
 'native_main_provider':main,'recovery_provenance':full['provenance'],
 'scope':'Actual272 selected-pass products compared with diagnostic inventory objects from original1700 valid rows plus1915 missing-source recovery. Every non-debug section name/size/flags/data and relocation offset/type/target is compared. Diagnostic probes are separately bound by unchanged actual771 include files/7 tools; they are not product inventory rows. Raw mismatches are preserved for exact scoped examination, never normalized broadly. No new behavior/link/image/launch proof.',
 'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'full-selected-runtime-retention.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps({'rows':len(rows),'exact':sum(r.get('exact_runtime_data_and_named_relocations',False)for r in rows),'owned_exact':result['all_owned_changed_product_objects_exact'],
 'different':[r['source']for r in rows if not r.get('exact_runtime_data_and_named_relocations')],'main_provider':main is not None,'sha256':sha(HERE/'full-selected-runtime-retention.json')}))
