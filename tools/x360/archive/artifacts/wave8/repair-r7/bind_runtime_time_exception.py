"""Preserve and explain the sole unowned RenderMetrics __TIME__ difference."""
from datetime import datetime,timezone
import ast,copy,hashlib,json,re,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
# Reuse only the read-only COFF literal decoder, not the R6 top-level assertions.
helper=HERE.parent/'repair-r6/bind_runtime_time_exception.py';nodes=[n for n in ast.parse(helper.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='literal'];exec(compile(ast.Module(body=nodes,type_ignores=[]),str(helper),'exec'),globals())
rawpath=HERE/'full-selected-runtime-retention.json';raw=read(rawpath);bad=[r for r in raw['rows']if not r.get('exact_runtime_data_and_named_relocations')]
assert len(bad)==1 and bad[0]['source']=='src/GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.cpp'
r=bad[0];assert not r['owned_changed_product']
assert len(r['only_selected_sections'])==len(r['only_full_sections'])==2
a={x['name']:x for x in r['only_selected_sections']};b={x['name']:x for x in r['only_full_sections']};assert set(a)==set(b)=={'.text','.rdata'}
assert a['.text']['data_sha256']==b['.text']['data_sha256']
changes=[(x,y)for x,y in zip(a['.text']['relocations'],b['.text']['relocations'])if x!=y]
assert len(changes)==2 and all(x[:2]==y[:2]for x,y in changes)
oldsym=changes[0][0][2];newsym=changes[0][1][2];assert all(x[2]==oldsym and y[2]==newsym for x,y in changes)
old=literal(r['selected_object'],oldsym);new=literal(r['full_object'],newsym)
assert a['.rdata']['size']==b['.rdata']['size']==9
assert hashlib.sha256(old).hexdigest()==a['.rdata']['data_sha256']and hashlib.sha256(new).hexdigest()==b['.rdata']['data_sha256']
aa=copy.deepcopy(a['.text']);aa['relocations']=[[(newsym if z==oldsym else z)for z in x]for x in aa['relocations']];assert aa==b['.text']
aa=copy.deepcopy(a['.rdata']);aa['data_sha256']=b['.rdata']['data_sha256'];assert aa==b['.rdata']
source=WORK/r['source'];lines=[{'line':i,'text':s.strip()}for i,s in enumerate(source.read_text().splitlines(),1)if '__TIME__'in s];assert len(lines)==1
assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'diff','ef64014612b510bffa6fa17bebe91461bd95a0ab','HEAD','--',r['source']])==b''
assert raw['all_owned_changed_product_objects_exact']
runtime_by_obj={x['selected_object']:x for x in raw['rows']if'selected_object'in x};provided=set()
for p in [HERE/'current-core-owner-witnesses/completion.json',HERE/'current-world-owner-witnesses/completion.json',HERE/'current-world-owner-retry1/completion.json',HERE/'current-world-auxiliary-witnesses/completion.json']:
 for model in read(p)['results']:
  if model.get('returncode')==0:provided.update(model.get('objects',{}))
event=read(HERE/'current-constants-and-camera-flags.json');provided.add(event['EventInfo_current_object'])
for p in provided:assert runtime_by_obj[p]['exact_runtime_data_and_named_relocations'],p
result={'head':raw['head'],'raw_comparison':str(rawpath),'raw_comparison_sha256':sha(rawpath),'raw_exact_all_flag_preserved':raw['all_selected_runtime_sections_retained'],
 'exact_runtime_object_count':271,'total_runtime_objects_compared':272,'all79_changed_owner_product_objects_exact':True,
 'compiler_time_only_exceptions':[{'source':r['source'],'source_sha256':sha(source),'R6_source_unchanged':True,'source_use':lines,
  'selected_object':r['selected_object'],'selected_object_sha256':r['selected_object_sha256'],'full_object':r['full_object'],'full_object_sha256':r['full_object_sha256'],
  'selected_time':old[:-1].decode(),'full_time':new[:-1].decode(),'instruction_bytes_identical':True,'only_changed_literal_bytes':9,'only_changed_named_relocations':changes}],
 'all_current_owner_model_product_inputs_exactly_retained_in_inventory':True,'bound_product_input_count':len(provided),'bound_product_inputs':sorted(provided),
 'separately_bound_camera_probe_sha256':event['current_camera_probe_sha256'],
 'scope':'Strict raw272-object equality remains false.271 exact; sole unowned RenderMetrics consumer emits a different9-byte compiler __TIME__ literal and2 named data references, with identical instruction bytes and all other sections. Every79 changed owner product and each product object provided to current bounded owner models is exact in final inventory. This is distinct from prior R6 AutoTest time exception and from RootGUI additional whole-COFF metadata differences; the latter were NOT timestamp-only. No source/product/byte normalization or rebuild.',
 'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'runtime-time-exception-supplement.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps({'exact':271,'total':272,'old_time':old.decode(),'full_time':new.decode(),'model_product_inputs_exact':len(provided),'sha256':sha(HERE/'runtime-time-exception-supplement.json')}))
