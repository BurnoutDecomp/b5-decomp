#include "vendor/renderware/collision/KdTreeLineQuery.hpp"

#include <cmath>

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
    // 1.0e-6f is the dir-seed pad epsilon (matches AALineClipper::KF_PAD_EPSILON).
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
KdTreeLineQuery::KdTreeLineQuery(KdTree* lpKdTree,
                                 const AALineClipper::Vec4& rStart,
                                 const AALineClipper::Vec4& rEnd, f32 lfFatness)
{
    mpKdTree = lpKdTree; // +0x00

    // ---- build the per-axis dir-seed for AALineClipper::Init ---------------
    // The w lane fatness is 0 (the asm splats fatness into 3 stack words and
    // leaves the 4th at 0), so the w seed reduces to just the pad epsilon.
    const f32 lFatness[4] = { lfFatness, lfFatness, lfFatness, 0.0f };
    AALineClipper::Vec4 lSeed;
    f32* lpSeed = &lSeed.x;
    const f32* lpStart = &rStart.x;
    const f32* lpEnd   = &rEnd.x;
    for (s32 li = 0; li < 4; ++li)
    {
        const f32 lfAbsMax = std::fmax(AbsF(lpStart[li]), AbsF(lpEnd[li]));
        lpSeed[li] = lfAbsMax * lFatness[li] + KF_SEED_PAD_EPS;
    }

    // ---- precompute the per-axis clip parameters ---------------------------
    AALineClipper::Aabb lTreeBox;
    lTreeBox.mMin = { lpKdTree->maBoxMin[0], lpKdTree->maBoxMin[1],
                      lpKdTree->maBoxMin[2], lpKdTree->maBoxMin[3] };
    lTreeBox.mMax = { lpKdTree->maBoxMax[0], lpKdTree->maBoxMax[1],
                      lpKdTree->maBoxMax[2], lpKdTree->maBoxMax[3] };
    mClipper.Init(rStart, rEnd, lSeed, &lTreeBox); // this+0x10

    muTopNode   = 0; // +0x254
    muTopNodeHi = 0; // +0x258

    // The clipped line interval [tMin,tMax] lives in the first stack record's
    // key/far fields: +0x58 == maNodeStack[0].mfKey, +0x5C == maNodeStack[0].mfFar.
    // (On the root-descent branch the record's node words are filled below; the
    // interval IS the root entry.)
    f32 lfClipMin = KF_CLIP_MIN_INIT; // +0x58 init 0.0
    f32 lfClipMax = KF_CLIP_MAX_INIT; // +0x5C init 1.0

    // ---- axis-aligned slab clip --------------------------------------------
    const f32* lpClipMin   = &mClipper.mClipMin.x;
    const f32* lpRecipSpan = &mClipper.mRecipSpan.x;
    const f32* lpPadExtent = &mClipper.mPadExtent.x;
    const f32* lpBoxMin    = lpKdTree->maBoxMin;
    const f32* lpBoxMax    = lpKdTree->maBoxMax;

    f32 lEnter[3];
    f32 lExit[3];
    for (s32 li = 0; li < 3; ++li) // horizontal reduce uses x,y,z only
    {
        const f32 lfA = lpRecipSpan[li] * ((lpBoxMin[li] - lpPadExtent[li]) - lpClipMin[li]);
        const f32 lfB = lpRecipSpan[li] * ((lpBoxMax[li] + lpPadExtent[li]) - lpClipMin[li]);
        lEnter[li] = std::fmin(lfA, lfB);
        lExit[li]  = std::fmax(lfA, lfB);
    }

    // fsel-form horizontal reduce (matches the asm fsub/fsel chain exactly):
    //   maxEnter = max(max(enter.x, enter.y), enter.z)
    //   minExit  = min(min(exit.x,  exit.y),  exit.z)
    f32 lfMaxEnter = Fsel(lEnter[0] - lEnter[1], lEnter[0], lEnter[1]);
    lfMaxEnter     = Fsel(lfMaxEnter - lEnter[2], lfMaxEnter, lEnter[2]);
    f32 lfMinExit  = Fsel(lExit[0] - lExit[1], lExit[1], lExit[0]);
    lfMinExit      = Fsel(lfMinExit - lExit[2], lExit[2], lfMinExit);

    // mfClipMin = max(prevClipMin, maxEnter) ; mfClipMax = min(prevClipMax, minExit)
    lfClipMin = Fsel(lfClipMin - lfMaxEnter, lfClipMin, lfMaxEnter);
    lfClipMax = Fsel(lfClipMax - lfMinExit, lfMinExit, lfClipMax);

    maNodeStack[0].mfKey = lfClipMin; // +0x58
    maNodeStack[0].mfFar = lfClipMax; // +0x5C

    // ---- seed the descent cursor -------------------------------------------
    if (lfClipMin >= lfClipMax)
    {
        // Empty clipped interval: the segment misses the tree AABB.
        muStackCount = 0; // +0x250
    }
    else if (lpKdTree->muNumBranchNodes != 0)
    {
        // Tree has interior nodes: seed the first stack record for a root descent.
        maNodeStack[0].muNode1 = 0;          // +0x54  (stw 0)
        // 0xFFFFFFFF bit pattern (li -1) -- the asm stores it as a word.
        maNodeStack[0].muNode0 = 0xFFFFFFFFu; // +0x50  (stw -1)
        muStackCount           = 1;           // +0x250 (stw 1)
    }
    else
    {
        // Leaf-only tree: latch the single leaf node and leave the cursor inactive.
        muTopNodeHi  = 0;                          // +0x258
        muTopNode    = lpKdTree->muLeafNodeIndex;  // +0x254
        muStackCount = 0;                          // +0x250
    }
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
KdTreeLineQuery* KdTreeLineQuery::ClipEnd(f32 lfFarParam)
{
    u32 luSurvivors = 0;

    const u32 luCount = muStackCount;
    for (u32 lui = 0; lui < luCount; ++lui)
    {
        if (maNodeStack[lui].mfKey <= lfFarParam)
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
    return this;
}

} // namespace collision
} // namespace rw
