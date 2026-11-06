from pathlib import Path
import hashlib,json,random,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-sampler-ctor1'
j=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
p=Path(next(r['object'] for r in j['results'] if r['source'].endswith('native_texture_parameters_ctor.cpp')))
o=F.Coff(p,'NativeTextureParametersConstructor');rng=random.Random(0x827E008C)
errors=[];offsets=[0x15e4,0x1648,0x16b0]
for i in range(128):
    image=rng.randbytes(0xca80)
    original=F.cpu(o);original.mem.wr(F.M.S.SELF,image);original.setg(3,F.M.S.SELF)
    lifecycle=[]
    def mutex(c):lifecycle.append(('mutex',c.g(3)&0xffffffff,c.g(4)&0xffffffff,c.g(5)&0xffffffff))
    def job(c):
        lifecycle.append(('job',c.g(3)&0xffffffff,c.g(4)&0xffffffff,
                          [c.mem.rd(F.M.S.SELF+n,0x4c).hex() for n in offsets]))
    original.hooks[0x82B43CA0]=mutex;original.hooks[0x82BCB0C0]=job
    original.run(0x827DFF48)
    assert len([x for x in lifecycle if x[0]=='job'])==32
    assert len([x for x in lifecycle if x[0]=='mutex'])==2
    for n in offsets:
        compiled=F.cpu(o);compiled.mem.wr(F.M.S.SELF,image);compiled.setg(3,F.M.S.SELF+n);compiled.run(o.entry)
        a=original.mem.rd(F.M.S.SELF+n,0x4c);b=compiled.mem.rd(F.M.S.SELF+n,0x4c)
        if a!=b:errors.append(dict(case=i,offset=hex(n),different_bytes=[k for k,(x,y) in enumerate(zip(a,b)) if x!=y],original=a.hex(),compiled=b.hex()))
        assert b[0x45:0x48]==image[n+0x45:n+0x48]
    first_job=next(x for x in lifecycle if x[0]=='job')
    assert first_job[3]==[original.mem.rd(F.M.S.SELF+n,0x4c).hex() for n in offsets]
result=dict(cases=384,errors=errors,original='ARTIST whole Renderer C++ constructor827DFF48, its three inlined Parameters regions and pre-Job snapshots',
    object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
    scope='actual shared-header native constructor versus all76original bytes, including preserved natural padding; original defaults verified present before first of32Job calls',
    limits='foreign RWMutex/Job constructors controlled; only Parameters regions and lifecycle ordering are claimed here, not whole Renderer ctor parity')
(out/'texture-parameters-constructor.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('384 native parameter ctor cases; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
