import json
from pathlib import Path
root=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/fma_native_luna')
p=root/'delivery.json'; d=json.loads(p.read_text(encoding='utf-8'))
v=json.loads((root/'whole_cpp_codegen_validation.json').read_text(encoding='utf-8'))
d['whole_cpp_results']['gpu_perfmon']['object_sha256']=v['reports']['gpu_perfmon']['object_sha256']
p.write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
print(d['whole_cpp_results']['gpu_perfmon']['object_sha256'])
