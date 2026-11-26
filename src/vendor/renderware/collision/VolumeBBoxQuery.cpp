// XDK size optimization: ARTIST compiled RenderWare's rwcollision for size (record-form cr0
// tests, as in the rwcore filesys sources).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "rw/rwcore_structs.h"
#include <new>
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"

#include <cmath>     // fabs
#include <cstring>   // memcpy

// ===========================================================================
// rw::collision::VolumeBBoxQuery -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2).
//
//   VolumeBBoxQuery::AddVolumeRef           @ 0x82BBBC20
//   VolumeBBoxQuery::GetResourceDescriptor  @ 0x82BBBD38
//   VolumeBBoxQuery::Initialize             @ 0x82BBBD90
//   VolumeBBoxQuery::GetOverlaps            @ 0x82BBBDE8
//
//   VolumeBBoxQuery::AddPrimitiveRef        @ 0x82BB0478   (waveQ5 C1)
//
// (AddPrimitiveRef was declaration-only until waveQ5 C1 on the theory that it
// "belongs to another TU". It has no per-address JSON in .ida-exports -- an
// EXPORT HOLE, not a missing function (AGENTS gotcha 6) -- and a targeted
// headless-IDA pass on a private .i64 copy recovered all 66 instructions.)
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // Aggregate volume-type id: canonical rwccore.h:1461-1468 -- the
    // VolumeType enum continues the GPInstance primitive ids (UNUSED=0 ..
    // CYLINDER=5) with VOLUMETYPEAGGREGATE == 6 (the asm's cmpwi cr6, rN, 6).
    const u32 KU_VOLUMETYPE_AGGREGATE = 6;

    // Bit-exact float constant (the translation threshold is a subnormal) --
    // same helper as the committed LineSegIntersect.cpp.
    f32 F32FromBits(u32 auBits)
    {
        f32 lfValue;
        std::memcpy(&lfValue, &auBits, sizeof(lfValue));
        return lfValue;
    }

    // flt_82180D6C: identity fast-path per-lane tolerance (FLT_EPSILON;
    // Hex-Rays shows the value).
    const f32 KF_IDENTITY_LANE_EPSILON = 1.1920928955078125e-7f;

    // flt_82180D68: raw 0x00200000 == 2^-128 subnormal -- the squared
    // translation-length threshold of the identity fast-path (word read from
    // the BURNOUT_X360_ARTIST.XEX.i64 database for this pass; the same raw
    // threshold as the committed LineSegIntersect.cpp KF_PARALLEL_EPSILON_BITS
    // / flt_82180A24).
    static const f32 KF_IDENTITY_TRANSLATION_EPSILON = F32FromBits(0x00200000u);

    // -----------------------------------------------------------------------
    u32 VolumeFlags(const Volume* lpVolume) { return lpVolume->muFlags; }
    const math::vpu::Matrix44Affine* VolumeTransform(const Volume* lpVolume)
    {
        return lpVolume->GetRelativeTransform();
    }
    Aggregate* VolumeAggregate(const Volume* lpVolume)
    {
        return lpVolume->mAggregateData.mpAggregate;
    }

    // VolRef pointer-word views (the committed PrimitiveIntersect.cpp
    // helpers). Since waveQ5 C1 the stored words are HOST width, so these are
    // plain reinterpretations, not a re-widening of a truncated console word.
    const Volume* VolRefVolume(const VolRef& lrRef)
    {
        return lrRef.volume;
    }

    const math::vpu::Matrix44Affine* VolRefTransform(const VolRef& lrRef)
    {
        return lrRef.tm;
    }

    // -----------------------------------------------------------------------


    // -----------------------------------------------------------------------
    // Affine compose: child = A (the aggregate volume's relative transform,
    // vol+0x00) applied on B (the parent transform cached in the VolRef).
    // The full vspltw/vmulfp128/vmaddfp chain from the asm, one row at a
    // time, all four lanes (the W lane travels through like any other):
    //   row_i = A_i.x*B0 + A_i.y*B1 + A_i.z*B2         (i = 0..2)
    //   row_3 = (A_3.x*B0 + B3) + A_3.y*B1 + A_3.z*B2  (translation seed)
    // Accumulation order preserved: the x*B0 product first (row 3 folds the
    // B3 seed into that first vmaddfp), then the y*B1 madd, then the z*B2
    // madd. Same association as the committed vpu Mult(Matrix44Affine,
    // Matrix44); written out locally because both operands here are the
    // 4-row Matrix44Affine images.
    // -----------------------------------------------------------------------
    void ComposeAffine(math::vpu::Matrix44Affine&       lrOut,
                       const math::vpu::Matrix44Affine& arVolTm,   // A: rows @ vol+0x00
                       const math::vpu::Matrix44Affine& arParent)  // B: cached VolRef rows
    {
        const math::vpu::VectorIntrinsic* lapRowA[4] =
            { &arVolTm.xAxis.mV, &arVolTm.yAxis.mV, &arVolTm.zAxis.mV, &arVolTm.wAxis.mV };
        math::vpu::VectorIntrinsic* lapRowOut[4] =
            { &lrOut.xAxis.mV, &lrOut.yAxis.mV, &lrOut.zAxis.mV, &lrOut.wAxis.mV };

        const f32* lafB0 = arParent.xAxis.mV.v;
        const f32* lafB1 = arParent.yAxis.mV.v;
        const f32* lafB2 = arParent.zAxis.mV.v;
        const f32* lafB3 = arParent.wAxis.mV.v;

        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const f32 lfAx = lapRowA[liRow]->v[0];   // vspltw row, 0
            const f32 lfAy = lapRowA[liRow]->v[1];   // vspltw row, 1
            const f32 lfAz = lapRowA[liRow]->v[2];   // vspltw row, 2
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                // Rows 0-2: vmulfp128 x*B0; row 3: vmaddfp x*B0 + B3.
                f32 lfAcc = lfAx * lafB0[liLane];
                if (liRow == 3)
                {
                    lfAcc = lfAx * lafB0[liLane] + lafB3[liLane];
                }
                lfAcc = lfAy * lafB1[liLane] + lfAcc;      // vmaddfp y*B1 + acc
                lapRowOut[liRow]->v[liLane] =
                    lfAz * lafB2[liLane] + lfAcc;          // vmaddfp z*B2 + acc
            }
        }
    }
}

