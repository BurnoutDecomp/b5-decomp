from pathlib import Path
import json,importlib.util
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/game-module-native-sol');p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');spec=importlib.util.spec_from_file_location('gm_coff',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
r=json.loads((OUT/'ice-baseline-v1/compile-results.json').read_text())['results'][0];items=json.loads((OUT/'ice-layout-fields.json').read_text());words=m.Records(r['object']).words('gIceNativeLayout',len(items));d={k:{'hex':hex(v),'decimal':v,'expression':e}for (k,e),v in zip(items,words)};(OUT/'ice-baseline-v1/layout.json').write_text(json.dumps(d,indent=2)+'\n')
for k,v in d.items():print(k,v['hex'])
