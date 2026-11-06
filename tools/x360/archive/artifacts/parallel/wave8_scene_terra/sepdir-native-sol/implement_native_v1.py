from pathlib import Path
import re, hashlib
ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sepdir-native-sol')
P=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/SeparatingDirection.cpp')
assert hashlib.sha256(P.read_bytes()).hexdigest()=='1e39d3fef44a346c2dcdd48b383e744756f8275d6c2247c9e04a7e54ee30c445'
s=P.read_text()
# All legacy rows in this owned TU are geometry directions/points; radius
# declarations are split to the true scalar SIMD class below.
s=re.sub(r'\bVec4\b','Vector3',s)
s=s.replace('namespace collision\n{','namespace collision\n{\n\nusing rw::math::vpu::Vector3;\nusing rw::math::vpu::VecFloat;',1)
s=s.replace('#include <cmath>   // sqrt, fabs\n','')
start=s.index('namespace\n{\n    // dot3')
end=s.index('// ===========================================================================\n// rw::collision::FindBestSepDir_FindCandidates',start)
s=s[:start]+'''namespace
{
    inline __vector4 Broadcast(f32 value)
    {
        const __vector4 row = {value, value, value, value};
        return row;
    }
    inline f32 Dot3(const Vector3& a, const Vector3& b)
    { return __vmsum3fp(a.mV, b.mV).x; }
    inline Vector3 Sub(const Vector3& a, const Vector3& b)
    { return Vector3(__vsubfp(a.mV, b.mV)); }
    inline Vector3 Add(const Vector3& a, const Vector3& b)
    { return Vector3(__vaddfp(a.mV, b.mV)); }
    inline Vector3 Scale(const Vector3& a, f32 b)
    { return Vector3(__vmulfp(a.mV, Broadcast(b))); }
    inline Vector3 MaddScalar(const Vector3& a, f32 b, const Vector3& c)
    { return Vector3(__vmaddfp(a.mV, Broadcast(b), c.mV)); }
    inline Vector3 Negate(const Vector3& a)
    {
        const __vector4 all = __vspltisw(-1);
        return Vector3(__vxor(a.mV, __vslw(all, all)));
    }
    inline Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return Vector3(__vpermwi(__vnmsubfp(__vpermwi(a.mV, 0x63), b.mV,
            __vmulfp(a.mV, __vpermwi(b.mV, 0x63))), 0x63));
    }
    inline __vector4 ReciprocalRefined(__vector4 x)
    {
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        __vector4 e = __vrefp(x);
        e = __vmaddfp(e, __vnmsubfp(e, x, one), e);
        return __vmaddfp(e, __vnmsubfp(e, x, one), e);
    }
}

'''+s[end:]
s=s.replace('// fcmpu/ble falls through for unordered values.\n        const bool flip = !(flipped <= forward);','// fcmpu/ble branches when GT is clear, including unordered.\n        const bool flip = flipped > forward;')
s=s.replace('if (!best || !(distance <= bestDistance))','if (!best || distance > bestDistance)')
# Real free returns are VMX VecFloat values, never a scalar FPR return.
for name in ['FindBestSepDirWithCylinder_Evaluate','FindBestSepDirWithCylinder','FindBestSeparatingDirCylVol','FindBestSeparatingDirVolCyl','FindBestSeparatingDirectionTriBox']:
    s=re.sub(r'\bf32 (?='+name+r'\s*\()', 'VecFloat ',s)
