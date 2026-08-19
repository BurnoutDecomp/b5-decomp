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
    // Collision-volume byte-offset views (the committed PrimitiveIntersect.cpp
    // precedent -- read by image offset until the full Volume layout lands;
    // canonical field homes rwccore.h:1599-1614).
    // -----------------------------------------------------------------------

    // Volume vtable slot 1 -- GetBBox (canonical Volume::VTable rwccore.h:1584;
    // signature volume.h:539 / rwccore.h:2444). X360 call site: r3=volume,
    // r4=matrix (may be NULL), r5=0 (tight), r6=&bbox out; result ignored.
    typedef RwBool (*VolumeGetBBoxFn)(const Volume*                    lpVolume,
                                      const math::vpu::Matrix44Affine* lpMtx,
                                      RwBool                           abTight,
                                      AABBox*                          lpBBox);

    struct VolumeVTableView
    {
        u32             muTypeID;    // slot 0  typeID
        VolumeGetBBoxFn mpGetBBox;   // slot 1  getBBox
    };

    // Console `lwz 0x40(vol)` = the descriptor pointer; on the host the slot holds the
    // type enum and the descriptor is gVolumeVTable[enum] (CollisionVolume.hpp
    // GetVolumeDescriptor; wave Q5 integration 2026-08-18).
    const VolumeVTableView* GetVolumeVTable(const Volume* lpVolume)
    {
        return reinterpret_cast<const VolumeVTableView*>(GetVolumeDescriptor(lpVolume));
    }

    // Volume::m_flags -- console byte +0x5C; bit 0 is VOLUMEFLAG_ISENABLED
    // (canonical rwccore.h:1474; the asm's clrlwi. r9, r9, 31).
    u32 VolumeFlags(const Volume* lpVolume)
    {
        return *reinterpret_cast<const u32*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x5C);
    }

    // Volume::transform -- the volume's relative transform IS the image base
    // (canonical rwccore.h:1599; the asm reads rows at vol+0x00..0x3F and
    // passes r5 = vol as the matrix pointer on the identity path).
    const math::vpu::Matrix44Affine* VolumeTransform(const Volume* lpVolume)
    {
        return reinterpret_cast<const math::vpu::Matrix44Affine*>(lpVolume);
    }

    // Volume::aggregateData.agg -- first word of the type union at console
    // byte +0x44 (canonical rwccore.h:1601-1609; lwz r3, 0x44(r4)).
    Aggregate* VolumeAggregate(const Volume* lpVolume)
    {
        return *reinterpret_cast<Aggregate* const*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x44);
    }

    // VolRef pointer-word views (the committed PrimitiveIntersect.cpp
    // helpers). Since waveQ5 C1 the stored words are HOST width, so these are
    // plain reinterpretations, not a re-widening of a truncated console word.
    const Volume* VolRefVolume(const VolRef& lrRef)
    {
        return reinterpret_cast<const Volume*>(lrRef.muVolumePtr);
    }

    const math::vpu::Matrix44Affine* VolRefTransform(const VolRef& lrRef)
    {
        return reinterpret_cast<const math::vpu::Matrix44Affine*>(lrRef.muTransformPtr);
    }

    // -----------------------------------------------------------------------
    // Aggregate dispatch: vtable pointer at Aggregate+0x20 (lwz r11, 0x20(r3);
    // canonical Aggregate layout rwccore.h:2402-2405: m_AABB +0x00..0x1F,
    // m_vTable +0x20), entry +0x18 = m_BBoxOverlapQuery (canonical VTable
    // rwccore.h:2376-2385). X360 call: r3=aggregate, r4=query, r5=matrix
    // (may be NULL). Returns 0 when the query ran out of buffer space.
    // -----------------------------------------------------------------------
    typedef RwBool (*AggregateBBoxOverlapQueryFn)(Aggregate*                       lpAggregate,
                                                  VolumeBBoxQuery*                 lpQuery,
                                                  const math::vpu::Matrix44Affine* lpMtx);

    struct AggregateVTableView
    {
        u32                         muType;                  // +0x00 m_type
        void*                       mpGetSize;               // +0x04
        u32                         muAlignment;             // +0x08
        RwBool                      mbIsProcedural;          // +0x0C
        void*                       mpUpdate;                // +0x10
        void*                       mpLineIntersectionQuery; // +0x14
        AggregateBBoxOverlapQueryFn mpBBoxOverlapQuery;      // +0x18
    };

    const AggregateVTableView* GetAggregateVTable(const Aggregate* lpAggregate)
    {
        return *reinterpret_cast<const AggregateVTableView* const*>(
            reinterpret_cast<const u8*>(lpAggregate) + 0x20);
    }

    // One lvx128/stvx128 pair: copy a 16-byte matrix row into the VolRef's
    // inline row storage (straight block move, no lane math).
    void CopyRow(VolRef::Vec4& lrDst, const math::vpu::VectorIntrinsic& arSrc)
    {
        lrDst.x = arSrc.mafLane[0];
        lrDst.y = arSrc.mafLane[1];
        lrDst.z = arSrc.mafLane[2];
        lrDst.w = arSrc.mafLane[3];
    }

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

        const f32* lafB0 = arParent.xAxis.mV.mafLane;
        const f32* lafB1 = arParent.yAxis.mV.mafLane;
        const f32* lafB2 = arParent.zAxis.mV.mafLane;
        const f32* lafB3 = arParent.wAxis.mV.mafLane;

        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const f32 lfAx = lapRowA[liRow]->mafLane[0];   // vspltw row, 0
            const f32 lfAy = lapRowA[liRow]->mafLane[1];   // vspltw row, 1
            const f32 lfAz = lapRowA[liRow]->mafLane[2];   // vspltw row, 2
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                // Rows 0-2: vmulfp128 x*B0; row 3: vmaddfp x*B0 + B3.
                f32 lfAcc = lfAx * lafB0[liLane];
                if (liRow == 3)
                {
                    lfAcc = lfAx * lafB0[liLane] + lafB3[liLane];
                }
                lfAcc = lfAy * lafB1[liLane] + lfAcc;      // vmaddfp y*B1 + acc
                lapRowOut[liRow]->mafLane[liLane] =
                    lfAz * lafB2[liLane] + lfAcc;          // vmaddfp z*B2 + acc
            }
        }
    }
}

