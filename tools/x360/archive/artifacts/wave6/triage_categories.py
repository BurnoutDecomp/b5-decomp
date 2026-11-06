import json,re,collections
from pathlib import Path
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json')
d=json.loads(p.read_text(encoding='utf-8'))
print('OLD_PASS_NOW_FAIL:')
for s in d['transitions']['old_pass_now_fail']['sources']: print(' ',s)
print('NEW_SOURCES_PASS:')
for s in d['transitions']['new_sources_pass']['sources']: print(' ',s)
print('OLD_FAIL_NOW_PASS sample/count:',d['transitions']['old_fail_now_pass']['count'])
rows=d['failed_rows']
code_rows=collections.defaultdict(set); loc_rows=collections.defaultdict(set); phrase_rows=collections.defaultdict(set)
patterns={
'layout/static guard':r'static.?assert|assert.*sizeof|sizeof|size mismatch|size guard|offset.*(mismatch|expected)|expected.*size|class size',
'C++03/language syntax':r'lambda|initializer list|constexpr|auto\b|=default|noexcept|syntax error|illegal use of this type|non-aggregates|too many initializers|local function definitions',
'host/stdlib API':r'not a member of .std|Cannot open include file: .(array|atomic|chrono|mutex|type_traits|intrin\.h|winsock2\.h)|vector<_Ty>|does not name a type',
'math API/VMX':r'fpclassify|fmaf?|copysign|signbit|nearbyint|\bvmx\b|not a member of .rw::math::vpu::Vector',
'missing provider/type':r'undefined type|not a class or namespace name|not a member of .renderengine|not a member of .Cgs|identifier not found|undeclared identifier|cannot open include file',
}
for x in rows:
 for e in x['diagnostics']:
  code_rows[e['code']].add(x['source']); loc_rows[e['location']].add(x['source'])
  for k,pat in patterns.items():
   if re.search(pat,e['message'],re.I): phrase_rows[k].add(x['source'])
print('CODES BY FAILED TU')
for k,v in sorted(code_rows.items(),key=lambda kv:(-len(kv[1]),kv[0])): print(len(v),k)
print('PHRASE COVERAGE (overlap intentional)')
for k,v in phrase_rows.items(): print(len(v),k)
print('LAYOUT_MATCHES:')
for x in rows:
 matches=[e for e in x['diagnostics'] if re.search(patterns['layout/static guard'],e['message'],re.I)]
 if matches: print(x['source'],x['log'],' || '.join(f"{e['location']}:{e['line']} {e['code']} {e['message']}" for e in matches[:5]))