s=s.replace('extern intptr_t FindBestSepDirWithCylinder_FindCandidates','extern u32 FindBestSepDirWithCylinder_FindCandidates')
s=s.replace('intptr_t FindBestSepDirWithCylinder_FindCandidates','u32 FindBestSepDirWithCylinder_FindCandidates')
s=s.replace('intptr_t aiCandidateResult','u32 aiCandidateResult').replace('const intptr_t liCandidateResult','const u32 liCandidateResult')
s=s.replace('static_cast<intptr_t>(luNumDirs)','luNumDirs')
s=s.replace('Vector3 laCandidates[18];','Vector3 laCandidates[17];')
s=s.replace('const f32 lfSeparation =\n        FindBestSepDirWithCylinder','const VecFloat lfSeparation =\n        FindBestSepDirWithCylinder')
# Remove the two old scalar rim bodies and their idealized helpers as a block.
a=s.index('namespace\n{\n    // Full-vector vmulfp128')
b=s.index('// ===========================================================================\n// rw::collision::AddRimToEdgeCandidate',a)
s=s[:a]+'@@NATIVE_RIM_BODIES@@\n\n'+s[b:]
# Fixed reference radius contract at its only named declaration/definition.
s=s.replace('const Vector3& arRimRadius, const Vector3& arEdgePoint','const VecFloat& arRimRadius, const Vector3& arEdgePoint')
# All component reads now name the real VectorIntrinsic storage; unrelated
# GP scalar members are untouched. Exact receivers are only vector locals/fields.
s=re.sub(r'(?<!mV)\.(x|y|z|w)\b',r'.mV.\1',s)
# The scalar helpers' VectorIntrinsic expressions already have direct lanes.
s=s.replace('__vmsum3fp(a.mV, b.mV).mV.x','__vmsum3fp(a.mV, b.mV).x')
s=s.replace('__vmsum3fp(a.mV,b.mV).mV.x','__vmsum3fp(a.mV,b.mV).x')
s=s.replace('__vsel(__vmulfp(x,SATInvLength(x)),zero,__vcmpeqfp(zero,x)).mV.x','__vsel(__vmulfp(x,SATInvLength(x)),zero,__vcmpeqfp(zero,x)).x')
s=re.sub(r'(__v(?:subfp|sel)\([^;\n]+\))\.mV\.x',r'\1.x',s)
# Remove old splat/scalar ideal normalize helpers, retaining real native types.
a=s.index('namespace\n{\n    // Broadcast one scalar')
b=s.index('// ===========================================================================\n// rw::collision::FindBestSepDirWithCylinder_Evaluate',a)
s=s[:a]+'''namespace
{
    inline VecFloat SplatLane(const Vector3& row, u32 lane)
    {
        if (lane == 0) return VecFloat(__vspltw(row.mV, 0));
        if (lane == 1) return VecFloat(__vspltw(row.mV, 1));
        return VecFloat(__vspltw(row.mV, 2));
    }
    inline Vector3 NormalizeRefined(const Vector3& row, f32 square)
    { return Vector3(__vmulfp(row.mV, SATInvLength(Broadcast(square)))); }
    inline VecFloat SignedHalfExtent(const Vector3& dimensions, u32 lane, f32 dot)
    {
        const VecFloat half = SplatLane(dimensions, lane);
        const __vector4 all = __vspltisw(-1);
        return VecFloat(__vsel(half.mV, __vxor(half.mV, __vslw(all, all)),
            __vcmpgtfp(__vspltisw(0), Broadcast(dot))));
    }
}

'''+s[b:]
s=s.replace('NormalizeExact(', 'NormalizeRefined(')
# Genuine splat radius values must remain VecFloat, not Vector3 wrappers.
for v in ['lvOtherRadius','lvRimRadius','lvRadius1','lvRadius2','lvSH0','lvSH1','lvSH2','lvSA0','lvSA1','lvSA2']:
    s=re.sub(r'\bVector3 (?:const )?'+v+r'\b', lambda m:m[0].replace('Vector3','VecFloat'),s)
