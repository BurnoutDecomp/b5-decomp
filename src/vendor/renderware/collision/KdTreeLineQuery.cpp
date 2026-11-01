#include "vendor/renderware/collision/KdTreeLineQuery.hpp"

#include <cmath>
#include "vendor/renderware/collision/LineSegKernelMath.hpp"

// ===========================================================================
// rw::collision::KdTreeLineQuery -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   KdTreeLineQuery::KdTreeLineQuery @ 0x828AEE80
//   KdTreeLineQuery::ClipEnd         @ 0x82BB0408
//
// The X360 ctor is hand-vectorised VMX/AltiVec (lvx128 / vsubfp / vaddfp /
// vmulfp128 / vminfp / vmaxfp / fsel). It computes an axis-aligned slab clip of
// the line interval against the tree's AABB. Following the AALineClipper /
// FeatureEdge / Triangle4 precedent in this project, it is lowered to a
// SEMANTIC, portable, named-member float reconstruction preserving the store
// ORDER, widths, and offsets. ClipEnd is scalar in the binary and transcribed
// faithfully (fcmpu key compare + fsub/fsel far-param clamp).
// ===========================================================================

namespace rw
{
namespace collision
{

const u32 KdTreeLineQuery::KU_NUM_STACK_ENTRIES;

namespace
{
    inline f32 AbsF(f32 lfV) { return std::fabs(lfV); }

    // PPC fsel f, cond, a, b == (cond >= 0) ? a : b.
    inline f32 Fsel(f32 lfCond, f32 lfA, f32 lfB)
    {
        return (lfCond >= 0.0f) ? lfA : lfB;
    }

