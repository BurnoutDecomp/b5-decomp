from pathlib import Path
import json,importlib.util,sys
O=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');sp=importlib.util.spec_from_file_location('rr',p);m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
r=next(x for x in json.loads((O/'replay-type-v1/compile-results.json').read_text())['results']if x['source'].endswith('BrnReplayDirector.cpp'));q=m.Records(r['object'])
print('CTOR_FUNCTIONS',[(n,v['size'],v['relocations'])for n,v in q.functions().items()if n.startswith('??0')]);print('UNRESOLVED',[v[0]for v in q.symbols.values()if v[2]==0])
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import ppcdis
for ea,name in ppcdis.names().items():
 if name=='BrnDirector::DirectorDevTools::Update':
  a=json.loads((Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')/('0x%08X.json'%ea)).read_text())['assembly'];(O/('%08X.asm'%ea)).write_text(a+'\n');print(name,hex(ea));print('\n'.join(x for x in a.splitlines()if '121' in x))
