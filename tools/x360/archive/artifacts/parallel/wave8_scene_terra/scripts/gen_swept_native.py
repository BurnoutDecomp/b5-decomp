from pathlib import Path
import json,re,sys
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
root=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra');source=(root/'src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.cpp').read_text(encoding='utf8');sigkey='    Triangle4::Mask4 IntersectTriangle4SweptSphere(';a=source.index(sigkey);sig=source[a:source.index('    {',a)]
outs=[f'l{name}{i}' for i in range(4) for name in ('ContactNormal','TriangleNormal','SphereContactPoint','TriangleContactPoint')];fields=['mVertex0X','mVertex0Y','mVertex0Z','mVertex1X','mVertex1Y','mVertex1Z','mVertex2X','mVertex2Y','mVertex2Z','mValidMasks','mSurfaceTags','mEdge0Cosigns','mEdge1Cosigns','mEdge2Cosigns'];
def generate(addr,is_helper):
 lines=json.load(open(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x'+addr+'.json',encoding='utf8'))['assembly'].splitlines();out=[];uses=[];defined=set(range(1,14)) if is_helper else set();stackdefined=set(range(0xe0,0x130,16)) if is_helper else set();gs={0:0,1:'stack',3:'out0' if is_helper else 'sphere',4:'out1' if is_helper else 'tri',**({} if is_helper else {i+5:outs[i] for i in range(6)})};args={0x54+8*i:outs[i+6] for i in range(10)}
 def read(i,a):
  if i not in defined:uses.append((hex(a),i))
  return f'v[{i}]'
 def write(i,expr,a):out.append(f'        v[{i}] = {expr}; // {a:08X}');defined.add(i)
 def stk(x):return f'stack{x if x>=0 else "m"+str(-x)}'
 def stackread(off,a):
  if off not in stackdefined:uses.append((hex(a),'stack',hex(off)))
  return stk(off)
 for line in lines:
  a=int(line.split()[0],16);opname=line.split(None,2)[1];arg=line.split(None,2)[2].split('#')[0].strip() if len(line.split(None,2))>2 else '';w=x360rd.u32(a)
  if opname in ('mflr','stwu','blr','b') or (not is_helper and a>=0x8283fd40):continue
  if opname=='bl':
   if a==0x8283f3a0:
    values=[read(i,a) for i in range(1,14)]+[stackread(i,a) for i in range(0xe0,0x130,16)]
    assert gs[3]==('stack',0x140) and gs[4]==('stack',0x150)
    for off in (0x140,0x150):
     if off not in stackdefined:out.append(f'        __vector4 {stk(off)};');stackdefined.add(off)
    out.append('        v[1] = Intersect2DCircleWithTriangleSOA('+', '.join(values)+', '+stk(0x140)+', '+stk(0x150)+');')
    defined={x for x in defined if x>=32 and not 59<=x<=63}|{1,8}
   continue
  if opname=='li':r,val=arg.split(',');gs[int(r[1:])]=int(val.strip(),0);continue
  if opname=='mr':r,t=arg.split(',');gs[int(r[1:])]=gs[int(t.strip()[1:])];continue
  if opname=='addi':
   r,t,val=[x.strip() for x in arg.split(',')];reg=int(r[1:]);base=gs.get(int(t[1:]));
   if base=='stack':
    if 'arg_' in val: off=int(val.split('arg_')[1],16)
    elif 'var_' in val: off=(int(val.split('+')[0],16) if '+' in val else 0)-int(val.split('var_')[1],16)
    else:off=int(val,0)
    gs[reg]=('stack',off)
   elif base=='sphere':gs[reg]=('sphere',int(val,0))
   else:raise RuntimeError((hex(a),'addi',line,base))
   continue
  if opname=='lwz':
   r,mem=arg.split(',');mt=re.search(r'arg_([0-9A-Fa-f]+)\(r1\)',mem);assert mt,(hex(a),line);gs[int(r[1:])]=args[int(mt.group(1),16)];continue
  if opname in ('lvx128','stvx128'):
   vd=((w>>21)&31)|(((w>>2)&3)<<5);ra=(w>>16)&31;rb=(w>>11)&31;av=gs.get(ra);bv=gs.get(rb)
   if ra==1 or rb==1:
    off=bv if ra==1 else av;loc=('stack',off)
   else:
    base=av if av not in (0,None) else bv;off=bv if av not in (0,None) else av
    loc=('tri',off) if base=='tri' else (('sphere',0) if base=='sphere' else base)
   if opname=='lvx128':
    if isinstance(loc,tuple) and loc[0]=='stack':expr=stackread(loc[1],a)
    elif isinstance(loc,tuple) and loc[0]=='sphere':expr='lPosition.mV' if loc[1]==0 else 'lDirection.mV'
    elif isinstance(loc,tuple) and loc[0]=='tri':off=loc[1];assert isinstance(off,int) and off%16==0 and off//16<len(fields),(hex(a),loc);expr='lTriangles.'+fields[off//16]+'.mV'
    else:raise RuntimeError((hex(a),'load',av,bv,loc))
    write(vd,expr,a)
   else:
    if isinstance(loc,tuple) and loc[0]=='stack':
     off=loc[1];out.append(('        __vector4 ' if off not in stackdefined else '        ')+f'{stk(off)} = {read(vd,a)}; // {a:08X}');stackdefined.add(off)
    elif loc in outs or loc in ('out0','out1'):out.append(f'        {loc}'+('.mV' if loc in outs else '')+f' = {read(vd,a)}; // {a:08X}')
    else:raise RuntimeError((hex(a),'store',av,bv,loc))
   continue
  wide=opname.endswith('128');vd=((w>>21)&31)|(((w>>2)&3)<<5) if wide else (w>>21)&31;va=((w>>16)&31)|(((w>>5)&1)<<5)|(((w>>10)&1)<<6) if wide else (w>>16)&31;vb=((w>>11)&31)|((w&3)<<5) if wide else (w>>11)&31;vc=(w>>6)&31
  D=lambda:read(vd,a);A=lambda:read(va,a);B=lambda:read(vb,a);C=lambda:read(vc,a);base=opname.removesuffix('128')
  if base=='vmr':expr=B()
  elif base=='vpermwi':expr=f'__vpermwi({B()}, {((w>>16)&31)|(((w>>6)&7)<<5)})'
  elif base=='vspltw':expr=f'__vspltw({B()}, {va&3})'
  elif base=='vspltisw':n=va&31;n=n-32 if n&16 else n;expr=f'__vspltisw({n})'
  elif base in ('vcfsx','vcsxwfp'):expr=f'__vcfsx({B()}, {va&31})'
  elif base=='vnot':expr=f'__vnor({A()}, {A()})'
  elif base in ('vrsqrtefp','vrefp'):expr=f'__{base}({B()})'
  elif base in ('vmaddfp','vnmsubfp'):expr=f'__{base}({A()}, {B() if wide else C()}, {D() if wide else B()})'
  elif base=='vmaddcfp':expr=f'__vmaddfp({A()}, {D()}, {B()})'
  elif base=='vsel':expr=f'__vsel({A()}, {B()}, {D() if wide else C()})'
  elif base in ('vmulfp','vsubfp','vaddfp','vor','vand','vandc','vslw','vnor','vcmpgefp','vcmpgtfp','vcmpeqfp','vmaxfp','vminfp','vmrglw','vmrghw'):expr=f'__{base}({A()}, {B()})'
  else:raise RuntimeError((hex(a),opname,arg,hex(w)))
  write(vd,expr,a)
 print(addr,'undefined',uses);assert not uses
 return out
helper=generate('82839AC0',True);parent=generate('8283EF50',False)
args=', '.join('__vector4 a'+str(i) for i in range(1,19));h=['    static __vector4 Intersect2DCircleWithTriangleSOA('+args+', __vector4& out0, __vector4& out1)','    {','        __vector4 v[128];']+[f'        v[{i}] = a{i};' for i in range(1,14)]+[f'        __vector4 stack{0xe0+(i-14)*16} = a{i};' for i in range(14,19)]+helper+['        return v[1];','    }'];q=[sig+'    {','        const Vector3Plus lPosition = lSweptSphere.GetPositionAndRadius();','        const Vector3Plus lDirection = lSweptSphere.GetDirectionAndLength();','        __vector4 v[128];']+parent+['        return Triangle4::Mask4(v[1]);','    }'];Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next/swept-generated.cpp').write_text('#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h"\n#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"\n#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"\nnamespace CgsGeometric {\n'+'\n'.join(h+q)+'\n}\n',encoding='utf8')
