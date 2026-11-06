"""Focused R165 original and compiled PPC request witnesses; not a link/runtime test."""
import hashlib,json,re,runpy,sys,struct
from pathlib import Path
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
m=runpy.run_path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r156-layout-linked/verify_layout_linked.py')['m'];out=Path(__file__).resolve().parent; obj=out/'BrnGameDataModule-r165.obj'; asm=obj.with_suffix('.asm').read_text(errors='replace'); data=obj.read_bytes()
def body(name):
 match=re.search(r'; Begin code for function: (\?'+name+r'@GameDataModule@BrnResource@@\S+)',asm);assert match,name
 symbol=match.group(1);c,r=m['collect'](obj,symbol);return symbol,c,r
def calls(rels):return [m['object_symbol'](data,i)[0] for o,i,k in rels if k==6]
def literals(rels):
 found=set()
 for o,i,k in rels:
  if k==16:
   name,_,_=m['object_symbol'](data,i)
   if name.startswith('??_C@'):found.add(m['symbol_literal'](data,i))
 return found
expected={'ProcessGetICEMovieRequest':(0x8266fd10,bytes.fromhex('7d8802a6485991d19421fee07c7f1b78')),'ProcessUnloadTrafficLanesRequest':(0x82670d30,bytes.fromhex('7d8802a6485981a19421fec07cb92b78')),'ProcessUnloadAILanesRequest':(0x82670e40,bytes.fromhex('7d8802a6485980919421fec07cb92b78')),'ProcessUnloadPropInstancesRequest':(0x82670f50,bytes.fromhex('7d8802a648597f759421fe203d608200'))}
rows={}
for name,(va,raw) in expected.items():
 assert x360rd.rd(va,len(raw))==raw
 symbol,code,rel=body(name); cs=calls(rel);lit=literals(rel)
 assert any('AddEvent@?$VariableEventQueue' in x for x in cs)
 assert bytes.fromhex('38a00004 38c00018') in code if 'ICEMovie' in name else bytes.fromhex('38a00003 38c00090') in code
 assert any('IndexedPool' in x for x in cs) and not any('PushIndex' in x for x in cs)
 if 'ICEMovie' in name:assert any('CgsIDConvertToString' in x for x in cs) and any('HashString@ID@CgsResource' in x for x in cs)
 else:assert any('SetFileName@BundleLoaderEvent' in x for x in cs)
 if 'TrafficLanes' in name:assert {'B5Traffic.bndl','Invalid asset type for traffic lanes\n'}<=lit and bytes.fromhex('38a01466') in code
 if 'AILanes' in name:assert {'AI.dat','Invalid asset type for AI lanes\n'}<=lit and bytes.fromhex('38a01482') in code
 if 'PropInstances' in name:
  assert {'Props/Instances/TRK_UNIT','_PropInstances.bundle','Invalid asset type for prop instances\n','PRP_INST_','luZoneId < KU_MAX_ZONES','strncmp(lacResourceName, "PRP_INST_", KU_STRING_INDEX_OF_ZONE_NUMBER) == 0'}<=lit
  assert all(bytes.fromhex(x) in code for x in ('38a014a4','38a014af','38a014b1','38a014b4','2b1c01f4'))
  assert any('strncmp'==x for x in cs) and any('atoi'==x for x in cs)
 if 'ICEMovie' not in name:assert r'..\..\..\GameSource\Resource/BrnGameDataModule.cpp' in lit
 rows[name]={'artist_va':hex(va),'symbol':symbol,'bytes':len(code),'calls':cs,'literals':sorted(lit)}
for dispatch,names in [('ProcessGetGameDataEvent',['ProcessGetICEMovieRequest']),('ProcessUnloadGameDataEvent',['ProcessUnloadTrafficLanesRequest','ProcessUnloadAILanesRequest','ProcessUnloadPropInstancesRequest'])]:
 sym,c,r=body(dispatch);cs=calls(r)
 for n in names:assert any(x.startswith('?'+n+'@GameDataModule@BrnResource@@') for x in cs)
 assert not any('DeferredGameDataRequest' in x for x in cs)
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result={'state':'pass','base':'4da663341e39dd897eecce5642712dbd7b2c4b5e','tree':'4db7a0e79c33f4ebc6ed8058ee2e63edbc4b3cd3','object_sha256':sha(obj),'assembly_sha256':sha(obj.with_suffix('.asm')),'compile_report_sha256':sha(out/'compile-results.json'),'handlers':rows,'scope':'Raw ARTIST entry anchors and focused compiled PPC witnesses; response continuations inspected in source and ARTIST; no linked PE or execution.'}
(out/'PROOF.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS',sha(out/'PROOF.json'))
