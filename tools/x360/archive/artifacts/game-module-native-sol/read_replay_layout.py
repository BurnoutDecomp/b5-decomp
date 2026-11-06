from pathlib import Path
import importlib.util,json
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');sp=importlib.util.spec_from_file_location('rr',p);m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
r=json.loads((OUT/'replay-type-v1/compile-results.json').read_text())['results'][0];items=json.loads((OUT/'replay-layout-fields.json').read_text());w=m.Records(r['object']).words('gReplayNativeLayout',len(items));d={k:hex(v)for(k,_),v in zip(items,w)};(OUT/'replay-type-v1/layout.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2))
