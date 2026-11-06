"""Bind successful R11 native/header/provider gates before the authorized full."""
from datetime import datetime,timezone
import hashlib,json,struct,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):return {'path':str(p),'sha256':sha(p)}
C=read(HERE/'campaign.json');plan=read(C['current_plan']);a=read(HERE/'selected-stage1-terminal-audit.json');r=read(a['report']);named=read(HERE/'named-native-gates/gate-results.json');proof=read(HERE/'integrated-getparameters-proof.json')
assert a['head']==named['head']==proof['head']and r['passed']==r['requested']==8 and r['failed']==0
assert not a['summary']['regressions']and a['summary']['R10_prior_passes_in_selection']==1
assert all(x['gate_satisfied']for x in named['rows'])and named['emitted_facts']['ResourceContracts']==[12,0,4,8,32,24,36,32,12]
for row in named['rows']:
 for p,h in row['artifacts'].items():assert sha(p)==h,p
assert proof['exact48']and proof['code_relocations']==[]
def symbols(path):
 b=Path(path).read_bytes();machine,n,_,sp,ns,opt,_=struct.unpack_from('<HHLLLHH',b);assert machine==0x1f2;st=sp+18*ns;i=0;out=[]
 while i<ns:
  at=sp+i*18;raw=b[at:at+8]
  if raw[:4]==bytes(4):start=st+struct.unpack_from('<I',raw,4)[0];name=b[start:b.index(0,start)].decode()
  else:name=raw.split(bytes(1),1)[0].decode()
  value,sec,typ,storage,aux=struct.unpack_from('<IhHBB',b,at+8)
  if storage==2:out.append({'name':name,'section':sec,'value':value})
  i+=1+aux
 return out
contracts={name:[]for name in ['?GetParameters@IndexBuffer@renderengine@@QBAXAAUParameters@12@@Z','?Xbox2CheckPhysicalMemoryFlags@IndexBuffer@renderengine@@QAAXXZ']}
for row in r['results']:
 for s in symbols(row['object']):
  if s['name']in contracts:contracts[s['name']].append({'source':row['source'],'object':row['object'],'object_sha256':sha(row['object']),**s})
for name,rows in contracts.items():
 assert len([x for x in rows if x['section']>0])==1 and any(x['section']==0 for x in rows),(name,rows)
registration=next(x for x in r['results']if x['source']==plan['registration_consumer']);assert registration['passed']
head=subprocess.check_output(['git','--no-optional-locks','-C',C['workspace'],'rev-parse','HEAD']).decode().strip();assert head==a['head']
assert not subprocess.check_output(['git','--no-optional-locks','-C',C['workspace'],'status','--porcelain=v1']).strip()
for p,h in a['source_snapshot']['hashes'].items():assert sha(Path(C['workspace'])/p)==h,p
result={'head':head,'all_required_gates_satisfied':True,'selected_requested':8,'selected_passed':8,'selected_failed':0,'registration_prior_PASS_retained':True,'R10_selected_regressions':[],
 'changed_or_new_native_sources':7,'new_native_provider_CPP':1,'new_genuine_headers':2,'native_inventory':3616,'source_input_hashes':6302,
 'native_header_positive_probes':5,'expected_independent_C2118_negatives':2,'resource_facts':named['emitted_facts']['ResourceContracts'],
 'provider_caller_contracts':contracts,'selected_terminal_audit':pin(HERE/'selected-stage1-terminal-audit.json'),'native_header_gates':pin(HERE/'named-native-gates/gate-results.json'),
 'exact_integrated_getparameters':pin(HERE/'integrated-getparameters-proof.json'),'initial_binder_new_source_accounting_failure_preserved':pin(HERE/'selected-binder-initial-failure.json'),
 'scope':'Current8 whole native product objects plus real header ABI/provider-caller symbols and exact48-byte method. Not a whole-TU/runtime/game/link claim. MemoryFlags remains explicitly not byte-identical; descriptorFC is not MaterialState object size.',
 'native_guards':'Preserve allRCE/World/Render/Main and existing typed-layout facts; HUD7920/7940 and WorldEntity79E0/79F0 independent negatives remain.',
 'authorization':'Root exact2 resource-only pair. One fresh3616/6302 full only after these positive/stable gates; no reuse-only global completion,optional link/owner suite or later source.',
 'formal_review':False,'publication_allowed':False,'goal':'ACTIVE','created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'selected-gates-completion.json').open('x')as f:json.dump(result,f,indent=2)
print('R11_GATES_COMPLETE',sha(HERE/'selected-gates-completion.json'),flush=True)
