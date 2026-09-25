#include "vendor/renderware/collision/VolumeQuery.hpp"

#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"        // VolRef1xN / PPIR / PrimitiveBatchIntersect
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"   // the embedded sub-queries
#include "vendor/renderware/collision/VolumeQueryHostLayout.hpp"   // NOT X360: the host carve sizes

#include <cstring>  // memset
#include <cstdio>   // snprintf ([vvq] DIAG only)
#include <cstdlib>  // getenv   ([vvq] DIAG only)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([vvq] DIAG only); WriteToLog (the [vlq] traps)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (the [vlq] traps, NOT X360)

namespace rw
{
namespace collision
{

// ===========================================================================
// NOT X360: host layout, the console carves at +0x50.  (2026-09-25, crash parity FX-FOLLOWUPS)
//
// The query object is built IN PLACE at the base of its backing store, and the two embedded
// VolumeBBoxQuery sub-queries are carved right behind it. That offset is the object's SIZE, not a
// field offset:
//   * the DWARF member block (volumevolumequery.h:191-220) ends with m_bBoxQueryBtoA at +0x44, so
//     the console object is 0x48 bytes, and the sub-queries (16-byte aligned: they embed an AABBox
//     and a VolRef) begin at the next 16-byte boundary, 0x50;
//   * Construct @0x82BB38F0: 0x82BB390C `addi r11, r31, 0x50` -> `stw r11, 0x40(r31)` m_bBoxQueryAtoB;
//   * GetResourceDescriptor @0x82BB3A20: 0x82BB3A4C `addi r11, r11, 0x28` + 0x82BB3A58 `slwi r11, 1`
//     == 2 * (bboxDesc + 0x28) == 2 * bboxDesc + 0x50 -- the same header folded into the per-side term.
// On x64 the pointer members widen and the object is 0x88 bytes (MEASURED: m_intersectionBuffer @0x50,
// m_intersectionBufferMaxSize @0x58, m_queryVol @0x60, m_queryMtx @0x68, m_bBoxQueryAtoB @0x70,
// m_bBoxQueryBtoA @0x78). Carving at +0x50 put sub-query A ON TOP of those members:
// GetPrimitiveBBoxOverlaps' first-pass priming (A's m_inputVols / m_inputMats / m_numInputs /
// m_currInput / m_aabb) overwrote them, and its second-pass read of m_bBoxQueryBtoA then wrote
// through the AABB's float bits -- a silent wild write on this build's below-4GB heap -- before
// PrimitiveBatchIntersect was handed m_intersectionBuffer == a dead stack slot. The host carve
// starts at the host size rounded to the same 16-byte alignment, and the descriptor total grows by
// the same delta; every site that derives a sub-query address or the descriptor total from the
// header uses this ONE constant (Construct, GetResourceDescriptor; Initialize reads the carved
// handles back and derives nothing). The constant, and the two report-region sizes that widen the
// same way (the 1xN staging, the instancing scratch), live in VolumeQueryHostLayout.hpp.
// ===========================================================================
static_assert(VolumeVolumeQueryResourceSize(100u) == KU_VOLUME_VOLUME_QUERY_HOST_SIZE_R100,
              "VolumeQuery.hpp's compile-time host size for 100/100 is the total GetResourceDescriptor returns");

// ===========================================================================
// rw::collision::VolumeVolumeQuery::Construct  @ 0x82BB38F0
//
// In-place construction over the backing buffer that begins at `this`. Pure
// pointer/scalar arithmetic (no VMX): caches the result count, points the two
// embedded VolumeBBoxQuery sub-queries at this+0x50 / this+0x50+descSize, and
// lays the report buffer out behind them. liResults (a3) drives both the
// report-buffer stride (1872 bytes/result == the 0x750 PPIR stride) and the
// sub-query stride.
//
//   stw  a3, 0x20(this)                 -> m_volRefPairBufferSize = liResults
//   addi r11, this, 0x50                -> m_bBoxQueryAtoB = this + 0x50
//   r10 = VolumeBBoxQuery::GetResourceDescriptor(scratch); descSize = *r10
//   stw  0,  0x10(this)                 -> m_cullTable                  = 0
//   stw  a3, 0x34(this)                 -> m_intersectionBufferMaxSize  = liResults
//   stw  (A + descSize),        0x44    -> m_bBoxQueryBtoA
//   r10 = A + 2*descSize;  stw r10,0x18 -> m_volRefPairBuffer = A + 2*descSize
//   stw  (8*a3 + r10),          0x30    -> m_intersectionBuffer
//   stw  (1872*a3 + 8*a3 + r10),0x2C    -> m_instancingSPR
// ===========================================================================
VolumeVolumeQuery* VolumeVolumeQuery::Construct(int /*liVolumes*/, int liResults)
{
    m_volRefPairBufferSize = static_cast<u32>(liResults);         // +0x20

    // `addi r11, r31, 0x50` on the console: the sub-queries start right behind the object.
    // NOT X360: host layout -- see KU_VOLUME_VOLUME_QUERY_HEADER_SIZE above.
    u8* lpSubQueryRegion = reinterpret_cast<u8*>(this) + KU_VOLUME_VOLUME_QUERY_HEADER_SIZE;
    m_bBoxQueryAtoB = reinterpret_cast<VolumeBBoxQuery*>(lpSubQueryRegion); // +0x40

    // The descriptor's first word is the per-side VolumeBBoxQuery backing size.
    u32 laDescriptor[12];
    VolumeBBoxQuery::GetResourceDescriptor(laDescriptor, liResults, liResults);
    u32 luDescSize = laDescriptor[0];

    m_cullTable                 = nullptr;                        // +0x10
    m_intersectionBufferMaxSize = liResults;                      // +0x34

    // Side-B sub-query sits one descriptor past side A.
    m_bBoxQueryBtoA = reinterpret_cast<VolumeBBoxQuery*>(lpSubQueryRegion + luDescSize); // +0x44

    // Report buffer starts two descriptors past side A. The staged VolRef1xN
    // groups lead it; the 0x750-stride PrimitivePairIntersectResult array sits
    // behind them, and the GPInstance instancing scratch behind that. The console
    // stages into 8*results bytes (`8*a3`), the budget GetPrimitiveBBoxOverlaps
    // meters in console words.
    // NOT X360: host GPInstance / VolRef widths -- the host 1xN groups are 16 + 8n
    // bytes against the console's 12 + 4n, so the region is
    // VolumeVolumeQueryStagingSize(results) == 16*results (VolumeQueryHostLayout.hpp).
    u8* lpReportBase = lpSubQueryRegion + (2u * luDescSize);
    m_volRefPairBuffer = reinterpret_cast<VolRef1xN*>(lpReportBase);        // +0x18

    const u32 luStagingSize = VolumeVolumeQueryStagingSize(static_cast<u32>(liResults));
    m_intersectionBuffer = reinterpret_cast<PrimitivePairIntersectResult*>(
        lpReportBase + luStagingSize);                                      // +0x30
    m_instancingSPR = reinterpret_cast<GPInstance*>(
        lpReportBase + (KU_PRIMITIVE_PAIR_INTERSECT_RESULT_STRIDE * static_cast<u32>(liResults))
                     + luStagingSize);                                      // +0x2C

    return this;
}

// ===========================================================================
// rw::collision::VolumeVolumeQuery::Initialize  @ 0x82BB3980
//
// Static factory: lppBuffer[0] is the backing-store base. When non-null, the
// query is constructed in place over it; the two embedded VolumeBBoxQuery
// sub-queries are then initialised from per-side buffer tables whose [0] slot
// holds the sub-query handle (this+0x40 / this+0x44) and whose [1..4] slots are
// zeroed. Returns the constructed query handle.
// ===========================================================================
void* VolumeVolumeQuery::Initialize(void** lppBuffer, int liVolumes, int liResults)
{
    VolumeVolumeQuery* lpQuery;
    if (*lppBuffer)
    {
        lpQuery = reinterpret_cast<VolumeVolumeQuery*>(*lppBuffer)->Construct(liVolumes, liResults);
    }
    else
    {
        lpQuery = nullptr;
    }

    // --- side-A sub-query ---
    void* laBufferA[5];
    laBufferA[0] = lpQuery->m_bBoxQueryAtoB;   // *(this+0x40)
    std::memset(&laBufferA[1], 0, 4 * sizeof(void*));
    VolumeBBoxQuery::Initialize(laBufferA, liVolumes, liResults);

    // --- side-B sub-query ---
    void* laBufferB[5];
    laBufferB[0] = lpQuery->m_bBoxQueryBtoA;   // *(this+0x44)
    std::memset(&laBufferB[1], 0, 4 * sizeof(void*));
    VolumeBBoxQuery::Initialize(laBufferB, liVolumes, liResults);

    return lpQuery;
}

// ===========================================================================
// rw::collision::VolumeVolumeQuery::GetResourceDescriptor  @ 0x82BB3A20
//
// Static factory: fills the 5-entry rw::ResourceDescriptor block at lpOut. Each
// of the five (size, align) entries is first stamped (0, 1); entry[0] is then
// overwritten with (totalSize, 16). totalSize = 2*(bboxDescSize + 40) + 2072*a3
// + 192 on the console, where bboxDescSize is the side sub-query's descriptor size
// and 2*40 is the 0x50 query header (see KU_VOLUME_VOLUME_QUERY_HEADER_SIZE).
// ===========================================================================
void* VolumeVolumeQuery::GetResourceDescriptor(void* lpOut, int /*liVolumes*/, int liResults)
{
    // Side sub-query descriptor (first word = its backing size).
    u32 laBBoxDesc[12];
    VolumeBBoxQuery::GetResourceDescriptor(laBBoxDesc, liResults, liResults);
    u32 luBBoxDescSize = laBBoxDesc[0];

    // Total backing size for the object, the two sub-queries and the report buffer. The console
    // folds its 0x50 header into `2 * (bboxDesc + 0x28)` (0x82BB3A4C / 0x82BB3A58) and its report
    // buffer into `2072 * a3 + 192` == 8*a3 staging + 1872*a3 results + 192*(a3 + 1) instancing.
    // NOT X360: host layout -- the header is KU_VOLUME_VOLUME_QUERY_HEADER_SIZE, the size Construct
    // carves behind; the staging and instancing regions are the host GPInstance / VolRef widths
    // (VolumeQueryHostLayout.hpp). The results stride is native.
    const u32 luResults   = static_cast<u32>(liResults);
    u32       luTotalSize = KU_VOLUME_VOLUME_QUERY_HEADER_SIZE
                          + 2u * luBBoxDescSize
                          + VolumeVolumeQueryStagingSize(luResults)
                          + KU_PRIMITIVE_PAIR_INTERSECT_RESULT_STRIDE * luResults
                          + VolumeVolumeQueryInstancingSize(luResults);

    // Initialise all five resource-descriptor entries to (size=0, align=1).
    u32* lpEntry = reinterpret_cast<u32*>(lpOut);
    for (int li = 4; li >= 0; --li)
    {
        lpEntry[0] = 0;   // size
        lpEntry[1] = 1;   // alignment
        lpEntry += 2;
    }

    // Entry[0] carries the real (size, alignment=16).
    u32* lpDesc = reinterpret_cast<u32*>(lpOut);
    lpDesc[0] = luTotalSize;
    lpDesc[1] = 16;

    return lpOut;
}

// ===========================================================================
// rw::collision::VolumeVolumeQuery::GetPrimitiveBBoxOverlaps  @ 0x82BB3AB0
// (wave 2: the wave-1 declaration-only VMX FLAG is lifted -- the body was
// decoded in the dedicated VMX pass.)
// ===========================================================================

namespace
{
    // -----------------------------------------------------------------------
    // Collision-volume dispatch, reached through the pointer at Volume+0x40
    // (`lwz r10, 0x40(r3)`) -- the same TU-local view the committed
    // PrimitiveIntersect.cpp uses (canonical Volume::VTable, rwccore.h:1584:
    // slot 0 typeID, slot 1 getBBox, ... slot 5 createGPInstance).
    // GetBBox canonical signature (rwccore.h / DWARF volume.h:539):
    //   RwBool GetBBox(const Matrix44Affine*, RwBool, AABBox&) const
    // X360 call sites here: r3=volume, r4=matrix (may be NULL), r5=0,
    // r6=&bbox out (`lwz r11, 4(r10); mtctr; bctrl`).
    // -----------------------------------------------------------------------
    typedef RwBool (*VolumeGetBBoxFn)(const Volume*                    lpVolume,
                                      const math::vpu::Matrix44Affine* lpMtx,
                                      RwBool                           abInstanced,
                                      AABBox*                          lpBBox);