// ===========================================================================
// rw::collision::VolumeBBoxQuery::AddVolumeRef @ 0x82BBBC20
//
// Push a volume reference: non-aggregate volumes tail-branch straight into
// AddPrimitiveRef (b rw__collision__VolumeBBoxQuery__AddPrimitiveRef, same
// argument registers); aggregates are staged as a 0x80-stride VolRef record
// on the traversal stack. The X360 body is the out-of-line compile of the
// canonical inline rwccore.h:2825-2851 (verbatim match, including the
// tm-null branch and the field order), so the reconstruction below is that
// canonical body spelled in the committed VolRef vocabulary.
//
// The only VMX here is the 4x lvx128/stvx128 transform-row copy (a straight
// 16-byte-per-row block move, no lane math); the 32-byte AABBox copy is four
// 8-byte integer ld/std pairs.
//
// NOTE (Hex-Rays bug, asm authoritative): the pseudocode's final store
// "...+116 = BYTE3(v16)" claims the tag-bit byte comes from the last bbox
// qword; the asm stb r8, 0x74(r10) stores the numTagBits ARGUMENT (r8).
//
// No rodata: the only constant is the inline immediate 6 (VOLUMETYPEAGGREGATE).
// ===========================================================================
RwBool VolumeBBoxQuery::AddVolumeRef(const Volume*                    lpVol,
                                     const math::vpu::Matrix44Affine* lpTm,
                                     const AABBox&                    arBBox,
                                     u32                              auTag,
                                     u8                               auNumTagBits)
{
    // lwz r10,0x40(r4); lwz r10,0(r10); cmpwi cr6,r10,6; beq/b: non-aggregate
    // volumes divert straight to the primitive-ref buffer (tail branch with
    // the same argument registers).
    if (GetVolumeVTable(lpVol)->muTypeID != KU_VOLUMETYPE_AGGREGATE)
    {
        return AddPrimitiveRef(lpVol, lpTm, arBBox, auTag, auNumTagBits);
    }

    // lwz 0xC0/0xC4; cmplw cr6; blt: traversal stack full -> reject.
    if (m_stackNext >= m_stackMax)
    {
        return 0;
    }

    // slwi r10, r10, 7: the stack records are 0x80-stride VolRefs
    // (sizeof(VolRef) static_asserts to 0x80).
    VolRef& lrRef = m_stackVRefBuffer[m_stackNext];

    // stwx r4 -> record +0x00 (the console's 32-bit pointer word; VolRef
    // carries it at HOST width since waveQ5 C1, so no truncation here -- the
    // readers below dereference exactly what was stored).
    lrRef.muVolumePtr = reinterpret_cast<uintptr_t>(lpVol);

    if (lpTm)                                       // cmplwi cr6, r5, 0
    {
        // 4x lvx128/stvx128 (tm +0x00/+0x10/+0x20/+0x30 -> record
        // +0x10..+0x4F): cache the transform rows inline, then aim the
        // record's transform pointer at that inline copy (stw r9 -> +0x04).
        CopyRow(lrRef.mRow0, lpTm->xAxis.mV);
        CopyRow(lrRef.mRow1, lpTm->yAxis.mV);
        CopyRow(lrRef.mRow2, lpTm->zAxis.mV);
        CopyRow(lrRef.mRow3, lpTm->wAxis.mV);
        lrRef.muTransformPtr = reinterpret_cast<uintptr_t>(&lrRef.mRow0);
    }
    else
    {
        lrRef.muTransformPtr = 0;                   // stw 0 -> +0x04
    }

    // 4x ld/std: the 32-byte AABBox block-copied into record +0x50..+0x6F
    // (the committed VolRef carries those bytes as mu50..mu68).
    std::memcpy(&lrRef.mu50, &arBBox, 4 * sizeof(u64));

    lrRef.muTag        = auTag;                     // stw r7 -> +0x70
    lrRef.muNumTagBits = auNumTagBits;              // stb r8 -> +0x74 (arg r8;
                                                    // the pseudocode's BYTE3(v16)
                                                    // is a Hex-Rays artefact)

    ++m_stackNext;                                  // lwz/addi/stw +0xC0
    return 1;                                       // li r3, 1
}

