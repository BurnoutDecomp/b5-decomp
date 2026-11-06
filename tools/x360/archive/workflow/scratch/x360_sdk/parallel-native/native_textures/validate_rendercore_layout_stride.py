from pathlib import Path
import hashlib,json,random,struct,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F

out=parent/'b5-decomp/build/x360/parallel/native_textures/rendercore3'
report=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
paths={Path(r['source']).name:Path(r['object']) for r in report['results'] if r['passed']}
layout=F.Coff(paths['native_rendercore_layout.cpp'],'NativeRenderCoreLayout')
c=F.cpu(layout);c.setg(3,F.M.S.AUX);c.run(layout.entry)
observed=list(struct.unpack('>41I',c.mem.rd(F.M.S.AUX,41*4)))
expected=[24,0,4,8,12,16,20,32,24,40,32,36,8,16,0,4,8,12,
          48,52,48,28,32,28,20,24,12,68,0,4,8,20,40,84,8,12,76,80,
          2761657,2892709,0xffffffff]
assert observed==expected,(observed,expected)
conversions=[]
for name in ['NativeVertexBufferBase','NativePixelBufferBase']:
    o=F.Coff(paths['native_rendercore_layout.cpp'],name)
    for a in [0,F.M.S.SELF,F.M.S.AUX,0x83123450,0xfffffffc]:
        c=F.cpu(o);c.setg(3,a);c.run(o.entry);assert c.g(3)&0xffffffff==a
    conversions.append(dict(symbol=name,cases=5,adjustment=0))

o=F.Coff(paths['vertexformat.cpp'],'?VertexFormatGetStride@')
rng=random.Random(0x82B61310)
case_values=[85126,1583238,1712218,1712262,1712263,1712291,1712474,1712518,1712519,
             1712547,1712730,1712774,1712775,1712803,1712986,1712992,1713030,1713031,
             1713059,1713062,2760839,2760848,2760849,2761095,2761104,2761105,2761351,
             2761360,2761361,2761607,2761616,2761617,2761657,2891865,2891938,2892121,
             2892194,2892377,2892450,2892633,2892639,2892706,2892709,2916513,2916769,
             2917025,2917281,2917284]
values=sorted({0,1,0x7fffffff,0x80000000,0xffffffff,*[v+d for v in case_values for d in [-1,0,1]]})
values += [rng.getrandbits(32) for _ in range(128)]
errors=[]
for value in values:
    results=[]
    for address in [0x82B61310,o.entry]:
        c=F.cpu(o);c.setg(3,value);c.run(address);results.append(c.g(3)&0xffffffff)
    if results[0]!=results[1]:errors.append(dict(value=hex(value),original=results[0],compiled=results[1]))
result=dict(layout_records=len(expected),layout_observed=observed,base_conversions=conversions,
            stride_cases=len(values),errors=errors,
            objects=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                     for p in [paths['native_rendercore_layout.cpp'],paths['vertexformat.cpp']]],
            scope='actual native COFF layout stores/base conversions; actual compiled stride function versus ARTIST instructions for every recognised word and its neighbours plus signed extremes/random words',
            limits='instruction interpreter; no physical GPU or linked game')
(out/'layout-stride-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('layout',len(expected),'base conversions10; stride',len(values),'cases; errors',len(errors))
print(errors[:5])
raise SystemExit(bool(errors))
