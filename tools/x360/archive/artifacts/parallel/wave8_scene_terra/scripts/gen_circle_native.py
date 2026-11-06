from pathlib import Path
import json,sys
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
j=json.load(open(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8283DAA8.json',encoding='utf8'));names=['lCentre','lRadiusSquared','lVert0','lVert1','lVert2','lEdge01Dir','lEdge12Dir','lEdge20Dir','lfEdge01Magnitude','lfEdge12Magnitude','lfEdge20Magnitude'];types=['Vector2','VecFloat','Vector2','Vector2','Vector2','Vector2','Vector2','Vector2','VecFloat','VecFloat','VecFloat'];sig='    rw::math::vpu::MaskScalar Intersect2DCircleWithTriangle('+', '.join(t+' '+n for t,n in zip(types,names))+', Vector2& lIntersectPoint)';out=[sig,'    {','        __vector4 v[128];']+[f'        v[{i+1}] = {n}.mV;' for i,n in enumerate(names)];defined=set(range(1,12));uses=[]
def R(i,a):
 if i not in defined:uses.append((hex(a),i))
 return f'v[{i}]'
for line in j['assembly'].splitlines():
 a=int(line.split()[0],16);opname=line.split(None,2)[1];w=x360rd.u32(a)
 if opname in ('blr','.long'):continue
 if opname=='stvx128':vd=((w>>21)&31)|(((w>>2)&3)<<5);assert ((w>>16)&31,(w>>11)&31)==(0,3);out.append(f'        lIntersectPoint.mV = {R(vd,a)}; // {a:08X}');continue
 wide=opname.endswith('128');vd=((w>>21)&31)|(((w>>2)&3)<<5) if wide else (w>>21)&31;va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6) if wide else (w>>16)&31;vb=((w>>11)&31)|((w&3)<<5) if wide else (w>>11)&31;vc=(w>>6)&31;A=lambda:R(va,a);B=lambda:R(vb,a);C=lambda:R(vc,a);D=lambda:R(vd,a);base=opname.removesuffix('128')
 if base=='vmr':expr=B()
 elif base=='vspltw':expr=f'__vspltw({B()}, {va&3})'
 elif base=='vspltisw':n=va&31;n=n-32 if n&16 else n;expr=f'__vspltisw({n})'
 elif base=='vnot':expr=f'__vnor({A()}, {A()})'
 elif base in ('vmaddfp','vnmsubfp'):expr=f'__{base}({A()}, {B() if wide else C()}, {D() if wide else B()})'
 elif base=='vsel':expr=f'__vsel({A()}, {B()}, {D() if wide else C()})'
 elif base in ('vmulfp','vsubfp','vaddfp','vor','vand','vcmpgefp','vcmpgtfp'):expr=f'__{base}({A()}, {B()})'
 else:raise RuntimeError((hex(a),opname,hex(w)))
 out.append(f'        v[{vd}] = {expr}; // {a:08X}');defined.add(vd)
print('undefined',uses);assert not uses
out+=['        return rw::math::vpu::MaskScalar(v[1]);','    }'];Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next/circle-generated.cpp').write_text('#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"\nnamespace CgsGeometric {\n'+'\n'.join(out)+'\n}\n',encoding='utf8')