// ===========================================================================
// rw::collision::VolumeBBoxQuery::AddPrimitiveRef @ 0x82BB0478   (waveQ5 C1)
//
// Stage ONE primitive volume reference in the result buffer. This is the
// buffer VolumeVolumeQuery::GetPrimitiveBBoxOverlaps drains and
// PrimitiveBatchIntersect turns into GPInstances, so it is on the direct path
// from "the culler asked for a pair query" to "a contact exists".
//
// 66 instructions, no VMX arithmetic -- the only vector work is the same 4x
// lvx128/stvx128 transform-row block move AddVolumeRef does. Structurally the
// aggregate arm above with three substitutions: the m_primVRefBuffer /
// m_primNext / m_primBufferSize triple (+0xC8/+0xCC/+0xD0) instead of the
// stack triple (+0x30/+0xC0/+0xC4), no VOLUMETYPEAGGREGATE test, and the
// capacity guard placed BEFORE the volume-pointer store.
//
//   lwz r10,0xCC(r11) / lwz r9,0xD0(r11) / cmplw / blt   ; full -> li r3,0; blr
//   lwz r9,0xC8(r11) / slwi r10,r10,7 / stwx r4,r10,r9   ; rec+0x00 = volume
//   cmplwi cr6, r5, 0                                    ; transform present?
//     yes: 4x lvx128/stvx128 tm+0x00/0x10/0x20/0x30 -> rec+0x10..+0x4F
//          stw (rec+0x10), 4(rec)                        ; rec+0x04 = &rec+0x10
//     no : stw 0, 4(rec)                                 ; rec+0x04 = NULL
//   4x ld/std from r6                -> rec+0x50..+0x6F  ; the 32-byte AABBox
//   stw r7, 0x70(rec)                                    ; tag
//   stb r8, 0x74(rec)                                    ; tag bit count
//   lwz/addi/stw 0xCC(r11)                               ; ++m_primNext
//   li r3, 1
//
// NOTE (Hex-Rays bug, asm authoritative -- the identical artefact AddVolumeRef
// already documents): the pseudocode renders the last store as
// "...+116 = BYTE3(v16)", sourcing the byte from the final bbox qword. The asm
// is `stb r8, 0x74(r10)`, i.e. the numTagBits ARGUMENT.
//
// Canonical inline body: rwccore.h:2796-2818.
// ===========================================================================
RwBool VolumeBBoxQuery::AddPrimitiveRef(const Volume*                    lpVol,
                                        const math::vpu::Matrix44Affine* lpTm,
                                        const AABBox&                    arBBox,
                                        u32                              auTag,
                                        u8                               auNumTagBits)
{
    // lwz 0xCC/0xD0; cmplw cr6; blt: result buffer full -> reject. The guard
    // runs before ANY store, so a rejected call leaves the buffer untouched.
    if (m_primNext >= m_primBufferSize)
    {
        return 0;
    }

    // slwi r10, r10, 7: the result records are 0x80-stride VolRefs.
    VolRef& lrRef = m_primVRefBuffer[m_primNext];

    // stwx r4 -> record +0x00 (the console's 32-bit pointer word; VolRef
    // carries it at HOST width since waveQ5 C1, so no truncation here -- the
    // readers below dereference exactly what was stored).
    lrRef.muVolumePtr = reinterpret_cast<uintptr_t>(lpVol);

    if (lpTm)                                       // cmplwi cr6, r5, 0
    {
        // Cache the transform rows inline, then aim the record's transform
        // pointer at that inline copy (stw r9 -> +0x04).
        CopyRow(lrRef.mRow0, lpTm->xAxis.mV);
        CopyRow(lrRef.mRow1, lpTm->yAxis.mV);
        CopyRow(lrRef.mRow2, lpTm->zAxis.mV);
        CopyRow(lrRef.mRow3, lpTm->wAxis.mV);
        lrRef.muTransformPtr = reinterpret_cast<uintptr_t>(&lrRef.mRow0);
    }
    else
    {
        lrRef.muTransformPtr = 0;                   // stw 0 -> +0x04
    }

    // 4x ld/std: the 32-byte AABBox block-copied into record +0x50..+0x6F.
    std::memcpy(&lrRef.mu50, &arBBox, 4 * sizeof(u64));

    lrRef.muTag        = auTag;                     // stw r7 -> +0x70
    lrRef.muNumTagBits = auNumTagBits;              // stb r8 -> +0x74

    ++m_primNext;                                   // lwz/addi/stw +0xCC
    return 1;                                       // li r3, 1
}