// ===========================================================================
// Aggregate refs are traversed via the stack. Primitive refs go to the result buffer.
// ARTIST reloads the current slot for each field write before advancing the cursor.
RwBool VolumeBBoxQuery::AddVolumeRef(const Volume*                    lpVol,
                                     const math::vpu::Matrix44Affine* lpTm,
                                     const AABBox&                    arBBox,
                                     u32                              auTag,
                                     u8                               auNumTagBits)
{
    if (lpVol->GetType() != VOLUMETYPEAGGREGATE)
    {
        return AddPrimitiveRef(lpVol, lpTm, arBBox, auTag, auNumTagBits);
    }
    if (m_stackNext >= m_stackMax)
    {
        return 0;
    }

    m_stackVRefBuffer[m_stackNext].volume = lpVol;
    if (lpTm)
    {
        m_stackVRefBuffer[m_stackNext].tmContents = *lpTm;
        m_stackVRefBuffer[m_stackNext].tm = &m_stackVRefBuffer[m_stackNext].tmContents;
    }
    else
    {
        m_stackVRefBuffer[m_stackNext].tm = 0;
    }
    m_stackVRefBuffer[m_stackNext].bBox = arBBox;
    m_stackVRefBuffer[m_stackNext].tag = auTag;
    m_stackVRefBuffer[m_stackNext].numTagBits = auNumTagBits;
    ++m_stackNext;
    return 1;
}

// ===========================================================================
// ARTIST 0x82BB0478, called by AddVolumeRef and GetOverlaps.
RwBool VolumeBBoxQuery::AddPrimitiveRef(const Volume*                    lpVol,
                                        const math::vpu::Matrix44Affine* lpTm,
                                        const AABBox&                    arBBox,
                                        u32                              auTag,
                                        u8                               auNumTagBits)
{
    if (m_primNext >= m_primBufferSize)
    {
        return 0;
    }

    m_primVRefBuffer[m_primNext].volume = lpVol;
    if (lpTm)
    {
        m_primVRefBuffer[m_primNext].tmContents = *lpTm;
        m_primVRefBuffer[m_primNext].tm = &m_primVRefBuffer[m_primNext].tmContents;
    }
    else
    {
        m_primVRefBuffer[m_primNext].tm = 0;
    }
    m_primVRefBuffer[m_primNext].bBox = arBBox;
    m_primVRefBuffer[m_primNext].tag = auTag;
    m_primVRefBuffer[m_primNext].numTagBits = auNumTagBits;
    ++m_primNext;
    return 1;
}