# Radius rows in AddRimToRim are obtained from dimensions lane1, not scalar
# assignment through physical fields.
a=s.index('    VecFloat lvRadius1;')
b=s.index('    const Vector3 lvAxisOffset1',a)
s=s[:a]+'''    const VecFloat lvRadius1(__vspltw(lpGP1->mDimensions.mV, 1));
    const VecFloat lvRadius2(__vspltw(lpGP2->mDimensions.mV, 1));

'''+s[b:]
s=s.replace('Scale(lvCrossDir, 1.0f / std::sqrt(lfCrossLenSq))','Vector3(__vmulfp(lvCrossDir.mV, SATInvLength(Broadcast(lfCrossLenSq))))')
# Real raw vrsqrtefp crosses of the triangle/box closed form.
s=s.replace('Scale(lvCross, 1.0f / std::sqrt(Dot3(lvCross, lvCross)))','Vector3(__vmulfp(lvCross.mV, __vrsqrtefp(__vmsum3fp(lvCross.mV, lvCross.mV))))')
s=s.replace('return lfBestSep;','return VecFloat(Broadcast(lfBestSep));')
# Native cylinder direct/PMF interval dispatch and per-lane winner values.
a=s.index('VecFloat FindBestSepDirWithCylinder_Evaluate(\n',s.index('static const u32 KU_CYLINDER_SAT_MAX_CANDIDATES'))
b=s.index('// ===========================================================================\n// rw::collision::AddAxisToEdgeCandidate',a)
s=s[:a]+'''VecFloat FindBestSepDirWithCylinder_Evaluate(
    Vector3& bestDirection, Vector3* candidates, u32 count,
    const GPCylinder& cylinder, const GPInstance& other)
{
    Interval cylinderIntervals[16], otherIntervals[16];
    cylinder.GetIntervals(candidates, count, cylinderIntervals);
    (other.*(other.mMethods.mGetIntervals))(candidates, count, otherIntervals);
    __vector4 bestDistance = __vspltisw(0);
    const Vector3* best = 0;
    bool bestFlip = false;
    for (u32 i = 0; i != count; ++i)
    {
        __vector4 distance = __vsubfp(otherIntervals[i].min.mV, cylinderIntervals[i].max.mV);
        const __vector4 reverse = __vsubfp(cylinderIntervals[i].min.mV, otherIntervals[i].max.mV);
        unsigned int cr;
        __vcmpgtfpR(reverse, distance, &cr);
        const bool flip = (cr & 0x80) != 0;
        if (flip) distance = reverse;
        __vcmpgtfpR(distance, bestDistance, &cr);
        if (!best || (cr & 0x80) != 0)
        { best = &candidates[i]; bestDistance = distance; bestFlip = flip; }
    }
    bestDirection = bestFlip ? Negate(*best) : *best;
    return VecFloat(bestDistance);
}

'''+s[b:]
# Geometry constant sources are actual existing math rows, not new initialized
# static class objects. Header/constructor lifecycle remains unchanged.
s=s.replace('static const Vector3 KV_AXIS_SEED_I = { 1.0f, 0.0f, 0.0f, 0.0f };','')
s=s.replace('static const Vector3 KV_AXIS_SEED_J = { 0.0f, 1.0f, 0.0f, 0.0f };','')
s=s.replace('? KV_AXIS_SEED_J\n                                                                  : KV_AXIS_SEED_I','? rw::math::vpu::GetVector3_YAxis()\n                                                                  : rw::math::vpu::GetVector3_XAxis()')
# Replace TriBox interval reduction with the actual VMX min/max/abs trees.
a=s.index('    inline f32 ComputeTriBoxAxisSeparation(')
b=s.index('\n}\n\nVecFloat FindBestSeparatingDirectionTriBox',a)
s=s[:a]+'''    inline f32 ComputeTriBoxAxisSeparation(const Vector3& axis,
        const Vector3& p0, const Vector3& p1, const Vector3& p2,
        const Vector3& boxPosition, const Vector3* boxProjections, f32& sign)
    {
        const __vector4 all = __vspltisw(-1), mask = __vslw(all, all);
        const __vector4 d0 = __vmsum3fp(p0.mV, axis.mV);
        const __vector4 d1 = __vmsum3fp(p1.mV, axis.mV);
        const __vector4 d2 = __vmsum3fp(p2.mV, axis.mV);
        const __vector4 low = __vminfp(d0, __vminfp(d1, d2));
        const __vector4 high = __vmaxfp(d0, __vmaxfp(d1, d2));
        const __vector4 center = __vmsum3fp(axis.mV, boxPosition.mV);
        const __vector4 a = __vandc(__vmsum3fp(boxProjections[0].mV, axis.mV), mask);
        const __vector4 b = __vandc(__vmsum3fp(boxProjections[1].mV, axis.mV), mask);
        const __vector4 c = __vandc(__vmsum3fp(boxProjections[2].mV, axis.mV), mask);
        const __vector4 radius = __vaddfp(__vaddfp(a, b), c);
        const __vector4 reverse = __vsubfp(low, __vaddfp(center, radius));
        const __vector4 forward = __vsubfp(__vsubfp(center, radius), high);
        const __vector4 gt = __vcmpgtfp(reverse, forward), plus = __vcfsx(__vspltisw(1), 0);
        sign = __vsel(plus, __vxor(plus, mask), gt).x;
        return __vsel(forward, reverse, gt).x;
    }'''+s[b:]
# Axis-edge uses native refinement, no exact floating point division.
s=s.replace('Dot3(lvPlaneNormal, Sub(arAxisPoint, arEdgePoint))\n                             / Dot3(lvPlaneNormal, arEdgeDirection)', '__vmulfp(__vmsum3fp(lvPlaneNormal.mV, Sub(arAxisPoint, arEdgePoint).mV),\n                                ReciprocalRefined(__vmsum3fp(lvPlaneNormal.mV, arEdgeDirection.mV))).x')
# Native semantic constructor spellings in automatic rows only.
s=s.replace('const Vector3 lvSeed =','const Vector3 lvSeed =')