// ===========================================================================
// rw::collision::VolumeBBoxQuery::GetResourceDescriptor @ 0x82BBBD38
//
// Static factory: fills the 5-entry rw::ResourceDescriptor block at lpOut.
// Pure scalar/integer code (no VMX). Same shape as the committed
// VolumeVolumeQuery::GetResourceDescriptor @ 0x82BB3A20 (VolumeQuery.cpp):
// all five (size, align) entries are first stamped (0, 1), then entry[0] is
// overwritten with (totalSize, 16).
//
// Size decode (add/mulli/slwi/addi chain, cross-checked against the buffer
// partition in Initialize @ 0x82BBBD90):
//   r11 = ((stackMax + resBufferSize + 2) << 7) + 0x60*resBufferSize + 0x27E0
//       =   0x100                        query-object header (the "+2" << 7)
//         + 0x80 * stackMax             stack VolRef records (0x80 stride)
//         + 0x60 * resBufferSize        instanced-volume pool (0x60/Volume)
//         + 0x80 * resBufferSize        result/primitive VolRef records
//         + 0x27E0 (10208)              spatial-map query workspace tail
//
// The 5-entry stamp loop is li r8,4 / addic. r8,-1 / bge: five iterations.
// The final store builds the (size,16) pair as one 8-byte stack word
// (stw/stw/ld/std) -- equivalent to the two u32 stores below.
// ===========================================================================
void* VolumeBBoxQuery::GetResourceDescriptor(void* lpOut, int liStackMax, int liResBufferSize)
{
    // add/addi/slwi/mulli/addi: total backing size for the query object, its
    // stack + result VolRef buffers, the instanced-volume pool, and the
    // spatial-map query workspace (breakdown in the banner).
    u32 luTotalSize = (static_cast<u32>(liStackMax + liResBufferSize + 2) << 7)
                    + 96u * static_cast<u32>(liResBufferSize)
                    + 10208u;                                       // 0x27E0

    // Initialise all five resource-descriptor entries to (size=0, align=1)
    // (li r8,4; addic./bge loop -- five iterations).
    u32* lpEntry = reinterpret_cast<u32*>(lpOut);
    for (int li = 4; li >= 0; --li)
    {
        lpEntry[0] = 0;   // size
        lpEntry[1] = 1;   // alignment
        lpEntry += 2;
    }

    // Entry[0] carries the real (size, alignment=16), stored on the console as
    // a single 8-byte word built on the stack (stw size / stw 0x10 / ld / std).
    u32* lpDesc = reinterpret_cast<u32*>(lpOut);
    lpDesc[0] = luTotalSize;
    lpDesc[1] = 16;

    return lpOut;
}

