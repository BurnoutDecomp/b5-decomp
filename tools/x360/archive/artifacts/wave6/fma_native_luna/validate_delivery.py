import json,hashlib
from pathlib import Path
d=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/fma_native_luna')
delivery=json.loads((d/'delivery.json').read_text(encoding='utf-8'))
v=json.loads((d/'whole_cpp_codegen_validation.json').read_text(encoding='utf-8'))
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert delivery['source_and_object_validation']['sha256']==sha(d/'whole_cpp_codegen_validation.json')
for key in ('inputpads','gpu_perfmon'):
 row=v['reports'][key]; out=delivery['whole_cpp_results'][key]
 assert row['source_sha256']==sha(Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/fma_scalar_luna_526adc')/row['source'])
 for f in ('report','object','assembly','log'):
  assert Path(row[f]).is_file(),(key,f,row[f])
  hashkey=f+'_sha256'
  assert row[hashkey]==sha(row[f]),(key,f)
  assert out[hashkey]==row[hashkey],(key,f,'delivery mismatch')
assert delivery['commits'][0]['sha']=='c21c82b203466f67b39f2f1e8f3cd47204ea5fb9'
assert delivery['commits'][1]['sha']=='a76269d542b8e7fa9c5efc63124b6f093d4bd352'
assert sum(1 for _ in (d/'rounding_witness_results.txt').open())==4
print('DELIVERY_BINDING_PASS: source/object/assembly/log/report hashes, two commits, four edge witnesses')