// The query header, stack and result references precede the spatial-map workspace.
ResourceDescriptor VolumeBBoxQuery::GetResourceDescriptor(u32 luStackMax, u32 luResBufferSize)
{
    const u32 luSize = sizeof(VolumeBBoxQuery) + sizeof(VolRef) * (luStackMax + luResBufferSize)
                       + sizeof(Volume) * luResBufferSize + 0x27E0;
    return ResourceDescriptor(luSize, __alignof(VolumeBBoxQuery));
}

inline VolumeBBoxQuery::VolumeBBoxQuery(u32 luStackMax, u32 luResults, u32 luInstVolMax)
{
    m_stackMax = luStackMax;
    m_primBufferSize = luResults;
    m_instVolMax = luInstVolMax;

    m_stackVRefBuffer = reinterpret_cast<VolRef*>(this + 1);
    m_instVolPool = reinterpret_cast<Volume*>(m_stackVRefBuffer + luStackMax);
    m_primVRefBuffer = reinterpret_cast<VolRef*>(m_instVolPool + luInstVolMax);
    m_spatialMapQueryMem = m_primVRefBuffer + luResults;
}

VolumeBBoxQuery* VolumeBBoxQuery::Initialize(const Resource& lrResource, u32 luStackMax, u32 luResults)
{
    void* lpMemory = lrResource.m_baseResources[0];
    if (!lpMemory)
    {
        return 0;
    }
    return new (lpMemory) VolumeBBoxQuery(luStackMax, luResults, luResults);
}
// ===========================================================================
// rw::collision::VolumeBBoxQuery::GetOverlaps @ 0x82BBBDE8
// Called by: VolumeVolumeQuery::GetPrimitiveBBoxOverlaps (VolumeQuery.cpp).
//
// The bbox-query driver loop: consume the remaining input volumes and the
// aggregate-traversal stack until both are empty (Finished(), rwccore.h:2900)
// or a result buffer fills up. Per iteration:
//   1. input stage (only when no current ref and the stack is empty): skip
//      disabled volumes (m_flags bit 0), build the volume's AABB through its
//      vtable GetBBox (slot 1, tight=0), VMX-test it against the query box,
//      and AddVolumeRef the overlappers (tag 0 / numTagBits 0);
//   2. pop the stack into m_currVRef when it holds no live volume
//      (VolRef::operator= @ 0x82BB33E8), publish the ref's tag/numTagBits
//      into the query (m_tag/m_numTagBits);
//   3. primitives -> AddPrimitiveRef (fail = result buffer full -> abort);
//      aggregates -> compose the child transform and dispatch the
//      aggregate's BBoxOverlapQuery (vtable +0x18; 0 = out of space ->
//      abort). A completed aggregate query clears the resumable-state words
//      (m_curSpatialMapQuery, m_aggIndex) and retires the current ref.
// Returns m_primNext, the number of primitive refs staged this call.
//
// VMX decode (asm authoritative):
//   * AABB overlap mask: vcmpgtfp(volMin, queryMax) | vcmpgtfp(queryMin,
//     volMax) per lane; vsldoi(0, mask, 12) shifts the W lane out (result =
//     {0, sep.x, sep.y, sep.z}); vcmpeqfp. against zero + mfocrf bit 24
//     (CR6 "all equal") -> overlap iff NO xyz lane is separated. NaN lanes
//     compare false in vcmpgtfp exactly as scalar `>` does.
//   * Affine compose: see ComposeAffine above.
//   * Identity fast-path (null parent transform only): |m00-1| < FLT_EPSILON
//     && |m11-1| < FLT_EPSILON && vmsum3fp128(row3,row3) < 2^-128 -> pass a
//     NULL matrix to the aggregate instead of &vol->transform. fcmpu+bge
//     polarity preserved: an unordered (NaN) compare rejects the fast-path,
//     as scalar `<` does.
//
// rodata: flt_82001C98 = 1.0f (committed SeparatingDirection.cpp);
// flt_82180D6C / flt_82180D68 (the two epsilons above).
//
// Dead code kept OUT (noted, no observable effect): stw r27 -> a stack slot
// (var_E0) after the GetBBox dispatch, and the cmplwi r3,0 after the
// AddVolumeRef call (both results unused on every path).
// ===========================================================================
int VolumeBBoxQuery::GetOverlaps(VolumeBBoxQuery* lpThis)
{
    u32 luOutOfSpace = 0;                       // r22 (abort latch)

    lpThis->m_primNext     = 0;                 // stw 0 -> +0xCC
    lpThis->m_instVolCount = 0;                 // stw 0 -> +0xD8
    lpThis->m_numTagBits   = 0;                 // stb 0 -> +0xF0

    for (;;)                                    // loc_82BBBE2C
    {
        const u32 luCurrInput = lpThis->m_currInput;   // lwz +0x0C
        const u32 luNumInputs = lpThis->m_numInputs;   // lwz +0x08

        // Finished() (rwccore.h:2900): no inputs left, no live current ref,
        // empty stack -> done. (blt/bne/ble chain -- any live source keeps
        // going.)
        if (luCurrInput >= luNumInputs &&
            lpThis->m_currVRef.volume == 0 &&
            lpThis->m_stackNext == 0)
        {
            break;                              // ble cr6, loc_82BBC108
        }
        // A full result buffer aborts even with work remaining (the query is
        // resumable: m_currVRef / the stack / m_currInput keep their state).
        if (luOutOfSpace)
        {
            break;                              // bne cr6, loc_82BBC108
        }

        // ---- input stage: only when there is no staged work at all --------
        if (lpThis->m_currVRef.volume == 0 &&
            lpThis->m_stackNext == 0 &&
            luCurrInput < luNumInputs)
        {
            // lwz r9, 0(r31); slwi r11, r10, 2; lwzx: vol = m_inputVols[curr].
            const Volume* lpVol = lpThis->m_inputVols[luCurrInput];

            // lwz r9, 0x5C(r30); clrlwi. 31: skip disabled volumes
            // (VOLUMEFLAG_ISENABLED, bit 0).
            if ((VolumeFlags(lpVol) & 1u) == 0)
            {
                lpThis->m_currInput = luCurrInput + 1;
                continue;                       // b loc_82BBBE2C
            }

            // Optional parallel matrix array (lwz r10, 4(r31); beq -> NULL).
            const math::vpu::Matrix44Affine* lpMtx = 0;
            if (lpThis->m_inputMats)
            {
                lpMtx = lpThis->m_inputMats[luCurrInput];
            }

            // Vtable GetBBox (slot 1), tight = 0; result ignored. (A dead
            // stw 0 to a spilled stack slot follows the bctrl -- omitted.)
            AABBox lVolBBox;                    // var_C0/var_B0
            lpVol->GetBBox(lpMtx, 0, lVolBBox);

            // VMX overlap mask (see banner): separated iff volMin > queryMax
            // or queryMin > volMax on any of the x/y/z lanes; the vsldoi-by-12
            // drops the W lane from the test.
            const f32* lafVolMin   = lVolBBox.mMin.mV.v;
            const f32* lafVolMax   = lVolBBox.mMax.mV.v;
            const f32* lafQueryMin = lpThis->m_aabb.mMin.mV.v;
            const f32* lafQueryMax = lpThis->m_aabb.mMax.mV.v;

            bool lbSeparated = false;
            for (int li = 0; li < 3; ++li)
            {
                if (lafVolMin[li] > lafQueryMax[li] ||     // vcmpgtfp v13
                    lafQueryMin[li] > lafVolMax[li])       // vcmpgtfp v12
                {
                    lbSeparated = true;                    // vor lane != 0
                }
            }

            if (!lbSeparated)                   // mfocrf/extrwi bit 24 set
            {
                // Overlapper -> stage it (tag 0, numTagBits 0). The X360
                // compares the result (cmplwi r3, 0) but both paths fall
                // through -- return value unused.
                lpThis->AddVolumeRef(lpVol, lpMtx, lVolBBox, 0, 0);
            }

            ++lpThis->m_currInput;              // lwz/addi/stw +0x0C
            // falls through into the staged-work stage (loc_82BBBF4C)
        }

        // ---- staged-work stage (loc_82BBBF4C) -----------------------------
        if (lpThis->m_currVRef.volume == 0 && lpThis->m_stackNext == 0)
        {
            continue;                           // beq cr6, loc_82BBBE2C
        }

        // Pop the traversal stack into the current ref when it is empty
        // (VolRef::operator= @ 0x82BB33E8 -- bl rw__collision__VolRef__operator_).
        if (lpThis->m_currVRef.volume == 0)
        {
            const u32 luTop = lpThis->m_stackNext - 1;     // addi -1
            lpThis->m_stackNext = luTop;                   // stw +0xC0
            lpThis->m_currVRef  = lpThis->m_stackVRefBuffer[luTop];
        }

        // Publish the ref's tag context into the query (read by the
        // aggregate's BBoxOverlapQuery / AddPrimitiveRef downstream).
        lpThis->m_tag        = lpThis->m_currVRef.tag;        // +0xEC <- +0xB0
        lpThis->m_numTagBits = lpThis->m_currVRef.numTagBits; // +0xF0 <- +0xB4

        const Volume* lpCurVol = VolRefVolume(lpThis->m_currVRef);

        if (lpCurVol->GetType() != KU_VOLUMETYPE_AGGREGATE)
        {
            // ---- primitive: stage it in the result buffer ------------------
            // r5 = the ref's cached transform pointer (+0x04), r6 = the ref's
            // cached 32-byte bbox (this+0x90 == &m_currVRef.mu50), r7/r8 = the
            // tag words loaded above (still live in registers at the call).
            const AABBox& lrCurBBox =
                lpThis->m_currVRef.bBox;

            if (!lpThis->AddPrimitiveRef(lpCurVol,
                                         VolRefTransform(lpThis->m_currVRef),
                                         lrCurBBox,
                                         lpThis->m_currVRef.tag,
                                         lpThis->m_currVRef.numTagBits))
            {
                luOutOfSpace = 1;               // li r22, 1
                continue;                       // b loc_82BBBE2C
            }
            lpThis->m_currVRef.volume = 0; // stw 0 -> +0x40 (retire)
            continue;
        }

        // ---- aggregate: compose the child transform and recurse -----------
        const math::vpu::Matrix44Affine* lpParentTm =
            VolRefTransform(lpThis->m_currVRef);            // lwz +0x04

        math::vpu::Matrix44Affine lComposed;                // var_A0..var_70
        const math::vpu::Matrix44Affine* lpAggTm;

        if (lpParentTm)                                     // cmplwi r11, 0
        {
            // Full vspltw/vmulfp128/vmaddfp compose (see banner + helper).
            ComposeAffine(lComposed, *VolumeTransform(lpCurVol), *lpParentTm);
            lpAggTm = &lComposed;                           // addi r5, sp, var_A0
        }
        else
        {
            // No parent transform: pass the volume's own transform (r5 = the
            // volume image base) -- unless it is identity-close, in which
            // case pass NULL. fcmpu/bge polarity: any lane failing its
            // tolerance (or comparing unordered) keeps the transform.
            const math::vpu::Matrix44Affine* lpVolTm = VolumeTransform(lpCurVol);
            lpAggTm = lpVolTm;

            const f32* lafRow0 = lpVolTm->xAxis.mV.v;
            const f32* lafRow1 = lpVolTm->yAxis.mV.v;
            const f32* lafRow3 = lpVolTm->wAxis.mV.v;

            if (std::fabs(lafRow0[0] - 1.0f) < KF_IDENTITY_LANE_EPSILON &&  // lfs 0(r5)
                std::fabs(lafRow1[1] - 1.0f) < KF_IDENTITY_LANE_EPSILON)    // lfs 0x14(r5)
            {
                // vmsum3fp128 row3 . row3: squared translation length.
                const f32 lfTranslationSq = lafRow3[0] * lafRow3[0]
                                          + lafRow3[1] * lafRow3[1]
                                          + lafRow3[2] * lafRow3[2];
                if (lfTranslationSq < KF_IDENTITY_TRANSLATION_EPSILON)
                {
                    lpAggTm = 0;                            // mr r5, r27
                }
            }
        }

        // lwz r3, 0x44(r4): the aggregate; vtable +0x18 = BBoxOverlapQuery
        // (r3=aggregate, r4=this query, r5=matrix-or-NULL).
        Aggregate* lpAggregate = VolumeAggregate(lpCurVol);
        if (lpAggregate->BBoxOverlapQuery(lpThis, lpAggTm))
        {
            // Completed: clear the resumable aggregate-query state and
            // retire the current ref.
            lpThis->m_curSpatialMapQuery   = 0;  // stw 0 -> +0xE8
            lpThis->m_aggIndex             = 0;  // stw 0 -> +0xE0
            lpThis->m_currVRef.volume = 0;  // stw 0 -> +0x40
        }
        else
        {
            luOutOfSpace = 1;                    // beq loc_82BBBFC8: out of space
        }
    }

    return static_cast<int>(lpThis->m_primNext); // lwz r3, 0xCC(r31)
}

} // namespace collision
} // namespace rw
