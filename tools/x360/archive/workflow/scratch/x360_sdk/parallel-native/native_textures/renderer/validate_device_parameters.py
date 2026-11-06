from pathlib import Path
import hashlib,json,random,struct,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-parameters2'
j=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
p=Path(next(r['object'] for r in j['results'] if r['source'].endswith('native_device_parameters_layout.cpp')))
o=F.Coff(p,'NativeDeviceParametersLayout');c=F.cpu(o);c.setg(3,F.M.S.AUX);c.run(o.entry)
layout=list(struct.unpack('>21I',c.mem.rd(F.M.S.AUX,84)))
expected=[100,*range(0,76,4),28]
assert layout==expected,(layout,expected)
o=F.Coff(p,'NativeDeviceParametersByValue');rng=random.Random(0x8240A8DC);errors=[]
for i in range(128):
    parameters=rng.randbytes(100);tag=rng.getrandbits(32);flag=i%2
    c=F.cpu(o);sp=c.g(1)&0xffffffff;c.setg(3,F.M.S.SELF)
    for chunk in range(7):c.setg(4+chunk,int.from_bytes(parameters[chunk*8:chunk*8+8],'big'))
    c.mem.wr(sp+0x58,parameters[56:])
    F.put(c,sp+0x8c,tag);c.mem.wr(sp+0x97,bytes([flag]))
    c.run(o.entry)
    result=c.mem.rd(F.M.S.AUX,116)
    want=parameters+parameters[12:20]+struct.pack('>II',tag,flag)
    if result!=want:errors.append(dict(index=i,different_bytes=[n for n,(a,b) in enumerate(zip(result,want)) if a!=b]))
report=dict(layout_records=len(layout),layout=layout,by_value_cases=128,errors=errors,
    object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
    scope='actual native COFF layout stores and by-value ABI: seven64-bit GPR aggregate chunks plus44 stack bytes; allocator/tagSP+8C and boolSP+97 match original Renderer call8240A8DC..8FC and Memory callee823FCA38',
    limits='type/marshalling only; Parameters::Initialize body remains owned by immediate worker')
(out/'parameters-layout-abi.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('layout',len(layout),'by-value cases128; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