# Pure VMX rim bodies are emitted instruction-for-instruction as intrinsic
# expressions; only dead stack staging is eliminated. This avoids algebraic
# rewriting of double-crosses, duplicated one-refine radial paths, all-lane
# branch records, and the scalar fsubs in the general RimToRim loop.
def asm_rows(addr):
    out=[]
    for ln in (ROOT/(addr+'.asm')).read_text().splitlines():
        m=re.match(r'0x([0-9A-F]+)\s+(\S+)\s*(.*)',ln)
        if m: out.append((int(m[1],16),m[2],m[3].split('#')[0].strip()))
    return out

def generate(addr,signature,inputs,loads,specials,return_expr):
    rows=asm_rows(addr)
    targets={int(re.search(r'loc_([0-9A-F]+)',args)[1],16) for _,op,args in rows if re.search(r'loc_([0-9A-F]+)',args)}
    regs=sorted({int(v) for _,_,args in rows for v in re.findall(r'\bv(\d+)\b',args)})
    out=[signature,'{','    // ARTIST '+addr+': preserve vector expression and select order.','    __vector4 '+', '.join('v'+str(v) for v in regs)+';','    unsigned int cr;','    bool allGreater = false;','    u32 iteration = 0;','    u32 remaining = 0;','    f32 scalarFraction = 0.0f;']
    out+=['    '+x for x in inputs]
    ignored={'lis','li','addi','stw','stfs','lfs','mfocrf','extrwi.','mr','addic.','cmplwi','fsubs'}
    for ea,op,args in rows:
        if ea in targets:out.append('L%08X:'%ea)
        if ea in specials:
            if specials[ea]:out.append('    '+specials[ea]+' // %08X'%ea)
            continue
        aa=[a.strip() for a in args.split(',')]
        d=aa[0] if aa else ''
        ex=None
        if op in ['vmr','vmr128']:ex=aa[1]
        elif op in ['vaddfp','vsubfp','vmulfp128','vmsum3fp128','vandc','vxor','vslw','vminfp','vmaxfp','vcmpgtfp','vcmpgefp','vcmpeqfp']:
            n={'vmulfp128':'vmulfp','vmsum3fp128':'vmsum3fp'}.get(op,op)
            ex='__'+n+'('+', '.join(aa[1:])+')'
        elif op in ['vnmsubfp','vmaddfp']:
            ex='__'+op+'('+aa[1]+', '+aa[3]+', '+aa[2]+')'
        elif op=='vsel':ex='__vsel('+', '.join(aa[1:])+')'
        elif op=='vnot':ex='__vnor('+aa[1]+', '+aa[1]+')'
        elif op in ['vcfsx','vspltw','vspltisw','vpermwi128']:
            n='vpermwi' if op=='vpermwi128' else op
            ex='__'+n+'('+', '.join(aa[1:])+')'
        elif op in ['vrsqrtefp','vrefp']:ex='__'+op+'('+aa[1]+')'
        elif op=='vcmpgtfp.':
            out.append('    '+d+' = __vcmpgtfpR('+', '.join(aa[1:])+', &cr);')
            out.append('    allGreater = (cr & 0x80) != 0;')
            continue
        elif op in ['lvx128','lvlx']:
            if ea not in loads:raise RuntimeError(('unmapped load',hex(ea),args))
            ex=loads[ea]
        elif op=='stvx128':
            if args=='v0, r0, r10':out.append('    lpCandidate->mV = v0; // %08X'%ea)
            elif ea==0x82BB5F24:out.append('    scalarFraction = v9.x;')
            else:raise RuntimeError(('unmapped store',hex(ea),args))
            continue
        elif op in ['b','beq','bne']:
            target='L'+re.search('loc_([0-9A-F]+)',args)[1]
            if op=='b': out.append('    goto '+target+';')
            elif 'cr6' in args:out.append('    if (iteration '+('==' if op=='beq' else '!=')+' 10) goto '+target+';')
            elif ea==0x82BB5608:out.append('    if (remaining) goto '+target+';')
            else:out.append('    if ('+('!' if op=='beq' else '')+'allGreater) goto '+target+';')
            continue
        elif op=='blr':out.append('    return '+return_expr(ea)+';');continue
        elif op in ignored or op=='.long':continue
        else:raise RuntimeError(('unknown',op,args))
        out.append('    '+d+' = '+ex+';')
    out+=['}']
    return '\n'.join(out)

