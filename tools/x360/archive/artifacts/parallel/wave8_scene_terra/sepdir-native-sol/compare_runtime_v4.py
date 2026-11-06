from pathlib import Path
import importlib.util,json,hashlib
root=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sepdir-native-sol')
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py')
spec=importlib.util.spec_from_file_location('sep_coff_records',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rows={v:json.loads((root/v/'compile-results.json').read_text())['results'][0]for v in ['candidate-v2','candidate-v3','candidate-v4']}
a=m.compare(rows['candidate-v2']['object'],rows['candidate-v3']['object']);b=m.compare(rows['candidate-v3']['object'],rows['candidate-v4']['object'])
report={'v2_to_v3':a,'v3_to_final_v4':b,'objects':{v:{'path':r['object'],'sha256':hashlib.sha256(Path(r['object']).read_bytes()).hexdigest()}for v,r in rows.items()}}
out=root/'runtime-retention-v4.json';assert not out.exists();out.write_text(json.dumps(report,indent=2)+'\n')
print('v2_to_v3',a['common'],'changed',list(a['changed']));print('v3_to_v4',b['common'],'changed',list(b['changed']),'only_a',b['only_a'],'only_b',b['only_b'])