    struct VolumeVTableView
    {
        void*           mpTypeID;    // slot 0  typeID (unreferenced here)
        VolumeGetBBoxFn mpGetBBox;   // slot 1  getBBox
    };

    // Console `lwz 0x40(vol)` = the descriptor pointer; on the host the slot holds the
    // type enum and the descriptor is gVolumeVTable[enum] (CollisionVolume.hpp
    // GetVolumeDescriptor; wave Q5 integration 2026-08-18).
    const VolumeVTableView* GetVolumeVTable(const Volume* lpVolume)
    {
        return reinterpret_cast<const VolumeVTableView*>(GetVolumeDescriptor(lpVolume));
    }

    // Volume::groupID -- canonical rwccore.h:1612, console byte +0x54
    // (transform 0x40 + vTable word + 12-byte type union + radius). Read by
    // image offset until the full Volume layout lands (the committed
    // CollisionVolume.hpp image is a flagged placeholder), matching the
    // PrimitiveIntersect.cpp byte-offset precedent.
    u32 VolumeGroupID(const Volume* lpVolume)
    {
        return *reinterpret_cast<const u32*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x54);
    }

    // VolRef +0x00 volume word -> Volume* (same widening helper as the
    // committed PrimitiveIntersect.cpp).
    const Volume* VolRefVolume(const VolRef& lrRef)
    {
        return reinterpret_cast<const Volume*>(static_cast<uintptr_t>(lrRef.muVolumePtr));
    }

    // -----------------------------------------------------------------------
    // VolRef cached-AABB views: the 0x82BB3AB0 body attests that the four
    // 8-byte fields at VolRef +0x50..+0x6F are the referenced volume's cached
    // bounding box (min row @+0x50, max row @+0x60): the enclosing-box fold
    // vminfp's the rows at [ref+0x50] and vmaxfp's the rows at [ref+0x60]
    // (r23 = 0x10), and the overlap test compares them as float lanes.
    // (Suggested VolRef.hpp tightening: mu50/mu58 -> Vec4 mBBoxMin, mu60/mu68
    // -> Vec4 mBBoxMax; the views below keep the committed layout untouched.)
    // -----------------------------------------------------------------------
    const Vec4& VolRefBBoxMin(const VolRef& lrRef)
    {
        return *reinterpret_cast<const Vec4*>(&lrRef.mu50);
    }

