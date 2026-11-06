from pathlib import Path
import json,re,sys
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
j=json.load(open(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8283D2E0.json',encoding='utf8'));lines=j['assembly'].splitlines()
source=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp').read_text(encoding='utf8');start=source.index('    Triangle4::Mask4 IntersectTriangle4Sphere_HackyBurnoutVersion(');brace=source.index('    {',start);sig=source[start:brace]
outs=[f'l{name}{i}' for i in range(4) for name in ('ContactNormal','TriangleNormal','SphereContactPoint','TriangleContactPoint')];gs={0:0,3:'sphere',4:'tri',5:outs[0],6:outs[1],7:outs[2],8:outs[3],9:outs[4],10:outs[5]};stack={0x64+8*i:outs[i+6] for i in range(10)};fields=['mVertex0X','mVertex0Y','mVertex0Z','mVertex1X','mVertex1Y','mVertex1Z','mVertex2X','mVertex2Y','mVertex2Z','mValidMasks','mSurfaceTags','mEdge0Cosigns','mEdge1Cosigns','mEdge2Cosigns'];defined={1};out=[sig+'    {','        // ARTIST 8283D2E0: straight-line VMX operations and full-lane output stores.','        __vector4 v[128];','        v[1] = lInPadding.mV;'];uses=[]
def read(i,addr):
 if i not in defined:uses.append((hex(addr),i))
 return f'v[{i}]'
def setv(i,expr,addr):out.append(f'        v[{i}] = {expr}; // {addr:08X}');defined.add(i)
for line in lines:
 a=int(line.split()[0],16);inst=line.split(None,2)[1];arg=line.split(None,2)[2].split('#')[0].strip() if len(line.split(None,2))>2 else '';w=x360rd.u32(a);op=w>>26
 if inst in ('mflr','bl','b'):continue
 if inst=='li':
  reg,val=arg.split(',');gs[int(reg[1:])]=int(val.strip(),0);continue
 if inst=='lis':gs[11]='constant';continue
 if inst=='addi':continue
 if inst=='lwz':
  r,mem=arg.split(',');m=re.search(r'arg_([0-9A-Fa-f]+)\(r1\)',mem);assert m,(hex(a),line);gs[int(r[1:])]=stack[int(m.group(1),16)];continue
 if inst in ('lvx128','stvx128'):
  vd=((w>>21)&31)|(((w>>2)&3)<<5);ra=(w>>16)&31;rb=(w>>11)&31;av=gs.get(ra);bv=gs.get(rb)
  if ra==1 or rb==1:
   off=bv if ra==1 else av;key=f'spill{abs(off):x}'
   if inst=='stvx128':out.append(f'        __vector4 {key} = {read(vd,a)}; // {a:08X}' if key not in defined else f'        {key} = {read(vd,a)}; // {a:08X}');defined.add(key)
   else:assert key in defined,(a,key);setv(vd,key,a)
  elif inst=='lvx128':
   base=av if av not in (0,None) else bv;off=bv if av not in (0,None) else av
   if base=='sphere':expr='lSphere.mData.mV'
   elif base=='tri':assert isinstance(off,int) and off%16==0 and off//16<len(fields),(hex(a),base,off);expr='lTriangles.'+fields[off//16]+'.mV'
   elif base=='constant':expr='KF_MIN_PLANE_DIST.mV'
   else:raise RuntimeError((hex(a),'load',av,bv))
   setv(vd,expr,a)
  else:
   target=av if isinstance(av,str) else bv;assert target in outs,(hex(a),'store',av,bv);out.append(f'        {target}.mV = {read(vd,a)}; // {a:08X}')
  continue
 vd=((w>>21)&31)|(((w>>2)&3)<<5) if inst.endswith('128') else (w>>21)&31
 va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6) if inst.endswith('128') else (w>>16)&31
 vb=((w>>11)&31)|((w&3)<<5) if inst.endswith('128') else (w>>11)&31
 vc=(w>>6)&31
 D=lambda:read(vd,a);A=lambda:read(va,a);B=lambda:read(vb,a);C=lambda:read(vc,a)
 base=inst.removesuffix('128')
 if base=='vmr':expr=B()
 elif base=='vpermwi':expr=f'__vpermwi({B()}, {((w>>16)&31)|(((w>>6)&7)<<5)})'
 elif base=='vspltw':expr=f'__vspltw({B()}, {va&3})'
 elif base=='vspltisw':n=va&31;n=n-32 if n&16 else n;expr=f'__vspltisw({n})'
 elif base=='vcfsx':expr=f'__vcfsx({B()}, {va})'
 elif base=='vnot':expr=f'__vnor({A()}, {A()})'
 elif base=='vrsqrtefp':expr=f'__vrsqrtefp({B()})'
 elif base in ('vmaddfp','vnmsubfp'):
  expr=f'__{base}({A()}, {B() if inst.endswith("128") else C()}, {D() if inst.endswith("128") else B()})'
 elif base=='vsel':expr=f'__vsel({A()}, {B()}, {D() if inst.endswith("128") else C()})'
 elif base in ('vmulfp','vsubfp','vaddfp','vor','vand','vcmpgefp','vcmpgtfp','vmaxfp','vminfp','vmrglw'):
  expr=f'__{base}({A()}, {B()})'
 else:raise RuntimeError((hex(a),inst,arg,hex(w)))
 setv(vd,expr,a)
print('undefined',uses);assert not uses
out+=['        return Triangle4::Mask4(v[1]);','    }'];Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next/hacky-generated.cpp').write_text('#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"\n#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"\nnamespace CgsGeometric {\nconst VecFloat KF_MIN_PLANE_DIST(0.001f);\n'+'\n'.join(out)+'\n}\n',encoding='utf8')