// ===========================================================================
// rw::collision::VolumeBBoxQuery::Initialize @ 0x82BBBD90
//
// Static factory: partitions the caller-owned backing buffer (*lppBuffer)
// into the query's working regions and returns the query handle (the buffer
// base interpreted as the query object). Pure scalar/pointer code (no VMX).
// Null backing base -> returns 0 (beq loc_82BBBDDC; li r3,0).
//
// Buffer partition (all offsets attested by the store chain; region sizes
// cross-check the GetResourceDescriptor total exactly):
//   base + 0x100                                   -> m_stackVRefBuffer
//   base + 0x100 + 0x80*stackMax                   -> m_instVolPool
//   ...          + 0x60*resBufferSize              -> m_primVRefBuffer
//   ...          + 0x80*resBufferSize              -> m_spatialMapQueryMem
// (No other member is written -- the cursor/count fields are primed later by
// the VolumeVolumeQuery driver / InitQuery, exactly as on the console.)
//
// FLAG (console units, same caveat as the GetPrimitiveBBoxOverlaps capacity
// note in VolumeQuery.cpp): the region strides are the console BYTE sizes --
// 0x100 query header, 0x80 per VolRef (holds natively: sizeof(VolRef)
// static_asserts to 0x80 on x64), 0x60 per instanced Volume. On x64 the
// pointer-widened query header and the Volume image are larger than the
// console constants, so the whole sizing chain (GetResourceDescriptor ->
// this partition) needs one coordinated widening pass. Kept console-exact
// per the translation contract.
// ===========================================================================
void* VolumeBBoxQuery::Initialize(void** lppBuffer, int liStackMax, int liResBufferSize)
{
    // lwz r11, 0(r3); cmplwi; beq: null backing base -> no query.
    VolumeBBoxQuery* lpQuery = reinterpret_cast<VolumeBBoxQuery*>(*lppBuffer);
    if (!lpQuery)
    {
        return 0;                                         // li r3,0
    }

    const u32 luStackMax = static_cast<u32>(liStackMax);
    const u32 luResults  = static_cast<u32>(liResBufferSize);

    lpQuery->m_stackMax       = luStackMax;               // stw r4 -> +0xC4
    lpQuery->m_primBufferSize = luResults;                // stw r5 -> +0xD0
    lpQuery->m_instVolMax     = luResults;                // stw r5 -> +0xDC

    // addi r10, r11, 0x100: the stack VolRef records start one 0x100 query
    // header past the base.
    u8* lpStackBuffer = reinterpret_cast<u8*>(lpQuery) + 0x100;
    lpQuery->m_stackVRefBuffer = reinterpret_cast<VolRef*>(lpStackBuffer); // stw -> +0x30

    // slwi r9, r4, 7: 0x80-stride stack records, then the instanced-volume
    // pool (mulli r7, r5, 0x60: console Volume stride 0x60 -- see FLAG above),
    // then the result VolRef records (slwi r8, r5, 7), then the spatial-map
    // query workspace tail.
    u8* lpInstVolPool = lpStackBuffer + (luStackMax << 7);
    lpQuery->m_instVolPool = reinterpret_cast<Volume*>(lpInstVolPool);     // stw -> +0xD4

    u8* lpPrimBuffer = lpInstVolPool + 96u * luResults;
    lpQuery->m_primVRefBuffer = reinterpret_cast<VolRef*>(lpPrimBuffer);   // stw -> +0xC8

    lpQuery->m_spatialMapQueryMem = lpPrimBuffer + (luResults << 7);       // stw -> +0xE4

    return lpQuery;                                       // mr r3, r11
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
            lpThis->m_currVRef.muVolumePtr == 0 &&
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
        if (lpThis->m_currVRef.muVolumePtr == 0 &&
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
            GetVolumeVTable(lpVol)->mpGetBBox(lpVol, lpMtx, 0, &lVolBBox);

            // VMX overlap mask (see banner): separated iff volMin > queryMax
            // or queryMin > volMax on any of the x/y/z lanes; the vsldoi-by-12
            // drops the W lane from the test.
            const f32* lafVolMin   = lVolBBox.mMin.mV.mafLane;
            const f32* lafVolMax   = lVolBBox.mMax.mV.mafLane;
            const f32* lafQueryMin = lpThis->m_aabb.mMin.mV.mafLane;
            const f32* lafQueryMax = lpThis->m_aabb.mMax.mV.mafLane;

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
        if (lpThis->m_currVRef.muVolumePtr == 0 && lpThis->m_stackNext == 0)
        {
            continue;                           // beq cr6, loc_82BBBE2C
        }

        // Pop the traversal stack into the current ref when it is empty
        // (VolRef::operator= @ 0x82BB33E8 -- bl rw__collision__VolRef__operator_).
        if (lpThis->m_currVRef.muVolumePtr == 0)
        {
            const u32 luTop = lpThis->m_stackNext - 1;     // addi -1
            lpThis->m_stackNext = luTop;                   // stw +0xC0
            lpThis->m_currVRef  = lpThis->m_stackVRefBuffer[luTop];
        }

        // Publish the ref's tag context into the query (read by the
        // aggregate's BBoxOverlapQuery / AddPrimitiveRef downstream).
        lpThis->m_tag        = lpThis->m_currVRef.muTag;        // +0xEC <- +0xB0
        lpThis->m_numTagBits = lpThis->m_currVRef.muNumTagBits; // +0xF0 <- +0xB4

        const Volume* lpCurVol = VolRefVolume(lpThis->m_currVRef);

        if (GetVolumeVTable(lpCurVol)->muTypeID != KU_VOLUMETYPE_AGGREGATE)
        {
            // ---- primitive: stage it in the result buffer ------------------
            // r5 = the ref's cached transform pointer (+0x04), r6 = the ref's
            // cached 32-byte bbox (this+0x90 == &m_currVRef.mu50), r7/r8 = the
            // tag words loaded above (still live in registers at the call).
            const AABBox& lrCurBBox =
                *reinterpret_cast<const AABBox*>(&lpThis->m_currVRef.mu50);

            if (!lpThis->AddPrimitiveRef(lpCurVol,
                                         VolRefTransform(lpThis->m_currVRef),
                                         lrCurBBox,
                                         lpThis->m_currVRef.muTag,
                                         lpThis->m_currVRef.muNumTagBits))
            {
                luOutOfSpace = 1;               // li r22, 1
                continue;                       // b loc_82BBBE2C
            }
            lpThis->m_currVRef.muVolumePtr = 0; // stw 0 -> +0x40 (retire)
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

            const f32* lafRow0 = lpVolTm->xAxis.mV.mafLane;
            const f32* lafRow1 = lpVolTm->yAxis.mV.mafLane;
            const f32* lafRow3 = lpVolTm->wAxis.mV.mafLane;

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
        if (GetAggregateVTable(lpAggregate)->mpBBoxOverlapQuery(lpAggregate, lpThis, lpAggTm))
        {
            // Completed: clear the resumable aggregate-query state and
            // retire the current ref.
            lpThis->m_curSpatialMapQuery   = 0;  // stw 0 -> +0xE8
            lpThis->m_aggIndex             = 0;  // stw 0 -> +0xE0
            lpThis->m_currVRef.muVolumePtr = 0;  // stw 0 -> +0x40
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