    const Vec4& VolRefBBoxMax(const VolRef& lrRef)
    {
        return *reinterpret_cast<const Vec4*>(&lrRef.mu60);
    }

    // AABBox corner rows viewed as the directory's shared scalar Vec4 (both
    // are one 16-byte VMX row; AABBox carries them as math::vpu::Vector3).
    Vec4& BoxMin(AABBox& arBox) { return *reinterpret_cast<Vec4*>(&arBox.mMin); }
    Vec4& BoxMax(AABBox& arBox) { return *reinterpret_cast<Vec4*>(&arBox.mMax); }

    static_assert(sizeof(AABBox) == 0x20, "AABBox must be two 16-byte corner rows");

    // vsubfp / vaddfp / vminfp / vmaxfp: per-lane, all four lanes (the pad
    // vector's W lane is 0 so W rides through the fatten unchanged).
    inline Vec4 Sub(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }

    inline Vec4 Add(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x + b.x;
        r.y = a.y + b.y;
        r.z = a.z + b.z;
        r.w = a.w + b.w;
        return r;
    }

    inline Vec4 Min4(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = (a.x < b.x) ? a.x : b.x;
        r.y = (a.y < b.y) ? a.y : b.y;
        r.z = (a.z < b.z) ? a.z : b.z;
        r.w = (a.w < b.w) ? a.w : b.w;
        return r;
    }

