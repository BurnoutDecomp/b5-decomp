from pathlib import Path
import hashlib,json,math,random,struct,sys
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-sun-retail-917')
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'native1/compile-results.json').read_text())['results'] if r['passed']}
probe=F.Coff(paths['sun_retail_abi.cpp'],'?Projection@SunRetailAbi@')
sun=F.Coff(paths['BrnSunCorona.cpp'],'?ComputeSunPositionOnScreen@')
bundle=F.Bundle([probe,sun]);bundle.select('?Projection@SunRetailAbi@')
callee=F.address(bundle,'?ComputeSunPositionOnScreen@BrnSunCorona@')
images={base:bytes(data) for base,data in bundle.images.items()}
SELF,AUX=F.M.S.SELF,F.M.S.AUX
SUN,MATRIX,VIEW,DIRECTION=SELF+0x1000,AUX+0x1000,AUX+0x2000,AUX+0x2100
rng=random.Random(0x82405e04);errors=[];windows=[]
for index in range(128):
    state=bytearray(rng.randbytes(0x80));struct.pack_into('>f',state,0x34,[0.0,0.5,-1.0,2.0][index%4])
    matrix=struct.pack('>16f',*[rng.uniform(-2,2) for _ in range(16)])
    view=struct.pack('>3fI',*[rng.uniform(-1000,1000) for _ in range(3)],0xa5a50000|index)
    direction=struct.pack('>3fI',*[rng.uniform(-3,3) for _ in range(3)],0x5a5a0000|index)
    if index%8==0:direction=bytes(12)+direction[12:]
    both=[]
    for compiled in (False,True):
        bundle.images={base:bytearray(data) for base,data in images.items()}
        if not compiled:
            for (oi,sec),base in bundle.bases.items():
                if oi!=0:continue
                for at,ix,kind in probe.relocations(sec):
                    if probe.symbols[ix][0].startswith('?ComputeSunPositionOnScreen@BrnSunCorona@'):
                        assert kind==6
                        raw=struct.unpack_from('>I',probe.data,probe.sections[sec][4]+at)[0]
                        struct.pack_into('>I',bundle.images[base],at,(raw&0xfc000003)|((0x823fd798-(base+at))&0x03fffffc))
        c=F.cpu(bundle);c.mem.wr(SUN,state);c.mem.wr(MATRIX,matrix);c.mem.wr(VIEW,view);c.mem.wr(DIRECTION,direction)
        sp=c.g(1)&0xffffffff;c.mem.wr(sp-0x3000,bytes([index&255])*0x3000)
        for reg in range(3,11):c.setg(reg,0x9600000000000000|(index<<8)|reg)
        c.setg(3,SUN);c.setg(4,MATRIX);c.setg(5,VIEW);c.setg(6,DIRECTION)
        seen=[];execute=c.exec_vector
        def observe(pc,word,mnemonic):
            if pc==(callee if compiled else 0x823fd798):
                pointer=c.g(4)&0xffffffff
                window=dict(this=c.g(3)&0xffffffff,matrix=pointer,matrix_alignment=pointer%16,
                            matrix_is_copy=pointer!=MATRIX,matrix_bytes=c.mem.rd(pointer,64).hex(),
                            v1=struct.pack('>4I',*c.vr[1]).hex(),v2=struct.pack('>4I',*c.vr[2]).hex())
                assert window['this']==SUN and window['matrix_is_copy'] and not window['matrix_alignment']
                assert window['matrix_bytes']==matrix.hex() and window['v1']==view.hex() and window['v2']==direction.hex()
                seen.append(window)
            return execute(pc,word,mnemonic)
        c.exec_vector=observe
        c.run(bundle.entry)
        assert len(seen)==1
        both.append((c.mem.rd(SUN,0x80),c.mem.rd(MATRIX,64),c.mem.rd(VIEW,16),c.mem.rd(DIRECTION,16),seen))
    if both[0]!=both[1]:errors.append(dict(case=index,windows=[x[-1] for x in both],sun=[x[0][0x48:0x54].hex() for x in both]))
    if index<4:windows.append(both[1][-1][0])
result=dict(cases=128,errors=errors,windows=windows,
    objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},
    scope='Actual emitted typed pointer-source caller -> complete actual Sun projection versus the same emitted caller -> original823FD798. Exact mangled declaration link, r3 receiver/r4 independent aligned64-byte Matrix44 value copy, raw v1/v2 payloads including W poison, preserved source data and all Sun outputs compare. No GPR/VR argument adaptation exists.',
    limits='Caller is a genuine-header ABI probe for missing source RendererVisibility; original actual caller window82405E04 is separately recorded. It is not a recovered whole RendererVisibility/Render algorithm or game-launch claim.')
report=out/'native1/projection-caller-retail-comparison.json';assert not report.exists();report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(cases=128,errors=len(errors),windows=windows[:1],first_error=errors[:1]),indent=2));raise SystemExit(bool(errors))
