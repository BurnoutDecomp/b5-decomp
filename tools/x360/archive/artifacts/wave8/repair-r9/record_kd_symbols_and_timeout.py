"""Record actual new symbols and preserve the inconclusive raw-utility timeout."""
from datetime import datetime,timezone
import ast,hashlib,json,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
symbol='?KU_NUM_STACK_ENTRIES@KdTreeLineQuery@collision@rw@@2IB'
source=HERE/'recheck_kd_native_link.py';nodes=[n for n in ast.parse(source.read_text()).body if isinstance(n,ast.FunctionDef)and n.name=='constant'];exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
C=read(HERE/'campaign.json');old=read(C['baseline_report']);new=read(HERE/'targeted-stage1/compile-results.json');names=['KdTreeLineQuery.cpp','CgsFineIntersectionTestModule_wSQ1.cpp'];rows={}
for label,report in [('R8',old),('R9',new)]:
 rows[label]=[{'object':r['object'],'object_sha256':sha(r['object']),**constant(r['object'])}for name in names for r in report['results']if Path(r['source']).name==name]
 assert len(rows[label])==2
assert sum(not r['COMDAT']for r in rows['R8'])==1 and all(r['COMDAT']for r in rows['R9'])
proofpath=ART/'native-kdtree-constant-root/constant-link-audit.json';proof=read(proofpath);assert sha(proofpath)=='b7e882d434c7cc64bf0003aa24151092168157a369a9f45846b34228ab7576e2'
for value in proof['results'].values():assert sha(value['log'])==value['log_sha256']
sdk=Path(new['sdk']);raw=sdk/'bin/win32/link.exe';prepared=Path(C['canonical'])/'build/x360/tool-host/link.exe';rsp=HERE/'kd-link-baseline_R8/link.rsp';assert rsp.exists()
assert not list((HERE/'kd-link-baseline_R8').glob('*.exe'))
result={'head':read(HERE/'repair-r9-receipts.json')['head'],'actual_constant_symbols':rows,'same_native_header_value32':True,'actual_new_both_matching_COMDAT':True,
 'existing_Root_strict_link_proof':str(proofpath),'existing_Root_strict_link_proof_sha256':sha(proofpath),'existing_baseline_duplicates':1,'existing_successor_duplicates':0,
 'existing_strict_proof_scope':'Prepared canonical tool-host linker, two actual native objects; both expected1120 unrelated unresolved/no EXE. Historical evidence,not relabeled a new R9 link.',
 'new_optional_link_attempt':{'unified_handle':15934,'python_pid':31340,'native_link_pid':35264,'script':str(source),'script_sha256':sha(source),'state':'ACTUAL subprocess180-second timeout; no linker exit observed',
  'python_script_exit':1,'raw_SDK_utility_path':str(raw),'raw_SDK_utility_sha256':sha(raw),'response_file':str(rsp),'response_sha256':sha(rsp),'EXE_emitted':False,
  'native_log_not_captured':'The wrapper raised TimeoutExpired before its post-return log write. Do not infer a native returncode or duplicate result from this attempt.',
  'tool_path_error':'Raw SDK bin/win32/link.exe was used instead of the prepared canonical build/x360/tool-host/link.exe. Root identified VC80 R6034 modal risk; this is not asserted as a separately observed diagnosis.',
  'correct_prepared_utility_path':str(prepared),'prepared_utility_sha256':sha(prepared),'rerun':False},
 'Root_resolution':'Do not rerun optional narrow link. Finish using actual new COMDAT32 symbol evidence plus existing Root strict1-to0 proof; preserve timeout explicitly. No additional tests/source changes.',
 'scope':'Compile/coverage unaffected. No FORCE,weak alias,declspec workaround,full-game link,image or launch claim. No raw SDK utility rerun.',
 'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'kd-symbols-and-link-timeout-supplement.json').open('x')as f:json.dump(result,f,indent=2)
print('KD_SYMBOLS_BOUND',sha(HERE/'kd-symbols-and-link-timeout-supplement.json'),'new_COMDAT32_2of2 no new link claim',flush=True)