    inline Vec4 Max4(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = (a.x > b.x) ? a.x : b.x;
        r.y = (a.y > b.y) ? a.y : b.y;
        r.z = (a.z > b.z) ? a.z : b.z;
        r.w = (a.w > b.w) ? a.w : b.w;
        return r;
    }
} // namespace

// ===========================================================================
// Per input volume (resumable at m_currInput while staging space remains):
//   1. build both sides' AABBs through the collision-volume vtable (slot 1,
//      getBBox(matrix, 0, &box)) and fatten each by the {pad,pad,pad,0} splat
//      (vsubfp128/vaddfp128 against v127);
//   2. compare the two UNfattened box volumes (the scalar lfs reads precede
//      the fatten stores) and make the LARGER side the bbox-query input of
//      the first pass, querying it against the smaller side's fattened box
//      (volumesSwapped records when the query volume was the smaller);
//   3. fold the enclosing box of every first-pass result's cached VolRef AABB
//      (vminfp/vmaxfp over the rows at ref+0x50/+0x60), fatten it, and run
//      the second pass with the smaller side's volume against it;
//   4. for every second-pass result, stage a VolRef1xN group headed by that
//      VolRef, appending each first-pass VolRef whose cached AABB overlaps
//      the fattened second-side AABB on x, y and z (the vcmpgtfp x2 / vor /
//      vsldoi-W-drop / vcmpeqfp.-all-clear mask test), subject to the
//      cull-table bit (muHeight * groupID(side2) + groupID(side1)) and the
//      byte budget (16-byte floor per group, 12-byte header refunded when a
//      group stays empty, 4 bytes per staged pair).
//
// Returns the total staged pair count (m_volRefPairCount).
//
// x64 capacity width: the staging budget keeps the console BYTE arithmetic
// exactly (budget = 8*m_volRefPairBufferSize; 12 bytes per group header, 4 per
// staged pair -- console words), so the staging DECISIONS are the console's.
// The host VolRef1xN groups are pointer-widened (16-byte header + 8 bytes/pair)
// and write up to 2 * budget - 8 host bytes; Construct / GetResourceDescriptor
// carve the region at that host size (VolumeVolumeQueryStagingSize,
// VolumeQueryHostLayout.hpp -- resolved 2026-09-25, FX-FOLLOWUPS).
//
// The X360 body also parks a zero in a dead stack slot next to the fattened
// second-side box (stw r30, var_C0, the Hex-Rays "v87[4] = 0.0") that nothing
// reads back; no C++ equivalent is emitted (ComputeContactPoints precedent).
// ===========================================================================
int VolumeVolumeQuery::GetPrimitiveBBoxOverlaps()
{
    // --- the fatten splat {pad, pad, pad, 0} (v127) ------------------------
    Vec4 lvPad;
    lvPad.x = m_padding;                                        // stfs f0, var_120
    lvPad.y = m_padding;                                        // stfs f0, var_11C
    lvPad.z = m_padding;                                        // stfs f0, var_118
    lvPad.w = 0.0f;                                             // stw r30, var_114

    // --- query-side AABB via the volume vtable (slot 1) --------------------
    AABBox lQueryBBox;                                          // var_100/var_F0
    GetVolumeVTable(m_queryVol)->mpGetBBox(m_queryVol, m_queryMtx, 0, &lQueryBBox);

    // --- staging byte budget over the pair buffer --------------------------
    u32 luBudget = m_volRefPairBufferSize << 3;                 // slwi r27, r11, 3

    Vec4& lrQueryMin = BoxMin(lQueryBBox);
    Vec4& lrQueryMax = BoxMax(lQueryBBox);

    // Box-volume product of the UNfattened query box (the lfs reads at
    // 0x82BB3B10..0x82BB3B4C precede the fattened stvx128 stores).
    f32 lfQueryVolume = (lrQueryMax.z - lrQueryMin.z)
                      * (lrQueryMax.y - lrQueryMin.y);          // fsubs x2 + fmuls
    lfQueryVolume *= (lrQueryMax.x - lrQueryMin.x);             // fmuls f31

    // Fatten the query box in place (vsubfp128 v0 / vaddfp128 v13).
    lrQueryMin = Sub(lrQueryMin, lvPad);
    lrQueryMax = Add(lrQueryMax, lvPad);

    VolRef1xN* lpStage      = m_volRefPairBuffer;               // r28 (staging cursor)
    VolRef*    lapRefsAtoB  = m_bBoxQueryAtoB->m_primVRefBuffer; // r24 = [0x40(r31)+0xC8]
    VolRef*    lapRefsBtoA  = m_bBoxQueryBtoA->m_primVRefBuffer; // r22 = [0x44(r31)+0xC8]

    m_volRefPairCount = 0;                                      // stw r30, 0x1C(r31)
    m_volRef1xNCount  = 0;                                      // stw r30, 0x28(r31)

    while (luBudget >= 0x10)                                    // cmplwi cr6, r27, 0x10
    {
        const u32 luInput = m_currInput;                        // lwz r10, 0xC(r31)
        if (luInput >= m_numInputs)                             // cmplw cr6 / bge
            break;

        // Per-iteration one-element (volume, matrix) input arrays handed to
        // the embedded sub-queries (the console's var_124/var_12C pair feeds
        // the first pass, var_128/var_130 the second; both live across the
        // GetOverlaps calls).
        const Volume*                    lpVol1 = m_queryVol;   // var_124
        const math::vpu::Matrix44Affine* lpMtx1 = m_queryMtx;   // var_12C
        const Volume*                    lpVol2 = m_inputVols[luInput];  // var_128 (lwzx r3)
        const math::vpu::Matrix44Affine* lpMtx2 =
            (m_inputMats != 0) ? m_inputMats[luInput] : 0;      // var_130 (beq -> r4 = 0)

        // Input-side AABB (dispatched on the input volume, pre-swap).
        AABBox lInputBBox;                                      // var_E0/var_D0
        GetVolumeVTable(lpVol2)->mpGetBBox(lpVol2, lpMtx2, 0, &lInputBBox);

        RwBool        liSwapped     = 0;                        // r25 = r30
        const AABBox* lpSmallerBBox = &lInputBBox;              // r29 = &var_E0

        Vec4& lrInputMin = BoxMin(lInputBBox);
        Vec4& lrInputMax = BoxMax(lInputBBox);

        // Box-volume product of the UNfattened input box (same load-before-
        // store scheduling as the query box).
        f32 lfInputVolume = (lrInputMax.z - lrInputMin.z)
                          * (lrInputMax.y - lrInputMin.y);      // fmuls f0, f13, f0
        lfInputVolume *= (lrInputMax.x - lrInputMin.x);         // fmuls f0, f0, f12

        // Fatten the input box in place.
        lrInputMin = Sub(lrInputMin, lvPad);                    // vsubfp128 v0
        lrInputMax = Add(lrInputMax, lvPad);                    // vaddfp128 v13

        if (lfQueryVolume < lfInputVolume)                      // fcmpu cr6; bge skips
        {
            // The query volume is the smaller side: swap it onto the second
            // pass and query the (larger) input volume against ITS box.
            lpSmallerBBox = &lQueryBBox;                        // r29 = &var_100
            liSwapped     = 1;                                  // mr r25, r26
            lpVol1 = lpVol2;                                    // var_124 <- var_128
            lpMtx1 = lpMtx2;                                    // var_12C <- var_130
            lpVol2 = m_queryVol;                                // var_128 <- 0x38(r31)
            lpMtx2 = m_queryMtx;                                // var_130 <- 0x3C(r31)
        }

        // --- prime the first (larger-side) sub-query -----------------------
        VolumeBBoxQuery* lpQueryAtoB = m_bBoxQueryAtoB;         // lwz r11, 0x40(r31)
        lpQueryAtoB->m_inputMats =
            (lpMtx1 != 0) ? &lpMtx1 : 0;                        // stw r10, 4(r11)
        lpQueryAtoB->m_inputVols            = &lpVol1;          // stw r9, 0(r11)
        lpQueryAtoB->m_numInputs            = 1;                // stw r26, 8(r11)
        lpQueryAtoB->m_currInput            = 0;                // stw r30, 0xC(r11)
        lpQueryAtoB->m_stackNext            = 0;                // stw r30, 0xC0(r11)
        lpQueryAtoB->m_primNext             = 0;                // stw r30, 0xCC(r11)
        lpQueryAtoB->m_currVRef.muVolumePtr = 0;                // stw r30, 0x40(r11)
        lpQueryAtoB->m_aggIndex             = 0;                // stw r30, 0xE0(r11)
        lpQueryAtoB->m_curSpatialMapQuery   = 0;                // stw r30, 0xE8(r11)
        lpQueryAtoB->m_instVolCount         = 0;                // stw r30, 0xD8(r11)
        lpQueryAtoB->m_aabb                 = *lpSmallerBBox;   // 32-byte ld/std x4 from r29
        lpQueryAtoB->m_tag                  = 0;                // stw r30, 0xEC(r11)
        lpQueryAtoB->m_numTagBits           = 0;                // stb r30, 0xF0(r11)

        const u32 luNumAtoB =
            static_cast<u32>(VolumeBBoxQuery::GetOverlaps(lpQueryAtoB)); // bl; mr. r29, r3
        if (luNumAtoB != 0)                                     // beq -> next input
        {
            // --- fold the enclosing box of the first-pass result AABBs -----
            Vec4 lvFoldMin = VolRefBBoxMin(lapRefsAtoB[0]);     // 32-byte copy from r24+0x50
            Vec4 lvFoldMax = VolRefBBoxMax(lapRefsAtoB[0]);     //   -> var_120/var_110
            for (u32 luRef = 1; luRef < luNumAtoB; ++luRef)     // addi r8, r29, -1 countdown
            {
                lvFoldMin = Min4(lvFoldMin, VolRefBBoxMin(lapRefsAtoB[luRef])); // vminfp
                lvFoldMax = Max4(lvFoldMax, VolRefBBoxMax(lapRefsAtoB[luRef])); // vmaxfp
            }

            // --- prime the second (smaller-side) sub-query -----------------
            VolumeBBoxQuery* lpQueryBtoA = m_bBoxQueryBtoA;     // lwz r11, 0x44(r31)
            lpQueryBtoA->m_numInputs            = 1;            // stw r26, 8(r11)
            lpQueryBtoA->m_inputVols            = &lpVol2;      // stw r8, 0(r11)
            lpQueryBtoA->m_inputMats            = &lpMtx2;      // stw r7, 4(r11) (unconditional)
            lpQueryBtoA->m_currInput            = 0;            // stw r30, 0xC(r11)
            lpQueryBtoA->m_stackNext            = 0;            // stw r30, 0xC0(r11)
            lpQueryBtoA->m_primNext             = 0;            // stw r30, 0xCC(r11)
            lpQueryBtoA->m_currVRef.muVolumePtr = 0;            // stw r30, 0x40(r11)
            lpQueryBtoA->m_aggIndex             = 0;            // stw r30, 0xE0(r11)
            lpQueryBtoA->m_curSpatialMapQuery   = 0;            // stw r30, 0xE8(r11)
            lpQueryBtoA->m_instVolCount         = 0;            // stw r30, 0xD8(r11)
            lpQueryBtoA->m_tag                  = 0;            // stw r30, 0xEC(r11)
            // fattened fold box (vsubfp128 v0 / vaddfp128 v13; also written
            // back to the fold slots before the 32-byte copy into m_aabb)
            lvFoldMin = Sub(lvFoldMin, lvPad);
            lvFoldMax = Add(lvFoldMax, lvPad);
            BoxMin(lpQueryBtoA->m_aabb) = lvFoldMin;            // 32-byte ld/std x4
            BoxMax(lpQueryBtoA->m_aabb) = lvFoldMax;
            lpQueryBtoA->m_numTagBits           = 0;            // stb r30, 0xF0(r11)

            const u32 luNumBtoA =
                static_cast<u32>(VolumeBBoxQuery::GetOverlaps(lpQueryBtoA)); // bl; cmplwi r3
            if (luNumBtoA != 0)                                 // beq -> next input
            {
                VolRef* lpRef2 = lapRefsBtoA;                   // r7 (0x80 stride)
                for (u32 lu2 = 0; lu2 < luNumBtoA; ++lu2, ++lpRef2) // cmplw r5, r3
                {
                    if (luBudget < 0x10)                        // cmplwi cr6, r27, 0x10
                        break;                                  // blt -> end of this input

                    // Stage the group header (refunded below if it stays empty).
                    luBudget -= 12;                             // addi r27, r27, -0xC
                    lpStage->vRefsNCount    = 0;                // stw r30, 4(r28)
                    lpStage->volumesSwapped = liSwapped;        // stw r25, 8(r28)
                    lpStage->vRef1          = lpRef2;           // stw r7, 0(r28)

                    // Fattened second-side result AABB (v13/v12; the console
                    // also writes the fattened copy back to its stack image).
                    const Vec4 lvFatMin2 = Sub(VolRefBBoxMin(*lpRef2), lvPad); // vsubfp128
                    const Vec4 lvFatMax2 = Add(VolRefBBoxMax(*lpRef2), lvPad); // vaddfp128

                    if (luNumAtoB != 0)                         // cmplwi cr6, r29, 0 (kept)
                    {
                        VolRef* lpRef1 = lapRefsAtoB;           // mr r10, r24
                        for (u32 lu1 = 0; lu1 < luNumAtoB; ++lu1, ++lpRef1)
                        {
                            if (luBudget < 4)                   // cmplwi cr6, r27, 4
                                break;                          // blt -> finalise group

                            // Optional cull-table rejection:
                            // bit(muHeight * groupID(vol2) + groupID(vol1)).
                            if (m_cullTable != 0)               // lwz r9, 0x10(r31)
                            {
                                const u32 luBit =
                                    m_cullTable->muHeight       // lwz r4, 4(r9)
                                        * VolumeGroupID(VolRefVolume(*lpRef2))  // [B vol +0x54]
                                    + VolumeGroupID(VolRefVolume(*lpRef1));     // [A vol +0x54]
                                if ((m_cullTable->maBits[luBit >> 5]            // +3-word header
                                     & (1u << (luBit & 31))) != 0)              // slw/and.
                                    continue;                   // bne loc_82BB3F74
                            }

                            // Fattened-2 vs 1 AABB overlap on x/y/z: two
                            // vcmpgtfp separation masks or'd (vor), the W lane
                            // dropped (vsldoi v0,v11,12), accepted iff every
                            // remaining lane is clear (vcmpeqfp. vs zero,
                            // CR6[all-eq] via mfocrf/extrwi).
                            const Vec4& lrMin1 = VolRefBBoxMin(*lpRef1);  // [r11] (r11=r10+0x50)
                            const Vec4& lrMax1 = VolRefBBoxMax(*lpRef1);  // [r11+0x10]
                            const bool lbSeparated =
                                   (lvFatMin2.x > lrMax1.x) || (lrMin1.x > lvFatMax2.x)
                                || (lvFatMin2.y > lrMax1.y) || (lrMin1.y > lvFatMax2.y)
                                || (lvFatMin2.z > lrMax1.z) || (lrMin1.z > lvFatMax2.z);
                            if (!lbSeparated)                   // beq loc_82BB3F74 skips
                            {
                                luBudget -= 4;                  // addi r27, r27, -4
                                lpStage->vRefsN[lpStage->vRefsNCount] = lpRef1; // stwx r10
                                ++lpStage->vRefsNCount;         // stw +1, 4(r28)
                                ++m_volRefPairCount;            // stw +1, 0x1C(r31)
                            }
                        }
                    }

                    if (lpStage->vRefsNCount != 0)              // lwz r10, 4(r28)
                    {
                        lpStage = lpStage->NextGroup();         // r28 += (count+3)*4 (console words)
                        ++m_volRef1xNCount;                     // stw +1, 0x28(r31)
                    }
                    else
                    {
                        luBudget += 12;                         // addi r27, r27, 0xC (refund)
                    }
                }
            }
        }

        ++m_currInput;                                          // stw +1, 0xC(r31)
    }

    return static_cast<int>(m_volRefPairCount);                 // lwz r3, 0x1C(r31)
}

// ===========================================================================
// rw::collision::VolumeVolumeQuery::GetPrimitiveIntersections  @ 0x82BB3FF0
// Called by: CgsSceneManager::LooseOctree::VolumeTestRecursive,
//            CgsSceneManager::OverlapCullingModule::DoPairQuery,
//            CgsSceneManager::FineIntersectionTestModule::ComputeVolumeTestDeepest,
//            CgsSceneManager::FineIntersectionTestModule::ComputeVolumeTestFine.
//
// Stage the broad-phase overlap groups, then batch-intersect them; returns
// the number of narrow-phase intersections written to m_intersectionBuffer
// (the bbox pass's own pair count is discarded, exactly as the asm ignores
// the r3 of the first bl). No VMX in this body -- it is a straight-line
// two-call wrapper (the lfs f1, 0x14(r31) is the padding argument); the
// wave-1 declaration-only FLAG is lifted.
// ===========================================================================
namespace
{
    // [DIAG] NOT IN THE X360 BINARY (2026-09-25, crash parity FX-FOLLOWUPS). Opt-in BRN_VVQ_DIAG=1: one
    // `[vvq]` line per GetPrimitiveIntersections call (capped at 96) plus one `[vvq] max` line whenever a
    // per-call high-water mark rises (capped at 32). It proves which callers dispatch the query and measures
    // how close a live batch comes to the staging / result capacity: the staged pairs and 1xN groups
    // against the pair budget (console units 12 per group + 4 per pair out of 8 * m_volRefPairBufferSize;
    // host bytes 16 per group + 8 per pair out of the host region, 16 * m_volRefPairBufferSize), and the
    // intersections against m_intersectionBufferMaxSize.
    // Reads only; with the variable unset the cost is one static bool test.
    bool VolumeVolumeQueryDiagEnabled()
    {
        static const bool sbEnabled = []() {
            const char* lpcValue = std::getenv("BRN_VVQ_DIAG");
            return lpcValue != 0 && lpcValue[0] == '1';
        }();
        return sbEnabled;
    }