rimedge=generate('82BB53F8', 'Vector3 RimToEdge(Vector3 aRimCentre, Vector3 aRimAxis, VecFloat aRimRadius,\n                  Vector3 aEdgeP1, Vector3 aEdgeP2)',
    ['v1 = aRimCentre.mV; v2 = aRimAxis.mV; v3 = aRimRadius.mV;','v4 = aEdgeP1.mV; v5 = aEdgeP2.mV;'],
    {0x82BB5424:'Broadcast(KF_RIM_DEGENERATE_EPSILON)'},
    {0x82BB5520:'remaining = 10;',0x82BB5528:'--remaining;'},lambda ea:'Vector3(v1)')
loads={0x82BB56C8:'aRimRadius1.mV',0x82BB56D0:'aRimRadius2.mV',0x82BB570C:'Broadcast(KF_RIM_RADIUS_TOLERANCE_SCALE)',0x82BB5728:'Broadcast(KF_RIM_FLT_MIN)',0x82BB5748:'aRimAxis1.mV',0x82BB5754:'aSepDir.mV',0x82BB575C:'aRimAxis2.mV',0x82BB5764:'aRimCentre1.mV',0x82BB576C:'aRimCentre2.mV',0x82BB5870:'Broadcast(KF_RIM_DEGENERATE_EPSILON)',0x82BB58EC:'Broadcast(KF_RIM_FLT_MIN)',0x82BB5990:'aRimAxis2.mV',0x82BB5994:'aRimAxis1.mV',0x82BB599C:'aSepDir.mV',0x82BB5A38:'Broadcast(KF_RIM_FLT_MIN)',0x82BB5A8C:'Broadcast(0.5f)',0x82BB5B20:'Broadcast(2.0f)',0x82BB5B98:'Broadcast(KF_RIM_DEGENERATE_EPSILON)',0x82BB5CF8:'Broadcast(0.0f)',0x82BB5DB4:'Broadcast(0.0f)',0x82BB5E1C:'Broadcast(1.0f)',0x82BB5EC4:'Broadcast(1.0f)',0x82BB5EE0:'Broadcast(0.5f)',0x82BB5F38:'Broadcast(scalarFraction)',0x82BB5FE4:'Broadcast(0.0f)',0x82BB6060:'Broadcast(0.5f)'}
rimrim=generate('82BB56C8', 'RwBool RimToRim(const Vector3& aRimCentre1, const Vector3& aRimAxis1, const VecFloat& aRimRadius1,\n                const Vector3& aRimCentre2, const Vector3& aRimAxis2, const VecFloat& aRimRadius2,\n                const Vector3& aSepDir, Vector3* lpCandidate)',[],loads,
    {0x82BB5E10:'iteration = 0;',0x82BB5F30:'scalarFraction = 1.0f - scalarFraction;',0x82BB604C:'++iteration;'},
    lambda ea:'0' if ea==0x82BB5744 else '1')
s=s.replace('@@NATIVE_RIM_BODIES@@',rimedge+'\n\n'+rimrim)
# Remove obsolete reconstruction-history comments and inaccurate scalar claims;
# retain source/primary citations beside active algorithms in the final pass.
s=s.replace('const Vector3 lvOtherRadius =','const VecFloat lvOtherRadius =').replace('const Vector3 lvRimRadius  =','const VecFloat lvRimRadius  =')
# Fully qualified World helpers were not touched by the source migration.
s=s.replace('rw::math::vpu::Vector3','Vector3').replace('rw::math::vpu::VecFloat','VecFloat')
s=s.replace('using Vector3;','using rw::math::vpu::Vector3;').replace('using VecFloat;','using rw::math::vpu::VecFloat;')
# Current full file is a native implementation, not the obsolete PC lowering.
s=s.replace('hand-vectorised bodies are\n// lowered to portable per-lane scalar maths per the committed Feature /\n// FeatureEdge precedent, preserving branch polarity, store order and every\n// caller-visible store','native VMX bodies retain the original expression\n// order and actual Vector3/VecFloat register ABI')
P.write_bytes(s.replace('\n','\r\n').encode())
print(hashlib.sha256(P.read_bytes()).hexdigest())
