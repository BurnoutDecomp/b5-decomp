import json,re,collections
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json').read_text(encoding='utf-8'))
pats={
 'std::fma':r"'fma' : is not a member of 'std'|'fma': identifier not found",
 'std::fmaf':r"'fmaf' : is not a member of 'std'|'fmaf': identifier not found",
 'std finite/sign functions':r"fpclassify|copysign|signbit|nearbyint|isfinite",
 'std::snprintf':r"snprintf",
 'C++11 vector::data':r"'data' is not a member of 'std::vector",
 'Vector/Matrix name or member':r"Vector[234]|Matrix44Affine|Vector3Plus|VecFloat|\.mV\.|ambiguous symbol",
 'vmx namespace/helper':r"\bvmx\b|VmxMadd|Multiply.*identifier not found",
 'missing RenderEngine types':r"renderengine::|renderengine' : is not a class or namespace",
 'missing include':r"Cannot open include file",
}
for name,pat in pats.items():
 src=[]; ex=[]
 for x in d['failed_rows']:
  found=[e for e in x['diagnostics'] if re.search(pat,e['message'],re.I)]
  if found: src.append(x['source']); ex.extend((x['source'],e) for e in found[:2])
 print('\n',name,'TUs=',len(set(src)),'occurrences=',sum(len([e for e in x['diagnostics'] if re.search(pat,e['message'],re.I)]) for x in d['failed_rows']))
 for s,e in ex[:5]:print(' ',s,e['code'],e['location'],e['line'],e['message'])