    void NoteVolumeVolumeQuery(const VolumeVolumeQuery& lrQuery, int liIntersections)
    {
        static u32 suLines = 0, suMaxLines = 0, suCalls = 0;
        static u32 suMaxPairs = 0, suMaxGroups = 0;
        static int siMaxIntersections = 0;
        if (CgsDev::Log::gpDebugPrint == 0)
        {
            return;
        }
        ++suCalls;
        const u32 luPairs  = lrQuery.m_volRefPairCount;
        const u32 luGroups = lrQuery.m_volRef1xNCount;
        char lacLine[320];
        if (suLines < 96)
        {
            ++suLines;
            std::snprintf(lacLine, sizeof(lacLine),
                          "[vvq] call %u inputs %u queryType %u input0Type %u staged pairs %u groups %u (console bytes %u "
                          "of budget %u, host bytes %u of region %u) intersections %d of %d\n",
                          suCalls, lrQuery.m_numInputs,
                          lrQuery.m_queryVol ? lrQuery.m_queryVol->muVTableSlot : 0xFFFFFFFFu,
                          (lrQuery.m_numInputs && lrQuery.m_inputVols && lrQuery.m_inputVols[0])
                              ? lrQuery.m_inputVols[0]->muVTableSlot : 0xFFFFFFFFu,
                          luPairs, luGroups, 12u * luGroups + 4u * luPairs, 8u * lrQuery.m_volRefPairBufferSize,
                          16u * luGroups + 8u * luPairs, VolumeVolumeQueryStagingSize(lrQuery.m_volRefPairBufferSize),
                          liIntersections, lrQuery.m_intersectionBufferMaxSize);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
        if ((luPairs > suMaxPairs || luGroups > suMaxGroups || liIntersections > siMaxIntersections) && suMaxLines < 32)
        {
            ++suMaxLines;
            if (luPairs > suMaxPairs) suMaxPairs = luPairs;
            if (luGroups > suMaxGroups) suMaxGroups = luGroups;
            if (liIntersections > siMaxIntersections) siMaxIntersections = liIntersections;
            std::snprintf(lacLine, sizeof(lacLine),
                          "[vvq] max after call %u: pairs %u groups %u intersections %d\n",
                          suCalls, suMaxPairs, suMaxGroups, siMaxIntersections);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
    }
}

// ===========================================================================
// rw::collision::VolumeLineQuery -- the two construction entry points (2026-09-25, crash parity FX-FOLLOWUPS).
// Their one caller is FineIntersectionTestModule::Construct @0x828B0BF0 (0x828B0CC8 / 0x828B0D28).
// ===========================================================================

static_assert(VolumeLineQueryResourceSize(100u, 100u) == KU_VOLUME_LINE_QUERY_HOST_SIZE_R100,
              "KU_VOLUME_LINE_QUERY_HOST_SIZE_R100 is GetResourceDescriptor's host total for 100 volumes / 100 results");

// @ 0x82BB3838 -- the five descriptor entries {0, 1} (the `addic. r8, r8, -1 ; bge` loop), then entry 0 =
// {0x1B0 * results + 0x80 * volumes + 0x110 + 0x2880, 16} (the `std` of the staged pair).
// NOT X360: host VolumeLineQuery width -- the header term is KU_VOLUME_LINE_QUERY_HEADER_SIZE (VolumeQueryHostLayout.hpp).
void* VolumeLineQuery::GetResourceDescriptor(void* lpOut, int liVolumes, int liResults)
{
    u32* lpau = static_cast<u32*>(lpOut);
    for (int liEntry = 0; liEntry < 5; ++liEntry)
    {
        lpau[2 * liEntry + 0] = 0;   // m_size
        lpau[2 * liEntry + 1] = 1;   // m_alignment
    }
    lpau[0] = VolumeLineQueryResourceSize(static_cast<u32>(liVolumes), static_cast<u32>(liResults));
    lpau[1] = 16;
    return lpOut;
}

// @ 0x82BB3888 -- `lwz r11, 0(r3)`: null block -> null. Otherwise the four capacities and the five carves,
// in the console's store order: m_stackMax (+0xD4) = volumes, m_primBufferSize (+0xE0) = results,
// m_resBufferSize (+0x1C) = results, m_instVolMax (+0xEC) = results; m_stackVRefBuffer (+0x44) = base + header,
// m_primVRefBuffer (+0xD8) = that + 0x80 * volumes, m_instVolPool (+0xE4) = that + 0x80 * results,
// m_resBuffer (+0x10) = that + 0x60 * results, m_spatialMapQueryMem (+0xF4) = that + 0xD0 * results. Nothing
// else is written (the query is primed per test by InitQuery).
// NOT X360: host VolumeLineQuery width -- the first carve is at KU_VOLUME_LINE_QUERY_HEADER_SIZE, console 0x110.
void* VolumeLineQuery::Initialize(void** lppBuffer, int liVolumes, int liResults)
{
    VolumeLineQuery* lpQuery = static_cast<VolumeLineQuery*>(lppBuffer[0]);
    if (lpQuery == nullptr)
    {
        return nullptr;
    }

    lpQuery->m_stackMax       = static_cast<u32>(liVolumes);
    lpQuery->m_primBufferSize = static_cast<u32>(liResults);
    lpQuery->m_resBufferSize  = static_cast<u32>(liResults);
    lpQuery->m_instVolMax     = static_cast<u32>(liResults);

    u8* const lpBase = reinterpret_cast<u8*>(lpQuery);
    lpQuery->m_stackVRefBuffer    = reinterpret_cast<VolRef*>(lpBase + KU_VOLUME_LINE_QUERY_HEADER_SIZE);
    lpQuery->m_primVRefBuffer     = lpQuery->m_stackVRefBuffer + liVolumes;
    lpQuery->m_instVolPool        = reinterpret_cast<Volume*>(lpQuery->m_primVRefBuffer + liResults);
    lpQuery->m_resBuffer          = reinterpret_cast<VolumeLineSegIntersectResult*>(lpQuery->m_instVolPool + liResults);
    lpQuery->m_spatialMapQueryMem = lpQuery->m_resBuffer + liResults;
    return lpQuery;
}

// ---- BEGIN VolumeLineQuery line walk (run_fxfollowups_line_test_nearest compiles this region) ----
// ===========================================================================
// rw::collision::VolumeLineQuery -- THE LINE WALK (2026-09-25, crash parity FX-FOLLOWUPS stage a).
//   AddPrimitiveRef      @ 0x82BB3230
//   AddVolumeRef         @ 0x82BB3300
//   GetIntersections     @ 0x82BB3470   (an export hole: `tools/re/ppcdis.py 0x82BB3470 235`)
//   GetAllIntersections  @ 0x82BB3820
// Their caller is FineIntersectionTestModule::ComputeLineTestNearest @0x828C8CC8 (InitQuery, then
// GetAllIntersections until Finished). The query stages the primitives to test in m_primVRefBuffer, keeps
// the aggregates still to open on the stack m_stackVRefBuffer, and writes one 0xD0-byte
// VolumeLineSegIntersectResult per hit into m_resBuffer.
// ===========================================================================
namespace
{
    // The descriptor type id of an aggregate volume: `cmpwi cr6, r10, 6` (0x82BB330C) and `cmpwi cr6, r11, 6`
    // (0x82BB35EC) against the descriptor's leading word.
    const u32 KU_LINE_WALK_VOLUMETYPE_AGGREGATE = 6u;

    // One lvx128 / stvx128 pair: a 16-byte transform row copied into a VolRef's inline rows.
    void CopyLineWalkRow(VolRef::Vec4& arDst, const VolRef::Vec4& arSrc)
    {
        arDst.x = arSrc.x;
        arDst.y = arSrc.y;
        arDst.z = arSrc.z;
        arDst.w = arSrc.w;
    }

    // The four rows at a transform pointer (a Matrix44Affine is four 16-byte rows).
    const VolRef::Vec4* LineWalkRows(const void* lpTransform)
    {
        return static_cast<const VolRef::Vec4*>(lpTransform);
    }

    // AddPrimitiveRef's and AddVolumeRef's shared fill -- the two bodies are the same code over two buffers
    // (0x82BB324C..0x82BB32F8 and 0x82BB3330..0x82BB33DC): the volume word (stwx r4); with a transform, its
    // four rows copied into the entry (+0x10..+0x4F) and the transform word aimed at that copy (entry + 0x10),
    // without one a zero transform word; then the tag word (+0x70) and the tag-bit byte (+0x74).
    void FillLineWalkVolRef(VolRef& arEntry, const Volume* lpVolume, const math::vpu::Matrix44Affine* lpTransform,
                            u32 luTag, u8 luNumTagBits)
    {
        arEntry.muVolumePtr = reinterpret_cast<uintptr_t>(lpVolume);
        if (lpTransform != 0)
        {
            const VolRef::Vec4* lpRows = LineWalkRows(lpTransform);
            CopyLineWalkRow(arEntry.mRow0, lpRows[0]);
            CopyLineWalkRow(arEntry.mRow1, lpRows[1]);
            CopyLineWalkRow(arEntry.mRow2, lpRows[2]);
            CopyLineWalkRow(arEntry.mRow3, lpRows[3]);
            arEntry.muTransformPtr = reinterpret_cast<uintptr_t>(&arEntry.mRow0);
        }
        else
        {
            arEntry.muTransformPtr = 0;
        }
        arEntry.muTag        = luTag;
        arEntry.muNumTagBits = luNumTagBits;
    }

    // [PC TRAP, NOT X360] a walk step this host has no body for: announced in the game log the first time
    // (unconditionally -- no knob), then CGS_ASSERT(false) every time. Never a silent "no hit".
    void LineWalkTrap(bool& arbAnnounced, const char* lpcAnnouncement, const char* lpcAssert)
    {
        if (!arbAnnounced)
        {
            arbAnnounced = true;
            CgsDev::Log::WriteToLog(lpcAnnouncement);
        }
        CGS_ASSERT(false, lpcAssert);
    }
}

// @ 0x82BB3230 -- stage one primitive. `cmplw m_primNext (+0xDC), m_primBufferSize (+0xE0) ; blt` -- a full
// buffer returns 0 and writes nothing; otherwise the entry m_primVRefBuffer[m_primNext] (+0xD8, `slwi 7`) is
// filled and m_primNext incremented, returning 1.
s32 VolumeLineQuery::AddPrimitiveRef(const Volume* lpVolume, const math::vpu::Matrix44Affine* lpTransform, u32 luTag,
                                     u8 luNumTagBits)
{
    if (!(m_primNext < m_primBufferSize))
    {
        return 0;
    }
    FillLineWalkVolRef(m_primVRefBuffer[m_primNext], lpVolume, lpTransform, luTag, luNumTagBits);
    ++m_primNext;
    return 1;
}

// @ 0x82BB3300 -- `lwz r10, 0x40(r4) ; lwz r10, 0(r10) ; cmpwi cr6, r10, 6`: anything but an aggregate is the
// tail call `b AddPrimitiveRef`. An aggregate is pushed on the traversal stack the same way:
// `cmplw m_stackNext (+0xD0), m_stackMax (+0xD4) ; blt`, full -> 0; else m_stackVRefBuffer[m_stackNext] (+0x44)
// filled, m_stackNext incremented, 1.
// (The host reads the descriptor as gVolumeVTable[+0x40 enum], CollisionVolume.hpp's GetVolumeDescriptor.)
s32 VolumeLineQuery::AddVolumeRef(const Volume* lpVolume, const math::vpu::Matrix44Affine* lpTransform, u32 luTag,
                                  u8 luNumTagBits)
{
    if (GetVolumeDescriptor(lpVolume)->muTypeID != KU_LINE_WALK_VOLUMETYPE_AGGREGATE)
    {
        return AddPrimitiveRef(lpVolume, lpTransform, luTag, luNumTagBits);
    }
    if (!(m_stackNext < m_stackMax))
    {
        return 0;
    }
    FillLineWalkVolRef(m_stackVRefBuffer[m_stackNext], lpVolume, lpTransform, luTag, luNumTagBits);
    ++m_stackNext;
    return 1;
}

// @ 0x82BB3470 (an export hole; 235 words to the `b __restgprlr_23` at 0x82BB3818). r31 = this, r23 = 0.
//   0x82BB3490  m_resCount (+0x14) = 0, m_instVolCount (+0xE8) = 0, m_tag (+0x104) = 0, m_numTagBits (+0x108) = 0.
//   0x82BB34A0  THE OUTER LOOP. Nothing left -- m_currInput >= m_numInputs (cmplw), no current volume (+0x50) and
//               m_primNext + m_stackNext == 0 (`add.`) -- or no room (m_resCount >= m_resMax, cmplw) -> return
//               m_resCount (0x82BB3810).
//   0x82BB34DC  r29 (full) = 0. THE FILL LOOP (0x82BB34E0), left for the test loop when nothing is left to
//               stage (inputs consumed, no current volume, empty stack) or r29 is set:
//     0x82BB3510  with no current volume, an empty stack and an input left: the input m_inputVols[m_currInput]
//                 -- a disabled one (Volume::m_flags +0x5C bit 0 clear, `clrlwi. 31`) only advances
//                 m_currInput; otherwise AddVolumeRef(input, m_inputMats ? m_inputMats[i] : 0, 0, 0), its result
//                 NOT read (the `cmplwi r3, 0` at 0x82BB3584 is overwritten before any branch), and m_currInput++.
//     0x82BB3590  no current volume: an empty stack goes round again; else pop it, m_currVRef =
//                 m_stackVRefBuffer[--m_stackNext] (VolRef::operator= @0x82BB33E8).
//     0x82BB35D0  m_tag / m_numTagBits = the current reference's tag (+0xC0) / tag-bit byte (+0xC4).
//                 Not an aggregate: AddPrimitiveRef(volume, its transform word +0x54, tag, bits) -- 1 spends the
//                 reference (+0x50 = 0, 0x82BB36F0), 0 sets r29 (0x82BB3608).
//     0x82BB3610  an aggregate: its LineIntersectionQuery -- see the trap below.
//   0x82BB3800  THE TEST LOOP, while m_primNext > 0 (cmplwi ; bgt), last staged first:
//     0x82BB36F8  no room (m_resCount >= m_resMax) -> the outer loop. Else --m_primNext; the result record is
//                 m_resBuffer[m_resCount] (`mulli 0xD0`); r28 / r29 = the entry's volume / transform words.
//     0x82BB3738  a disabled volume is passed over (m_primNext already decremented).
//     0x82BB3744  the descriptor's lineSegIntersect (+0x18): (r3 = volume, r4 = &m_pt1 +0x20, r5 = &m_pt2 +0x30,
//                 r6 = the transform word, r7 = the result, f1 = m_fatness +0x40). 0 -> passed over.
//     0x82BB3770  a hit: with m_resultsSet (+0x100) != 0, m_endClipVal (+0xFC) = lineParam (+0x40) when lineParam
//                 is below it (`fcmpu ; bge` skips -- a NaN never clips); then result +0x50 (vRef volume) = r28,
//                 +0x00 (v) = m_inputVols[m_currInput - 1]; with a transform word its rows are copied to +0x60..
//                 and +0x54 aimed at them, else +0x54 = 0; +0xC0 (vRef tag) = the entry's tag (+0x70) -- the
//                 tag-bit byte (+0xC4) is NOT written; m_resCount++.
u32 VolumeLineQuery::GetIntersections()
{
    m_resCount     = 0;
    m_instVolCount = 0;
    m_tag          = 0;
    m_numTagBits   = 0;

    for (;;)
    {
        if (m_currInput >= m_numInputs && m_currVRef.muVolumePtr == 0 && (m_primNext + m_stackNext) == 0)
        {
            break;
        }
        if (m_resCount >= m_resMax)
        {
            break;
        }

        // ---- the fill loop (0x82BB34DC..0x82BB36F4) ----------------------------------------------------------
        s32 liFull = 0;   // r29
        for (;;)
        {
            const u32 luCurrInput = m_currInput;   // r10
            const u32 luNumInputs = m_numInputs;   // r11
            if (luCurrInput >= luNumInputs && m_currVRef.muVolumePtr == 0 && m_stackNext == 0)
            {
                break;
            }
            if (liFull != 0)
            {
                break;
            }

            if (m_currVRef.muVolumePtr == 0 && m_stackNext == 0 && luCurrInput < luNumInputs)
            {
                const Volume* lpInput = m_inputVols[luCurrInput];
                if ((lpInput->muFlags & KU_VOLUMEFLAG_ISENABLED) == 0)
                {
                    m_currInput = luCurrInput + 1;
                    continue;
                }
                const math::vpu::Matrix44Affine* lpInputMat = (m_inputMats != 0) ? m_inputMats[luCurrInput] : 0;
                AddVolumeRef(lpInput, lpInputMat, 0, 0);   // the result is not read on the console either
                ++m_currInput;
            }

            if (m_currVRef.muVolumePtr == 0)
            {
                if (m_stackNext == 0)
                {
                    continue;
                }
                --m_stackNext;
                m_currVRef = m_stackVRefBuffer[m_stackNext];
            }

            const Volume* lpVolume = reinterpret_cast<const Volume*>(m_currVRef.muVolumePtr);
            m_tag        = m_currVRef.muTag;
            m_numTagBits = m_currVRef.muNumTagBits;
            if (GetVolumeDescriptor(lpVolume)->muTypeID != KU_LINE_WALK_VOLUMETYPE_AGGREGATE)
            {
                if (AddPrimitiveRef(lpVolume,
                                    reinterpret_cast<const math::vpu::Matrix44Affine*>(m_currVRef.muTransformPtr),
                                    m_currVRef.muTag, m_currVRef.muNumTagBits) == 0)
                {
                    liFull = 1;
                    continue;
                }
                m_currVRef.muVolumePtr = 0;
                continue;
            }

            // 0x82BB3610 -- AN AGGREGATE. The console composes the volume's own transform with the reference's
            // (the vmulfp128 / vmaddfp block 0x82BB361C..0x82BB36BC into sp+0x50; the volume itself when the
            // reference has no transform), then asks the aggregate (Volume +0x44) through ITS vtable (+0x20,
            // slot +0x14 m_LineIntersectionQuery, DWARF aggregate.h:308) to stage its children:
            // (r3 = aggregate, r4 = this, r5 = the transform). 0 means "full" (r29 = 1; the next call resumes
            // it, its state in m_curSpatialMapQuery / m_aggIndex); otherwise m_curSpatialMapQuery (+0xF8) = 0,
            // m_aggIndex (+0xF0) = 0 and the reference is spent (+0x50 = 0).
            // [PC TRAP, NOT X360] no aggregate on this host has a LineIntersectionQuery body --
            // ClusteredMesh::LineIntersectionQueryThis @0x82BB2388 and TriangleKDTreeProcedural::
            // LineIntersectionQueryThis @0x82BB0700 are not reconstructed, and no aggregate vtable is built
            // (ClusteredMesh::Fixup is a __debugbreak link stub, CgsClusteredMeshResourceType.cpp). Announced and
            // asserted; the aggregate is then spent as on a completed query, so the walk ends -- it is never
            // re-dispatched (a "full" answer would loop) and never a silent miss.
            static bool sbAggregateAnnounced = false;
            LineWalkTrap(sbAggregateAnnounced,
                         "[vlq] TRAP (PC, NOT X360): rw::collision::VolumeLineQuery::GetIntersections @0x82BB3470 "
                         "met an AGGREGATE volume. Its LineIntersectionQuery (ClusteredMesh @0x82BB2388 / "
                         "TriangleKDTreeProcedural @0x82BB0700) has no host body: the aggregate is NOT tested.\n",
                         "VolumeLineQuery::GetIntersections @0x82BB3470: an aggregate's LineIntersectionQuery "
                         "has no host body");
            m_curSpatialMapQuery   = 0;
            m_aggIndex             = 0;
            m_currVRef.muVolumePtr = 0;
        }

        // ---- the test loop (0x82BB36F8..0x82BB380C) ----------------------------------------------------------
        while (m_primNext > 0)
        {
            if (m_resCount >= m_resMax)
            {
                break;
            }
            --m_primNext;
            VolumeLineSegIntersectResult& lrResult = m_resBuffer[m_resCount];
            const VolRef&       lrEntry     = m_primVRefBuffer[m_primNext];
            const Volume*       lpPrimitive = reinterpret_cast<const Volume*>(lrEntry.muVolumePtr);   // r28
            const VolRef::Vec4* lpTransform = LineWalkRows(reinterpret_cast<const void*>(lrEntry.muTransformPtr));
            if ((lpPrimitive->muFlags & KU_VOLUMEFLAG_ISENABLED) == 0)
            {
                continue;
            }

            const VolumeLineSegIntersectFn lpfnLineSegIntersect =
                GetVolumeDescriptor(lpPrimitive)->mpfnLineSegIntersect;
            if (lpfnLineSegIntersect == 0)
            {
                // [PC TRAP, NOT X360] the descriptor's lineSegIntersect slot has no host body: SPHERE @0x82BA82C8,
                // CAPSULE @0x82BAFCF8, BOX @0x82BA9478 and CYLINDER @0x82BAF688 are parked in VolumeVTables.cpp.
                // (AGGREGATE's slot is genuinely 0 in the image; AddVolumeRef never stages an aggregate.)
                // Announced once per type and asserted; the primitive is then passed over, NOT tested.
                static bool sabSlotAnnounced[E_VOLUMETYPE_NUMINTERNALTYPES] = {};
                const u32 luType = lpPrimitive->muVTableSlot < static_cast<u32>(E_VOLUMETYPE_NUMINTERNALTYPES)
                                 ? lpPrimitive->muVTableSlot : 0u;
                char lacAnnouncement[320];
                std::snprintf(lacAnnouncement, sizeof(lacAnnouncement),
                              "[vlq] TRAP (PC, NOT X360): rw::collision::VolumeLineQuery::GetIntersections "
                              "@0x82BB3470 staged a volume of type %u whose descriptor's lineSegIntersect slot has "
                              "no host body (VolumeVTables.cpp: SPHERE 0x82BA82C8, CAPSULE 0x82BAFCF8, BOX "
                              "0x82BA9478, CYLINDER 0x82BAF688): the volume is NOT tested.\n", luType);
                LineWalkTrap(sabSlotAnnounced[luType], lacAnnouncement,
                             "VolumeLineQuery::GetIntersections @0x82BB3470: a staged primitive's lineSegIntersect "
                             "slot has no host body");
                continue;
            }
            if (lpfnLineSegIntersect(lpPrimitive, reinterpret_cast<const Vec4&>(m_pt1),
                                     reinterpret_cast<const Vec4&>(m_pt2),
                                     reinterpret_cast<const Vec4*>(lpTransform), lrResult, m_fatness) == 0)
            {
                continue;
            }

            if (m_resultsSet != ALLLINEINTERSECTIONS && lrResult.lineParam < m_endClipVal)
            {
                m_endClipVal = lrResult.lineParam;
            }
            lrResult.vRef.muVolumePtr = lrEntry.muVolumePtr;
            lrResult.v                = reinterpret_cast<uintptr_t>(m_inputVols[m_currInput - 1]);
            if (lpTransform != 0)
            {
                CopyLineWalkRow(lrResult.vRef.mRow0, lpTransform[0]);
                CopyLineWalkRow(lrResult.vRef.mRow1, lpTransform[1]);
                CopyLineWalkRow(lrResult.vRef.mRow2, lpTransform[2]);
                CopyLineWalkRow(lrResult.vRef.mRow3, lpTransform[3]);
                lrResult.vRef.muTransformPtr = reinterpret_cast<uintptr_t>(&lrResult.vRef.mRow0);
            }
            else
            {
                lrResult.vRef.muTransformPtr = 0;
            }
            lrResult.vRef.muTag = m_primVRefBuffer[m_primNext].muTag;
            ++m_resCount;
        }
    }
    return m_resCount;
}

// @ 0x82BB3820 -- `lwz r11, 0x1C(r3) ; stw 0, 0x100(r3) ; stw r11, 0x18(r3) ; b GetIntersections`.
u32 VolumeLineQuery::GetAllIntersections()
{
    m_resultsSet = ALLLINEINTERSECTIONS;
    m_resMax     = m_resBufferSize;
    return GetIntersections();
}
// ---- END VolumeLineQuery line walk ----

int VolumeVolumeQuery::GetPrimitiveIntersections()
{
    GetPrimitiveBBoxOverlaps();                                 // bl 0x82BB3AB0 (result unused)

    const int liIntersections = PrimitiveBatchIntersect(
        m_intersectionBuffer,                                   // r3 <- lwz 0x30(r31)
        m_intersectionBufferMaxSize,                            // r4 <- lwz 0x34(r31)
        m_instancingSPR,                                        // r5 <- lwz 0x2C(r31)
        m_volRefPairBuffer,                                     // r6 <- lwz 0x18(r31)
        static_cast<s32>(m_volRef1xNCount),                     // r7 <- lwz 0x28(r31)
        m_padding);                                             // f1 <- lfs 0x14(r31)

    if (VolumeVolumeQueryDiagEnabled())                          // [DIAG] NOT IN THE X360 BINARY
    {
        NoteVolumeVolumeQuery(*this, liIntersections);
    }
    return liIntersections;
}

} // namespace collision
} // namespace rw
