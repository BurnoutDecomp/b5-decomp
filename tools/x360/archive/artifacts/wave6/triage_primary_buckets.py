import json,re,collections
from pathlib import Path
d=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json').read_text(encoding='utf-8'))
reg={'src/GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.cpp','src/GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp'}
layout={'src/GameSource/Game/BrnGameModule.cpp','src/GameSource/Main/BrnMainX360.cpp','src/GameSource/GameState/Offences/BrnDriveThruManager.cpp'}
soft_layout={'src/GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.cpp','src/SDKs/Packages/ICE/ICECameraSpaceHandlerCtor.cpp'}
def bucket(x):
 s=x['source']; e=x['diagnostics'][0] if x['diagnostics'] else {'code':'','message':''}; c=e['code']; m=e['message']; t=(s+' '+m).lower()
 if s in reg:return 'known wave6 regression'
 if s in layout:return 'hard source-layout gate'
 if s in soft_layout:return 'layout/trait guard unevaluable from unresolved type or toolchain trait'
 if c=='C1083':return 'missing / unsupported include'
 if re.search(r"not a member of 'std'|not a member of 'std::|not found in namespace 'std'",m,re.I):return 'CRT / std-library math or text API unavailable in XDK'
 if re.search(r"\bfma\b|\bfmaf\b|fpclassify|copysign|signbit|nearbyint|isfinite|\bvmx\b|\bVector[234]\b|Matrix44Affine|Quaternion|mV\.",m,re.I):return 'math / vector spelling or ABI'
 if c in {'C2059','C2143','C2146','C2147','C2153','C2334','C2447','C2552','C2601','C2466','C2550','C3646','C3409'} or re.search(r'initializer list|syntax error|illegal use of this type|local function definitions|unknown override specifier|noreturn',m,re.I):return 'C++03 syntax / unsupported language feature'
 if c in {'C2660','C2666','C2733','C2511','C2084','C2352','C2370','C2371','C2374','C2086','C2614','C2761','C2885'}:return 'signature / declaration / duplicate conflict'
 if re.search(r'not a class or namespace|undefined type|incomplete type|not a member of .renderengine|not a member of .Cgs|unknown size',m,re.I) or c in {'C2027','C2036','C2070','C2079','C2061','C4430','C2653'}:return 'native type / provider closure'
 if c in {'C2065','C3861','C2064'}:return 'missing identifier / helper / call target'
 if c in {'C2228','C2227','C2275','C2440','C2562'}:return 'typed expression / aggregate / member access'
 if c=='C2177':return 'constant width / native integer literal'
 if c=='C2872':return 'ambiguous type-name / namespace ownership'
 if c=='C2118':return 'compile-time array/size assertion'
 return 'other compiler diagnostic'
counts=collections.defaultdict(list)
for x in d['failed_rows']: counts[bucket(x)].append(x)
print('PARTITION',sum(map(len,counts.values())))
for k,v in sorted(counts.items(),key=lambda kv:(-len(kv[1]),kv[0])):
 print('\n',len(v),k)
 for x in v[:6]:
  e=x['diagnostics'][0] if x['diagnostics'] else {}
  print(' ',x['source'],e.get('code'),e.get('message'))
 if len(v)>6: print('  +',len(v)-6,'rows')