    // The shared .rdata zero/one floats reached by reference in the ctor asm
    // (already valued elsewhere in the project): flt_82001CC0 == 0.0f seeds the
    // clip tMin and flt_82001C98 == 1.0f seeds the clip tMax. flt_820AD47C ==
    // 1.0e-6f is the dir-seed pad epsilon (AALineClipper::KF_ABS_EPSILON's word).
    const f32 KF_CLIP_MIN_INIT = 0.0f; // flt_82001CC0
    const f32 KF_CLIP_MAX_INIT = 1.0f; // flt_82001C98
    const f32 KF_SEED_PAD_EPS  = 1.0e-6f; // flt_820AD47C
}

// ---------------------------------------------------------------------------
// KdTreeLineQuery::KdTreeLineQuery @ 0x828AEE80
//
// Steps, in asm order:
//   1. mpKdTree = lpKdTree                                   (stw a2, 0(this))
//   2. Build AALineClipper's dir-seed v3 from the segment:
//          seed.lane = max(|start.lane|,|end.lane|) * fatness.lane + padEps
//      where fatness is splatted to {fatness,fatness,fatness,0} (w lane = 0) and
//      padEps = flt_820AD47C. (vandc sign-clear + vmaxfp + vmaddfp.)
//   3. mClipper.Init(start, end, seed, &treeBox)             (treeBox = a2+0x10)
//   4. muTopNode = muTopNodeHi = 0                           (stw 0, 0x254/0x258)
//   5. mfClipMin = 0.0 ; mfClipMax = 1.0                     (flt_82001CC0 / C98)
//   6. Slab-clip the interval against the tree AABB:
//        for each axis: tA = recipSpan * ((boxMin - padExtent) - clipMin)
//                       tB = recipSpan * ((boxMax + padExtent) - clipMin)
//                       enter = min(tA,tB) ; exit = max(tA,tB)
//        maxEnter = max over x,y,z of enter ; minExit = min over x,y,z of exit
//        mfClipMin = max(mfClipMin, maxEnter) ; mfClipMax = min(mfClipMax, minExit)
//   7. Seed the descent cursor:
//        if (mfClipMin >= mfClipMax)             -> empty: muStackCount = 0
//        else if (muNumBranchNodes != 0)         -> root descent:
//               maNodeStack[0].node0 = -1 ; .node1 = 0 ; muStackCount = 1
//        else (leaf-only)                        -> muTopNodeHi = 0 ;
//               muTopNode = muLeafNodeIndex ; muStackCount = 0
// ---------------------------------------------------------------------------
KdTreeLineQuery::KdTreeLineQuery(const KdTree* lpKdTree, rw::math::vpu::Vector3 rStart,
                                 rw::math::vpu::Vector3 rEnd, f32 lfFatness)
    : mpKdTree(lpKdTree), mClipper(rStart,rEnd,rw::math::vpu::Vector3(lfFatness,lfFatness,lfFatness),lpKdTree->mBBox)
{
    muTopNode=0; muTopNodeHi=0;
    const __vector4 lo=__vsubfp(__vsubfp(lpKdTree->mBBox.mMin.mV,mClipper.m_padding.mV),mClipper.m_origin.mV);
    const __vector4 hi=__vsubfp(__vaddfp(lpKdTree->mBBox.mMax.mV,mClipper.m_padding.mV),mClipper.m_origin.mV);
    const __vector4 a=__vmulfp(mClipper.m_recip.mV,lo), b=__vmulfp(mClipper.m_recip.mV,hi);
    const __vector4 enter=__vminfp(a,b), exit=__vmaxfp(a,b);
    f32 maxEnter=Fsel(enter.x-enter.y,enter.x,enter.y);
    maxEnter=Fsel(maxEnter-enter.z,maxEnter,enter.z);
    f32 minExit=Fsel(exit.x-exit.y,exit.y,exit.x);
    minExit=Fsel(minExit-exit.z,exit.z,minExit);
    const f32 clipMin=Fsel(0.0f-maxEnter,0.0f,maxEnter);
    const f32 clipMax=Fsel(1.0f-minExit,minExit,1.0f);
    maNodeStack[0].mfKey=clipMin; maNodeStack[0].mfFar=clipMax;
    if (!(clipMin < clipMax)) muStackCount=0;
    else if (lpKdTree->muNumBranchNodes != 0)
    { maNodeStack[0].muNode1=0; maNodeStack[0].muNode0=0xFFFFFFFFu; muStackCount=1; }
    else { muTopNodeHi=0; muTopNode=lpKdTree->muNumEntries; muStackCount=0; }
}

// ---------------------------------------------------------------------------
// KdTreeLineQuery::ClipEnd @ 0x82BB0408
//
// Compact the pending-node stack to a new far line endpoint lfFarParam. Walk
// the live records [0, muStackCount); for each whose mfKey <= lfFarParam, copy
// it down to the next survivor slot (preserving the two header words) and clamp
// its mfFar to min(mfFar, lfFarParam); drop the rest. Write back the survivor
// count. Returns this.
//
//   for (i = 0, j = 0; i < count; ++i)
//     if (stack[i].mfKey <= lfFarParam)
//       stack[j].node0 = stack[i].node0 ; stack[j].node1 = stack[i].node1
//       stack[j].mfFar = (stack[i].mfFar - lfFarParam >= 0) ? lfFarParam
//                                                           : stack[i].mfFar
//       ++j
//   muStackCount = j
// ---------------------------------------------------------------------------
void KdTreeLineQuery::ClipEnd(f32 lfFarParam)
{
    u32 luSurvivors = 0;

    const u32 luCount = muStackCount;
    for (u32 lui = 0; lui < luCount; ++lui)
    {
        if (!(maNodeStack[lui].mfKey > lfFarParam))
        {
            NodeStackEntry& rDst = maNodeStack[luSurvivors];
            // Copy the two header words verbatim (asm: two 8-byte ld/std pairs).
            rDst.muNode0 = maNodeStack[lui].muNode0;
            rDst.muNode1 = maNodeStack[lui].muNode1;
            rDst.mfKey   = maNodeStack[lui].mfKey;
            // Clamp the far param to the new endpoint:
            //   fsub f13, far, end ; fsel f0, f13, end, far == min(far, end).
            const f32 lfFar = maNodeStack[lui].mfFar;
            rDst.mfFar = Fsel(lfFar - lfFarParam, lfFarParam, lfFar);
            ++luSurvivors;
        }
    }

    muStackCount = luSurvivors;
}

} // namespace collision
} // namespace rw
