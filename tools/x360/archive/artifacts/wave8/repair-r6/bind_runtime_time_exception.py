"""Explain the one __TIME__ delta without changing or hiding raw runtime comparison."""
from datetime import datetime,timezone
import copy,hashlib,json,re,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;WORK=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
rawpath=HERE/'full-selected-runtime-retention.json';raw=read(rawpath)
bad=[r for r in raw['rows']if not r.get('exact_runtime_data_and_named_relocations')]
assert len(bad)==1 and bad[0]['source']=='src/GameSource/Game/BrnAutoTestManager.cpp'
r=bad[0];assert len(r['only_selected_sections'])==len(r['only_full_sections'])==2
a={s['name']:s for s in r['only_selected_sections']};b={s['name']:s for s in r['only_full_sections']};assert set(a)==set(b)=={'.text','.rdata'}
assert a['.text']['data_sha256']==b['.text']['data_sha256']
changes=[(x,y)for x,y in zip(a['.text']['relocations'],b['.text']['relocations'])if x!=y]
assert len(changes)==2 and all(x[:2]==y[:2]for x,y in changes)
oldsym=changes[0][0][2];newsym=changes[0][1][2];assert all(x[2]==oldsym and y[2]==newsym for x,y in changes)
def literal(path,symbol):
 data=Path(path).read_bytes();machine,n,_,syms,ns,opt,_=struct.unpack_from('<HHLLLHH',data);assert machine==0x1f2;strings=syms+ns*18;i=0
 while i<ns:
  at=syms+i*18;name=data[at:at+8]
  if name[:4]==bytes(4):start=strings+struct.unpack_from('<I',name,4)[0];name=data[start:data.index(0,start)]
  else:name=name.split(bytes(1),1)[0]
  value,sec=struct.unpack_from('<Ih',data,at+8)
  if name.decode(errors='replace')==symbol:
   assert sec>0;header=20+opt+(sec-1)*40;size,offset=struct.unpack_from('<II',data,header+16);s=data[offset+value:offset+value+9];assert re.fullmatch(rb'\d\d:\d\d:\d\d\x00',s);return s
  i+=1+data[at+17]
 raise AssertionError(symbol)
old=literal(r['selected_object'],oldsym);new=literal(r['full_object'],newsym)
assert a['.rdata']['size']==b['.rdata']['size']==9
assert hashlib.sha256(old).hexdigest()==a['.rdata']['data_sha256'] and hashlib.sha256(new).hexdigest()==b['.rdata']['data_sha256']
aa=copy.deepcopy(a['.text']);aa['relocations']=[[(newsym if z==oldsym else z)for z in x]for x in aa['relocations']];assert aa==b['.text']
aa=copy.deepcopy(a['.rdata']);aa['data_sha256']=b['.rdata']['data_sha256'];assert aa==b['.rdata']
source=WORK/r['source'];lines=[{'line':i,'text':s.strip()}for i,s in enumerate(source.read_text().splitlines(),1)if '__TIME__'in s];assert len(lines)==1
assert subprocess.check_output(['git','--no-optional-locks','-C',str(WORK),'diff','04ed276f','HEAD','--',r['source']])==b''
runtime_by_obj={x['selected_object']:x for x in raw['rows']if 'selected_object'in x};tested=set()
for p in [HERE/'current-object-witnesses/completion.json',HERE/'current-gpu-witnesses/completion.json']:
 for m in read(p)['results']:
  tested.update(m.get('objects',m.get('actual_objects',{})))
matched=[];diagnostic=[]
for p in sorted(tested):
 if p in runtime_by_obj:
  assert runtime_by_obj[p]['exact_runtime_data_and_named_relocations'],p;matched.append(p)
 else:
  assert Path(p).name in ['ObservedMainFrequencyBranch.obj','sdk-u64tod.obj'],p;diagnostic.append(p)
result={'head':raw['head'],'raw_comparison':str(rawpath),'raw_comparison_sha256':sha(rawpath),'raw_exact_all_flag_preserved':raw['all_selected_runtime_sections_retained'],'exact_runtime_object_count':len(raw['rows'])-1,'total_runtime_objects_compared':len(raw['rows']),'compiler_time_only_exceptions':[{'source':r['source'],'source_sha256':sha(source),'R5_source_unchanged':True,'source_use':lines,'selected_object':r['selected_object'],'selected_object_sha256':r['selected_object_sha256'],'full_object':r['full_object'],'full_object_sha256':r['full_object_sha256'],'selected_time':old[:-1].decode(),'full_time':new[:-1].decode(),'instruction_bytes_identical':True,'only_changed_literal_bytes':9,'only_changed_named_relocations':changes}],'all_owner_product_objects_exactly_retained_in_full':True,'owner_product_object_count':len(matched),'owner_product_objects':matched,'separately_scoped_diagnostic_or_SDK_objects':diagnostic,'scope':'Raw223-object comparison remains false for exact-all.222 are exact. One untouched consumer logs compiler __TIME__; its instruction bytes match and only9-byte time literal/two symbolic data relocations differ. Every product object used by the bounded owner models is outside that exception and exact. No product/source/byte normalization, rebuild or claim that all223 runtime images are identical.','created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'runtime-time-exception-supplement.json').open('x')as f:json.dump(result,f,indent=2)
print(json.dumps({'exact':len(raw['rows'])-1,'total':len(raw['rows']),'selected_time':old.decode(),'full_time':new.decode(),'all_tested_product_objects_exact':True,'tested_product_objects':len(matched),'sha256':sha(HERE/'runtime-time-exception-supplement.json')}))
