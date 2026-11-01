#include "vendor/renderware/collision/VolumeQuery.hpp"

#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/Aggregate.hpp"
#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"        // VolRef1xN / PPIR / PrimitiveBatchIntersect
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"   // the embedded sub-queries
#include <cstring>  // memset
#include <cstdio>   // snprintf ([vvq] / [vlq] DIAG, the [vlq] traps)
#include <cstdlib>  // getenv   ([vvq] / [vlq] DIAG only)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint ([vvq] DIAG only); WriteToLog (the [vlq] traps)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT (the [vlq] traps, NOT X360)

namespace rw
{
namespace collision
{
CGS_STATIC_ASSERT((sizeof(GPInstance) == 0xc0), "Native GPInstance stride");
CGS_STATIC_ASSERT((sizeof(VolumeVolumeQuery) == 0x48), "Native VolumeVolumeQuery size");
CGS_STATIC_ASSERT((sizeof(VolumeLineQuery) == 0x110), "Native VolumeLineQuery size");
CGS_STATIC_ASSERT((((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1)) == 0x50), "Native aligned VolumeVolumeQuery size");

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
VolumeVolumeQuery* VolumeVolumeQuery::Construct(int liVolumes, int liResults)
{
    m_volRefPairBufferSize = static_cast<u32>(liResults);         // +0x20

    // `addi r11, r31, 0x50` on the console: the sub-queries start right behind the object.
    // NOT X360: host layout -- see ((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1)) above.
    u8* lpSubQueryRegion = reinterpret_cast<u8*>(this) + ((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1));
    m_bBoxQueryAtoB = reinterpret_cast<VolumeBBoxQuery*>(lpSubQueryRegion); // +0x40

    // The descriptor's first word is the per-side VolumeBBoxQuery backing size.
    u32 laDescriptor[12];
    VolumeBBoxQuery::GetResourceDescriptor(laDescriptor, liVolumes, liResults);
    u32 luDescSize = laDescriptor[0];

    m_cullTable                 = 0;                        // +0x10
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

    const u32 luStagingSize = (2u * sizeof(void*) * static_cast<u32>(liResults));
    m_intersectionBuffer = reinterpret_cast<PrimitivePairIntersectResult*>(
        lpReportBase + luStagingSize);                                      // +0x30
    m_instancingSPR = reinterpret_cast<GPInstance*>(
        lpReportBase + (sizeof(PrimitivePairIntersectResult) * static_cast<u32>(liResults))
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
        lpQuery = 0;
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
// and 2*40 is the 0x50 query header (see ((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1))).
// ===========================================================================
void* VolumeVolumeQuery::GetResourceDescriptor(void* lpOut, int liVolumes, int liResults)
{
    // Side sub-query descriptor (first word = its backing size).
    u32 laBBoxDesc[12];
    VolumeBBoxQuery::GetResourceDescriptor(laBBoxDesc, liVolumes, liResults);
    u32 luBBoxDescSize = laBBoxDesc[0];

    // Total backing size for the object, the two sub-queries and the report buffer. The console
    // folds its 0x50 header into `2 * (bboxDesc + 0x28)` (0x82BB3A4C / 0x82BB3A58) and its report
    // buffer into `2072 * a3 + 192` == 8*a3 staging + 1872*a3 results + 192*(a3 + 1) instancing.
    // NOT X360: host layout -- the header is ((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1)), the size Construct
    // carves behind; the staging and instancing regions are the host GPInstance / VolRef widths
    // (VolumeQueryHostLayout.hpp). The results stride is native.
    const u32 luResults   = static_cast<u32>(liResults);
    u32       luTotalSize = ((sizeof(VolumeVolumeQuery) + __alignof(VolumeBBoxQuery) - 1) & ~(__alignof(VolumeBBoxQuery) - 1))
                          + 2u * luBBoxDescSize
                          + (2u * sizeof(void*) * luResults)
                          + sizeof(PrimitivePairIntersectResult) * luResults
                          + (sizeof(GPInstance) * (luResults + 1u));

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
    // Volume::groupID -- canonical rwccore.h:1612, console byte +0x54
    // (transform 0x40 + vTable word + 12-byte type union + radius). Read by
    // image offset until the full Volume layout lands (the committed
    // CollisionVolume.hpp image is a flagged placeholder), matching the
    // PrimitiveIntersect.cpp byte-offset precedent.
    u32 VolumeGroupID(const Volume* lpVolume) { return lpVolume->muGroupID; }

    // VolRef +0x00 volume word -> Volume* (same widening helper as the
    // committed PrimitiveIntersect.cpp).
    const Volume* VolRefVolume(const VolRef& lrRef) { return lrRef.volume; }

    // -----------------------------------------------------------------------
    const math::vpu::Vector3& VolRefBBoxMin(const VolRef& ref) { return ref.bBox.mMin; }
    const math::vpu::Vector3& VolRefBBoxMax(const VolRef& ref) { return ref.bBox.mMax; }
    math::vpu::Vector3& BoxMin(AABBox& box) { return box.mMin; }
    math::vpu::Vector3& BoxMax(AABBox& box) { return box.mMax; }
    inline math::vpu::Vector3 VpuSub(const math::vpu::Vector3& a, const math::vpu::Vector3& b)
    { return math::vpu::Vector3(__vsubfp(a.mV, b.mV)); }
    inline math::vpu::Vector3 VpuAdd(const math::vpu::Vector3& a, const math::vpu::Vector3& b)
    { return math::vpu::Vector3(__vaddfp(a.mV, b.mV)); }
    inline math::vpu::Vector3 Min4(const math::vpu::Vector3& a, const math::vpu::Vector3& b)
    { return math::vpu::Vector3(__vminfp(a.mV, b.mV)); }
    inline math::vpu::Vector3 Max4(const math::vpu::Vector3& a, const math::vpu::Vector3& b)
    { return math::vpu::Vector3(__vmaxfp(a.mV, b.mV)); }

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
    math::vpu::Vector3 lvPad;
    lvPad.mV.x = m_padding;                                        // stfs f0, var_120
    lvPad.mV.y = m_padding;                                        // stfs f0, var_11C
    lvPad.mV.z = m_padding;                                        // stfs f0, var_118
    lvPad.mV.w = 0.0f;                                             // stw r30, var_114

    // --- query-side AABB via the volume vtable (slot 1) --------------------
    AABBox lQueryBBox;                                          // var_100/var_F0
    m_queryVol->GetBBox(m_queryMtx, 0, lQueryBBox);

    // --- staging byte budget over the pair buffer --------------------------
    u32 luBudget = m_volRefPairBufferSize << 3;                 // slwi r27, r11, 3

    math::vpu::Vector3& lrQueryMin = BoxMin(lQueryBBox);
    math::vpu::Vector3& lrQueryMax = BoxMax(lQueryBBox);

    // Box-volume product of the UNfattened query box (the lfs reads at
    // 0x82BB3B10..0x82BB3B4C precede the fattened stvx128 stores).
    f32 lfQueryVolume = (lrQueryMax.mV.z - lrQueryMin.mV.z)
                      * (lrQueryMax.mV.y - lrQueryMin.mV.y);          // fsubs x2 + fmuls
    lfQueryVolume *= (lrQueryMax.mV.x - lrQueryMin.mV.x);             // fmuls f31

    // Fatten the query box in place (vsubfp128 v0 / vaddfp128 v13).
    lrQueryMin = VpuSub(lrQueryMin, lvPad);
    lrQueryMax = VpuAdd(lrQueryMax, lvPad);

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
        lpVol2->GetBBox(lpMtx2, 0, lInputBBox);

        RwBool        liSwapped     = 0;                        // r25 = r30
        const AABBox* lpSmallerBBox = &lInputBBox;              // r29 = &var_E0

        math::vpu::Vector3& lrInputMin = BoxMin(lInputBBox);
        math::vpu::Vector3& lrInputMax = BoxMax(lInputBBox);

        // Box-volume product of the UNfattened input box (same load-before-
        // store scheduling as the query box).
        f32 lfInputVolume = (lrInputMax.mV.z - lrInputMin.mV.z)
                          * (lrInputMax.mV.y - lrInputMin.mV.y);      // fmuls f0, f13, f0
        lfInputVolume *= (lrInputMax.mV.x - lrInputMin.mV.x);         // fmuls f0, f0, f12

        // Fatten the input box in place.
        lrInputMin = VpuSub(lrInputMin, lvPad);                    // vsubfp128 v0
        lrInputMax = VpuAdd(lrInputMax, lvPad);                    // vaddfp128 v13

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
        lpQueryAtoB->m_currVRef.volume = 0;                // stw r30, 0x40(r11)
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
            math::vpu::Vector3 lvFoldMin = VolRefBBoxMin(lapRefsAtoB[0]);     // 32-byte copy from r24+0x50
            math::vpu::Vector3 lvFoldMax = VolRefBBoxMax(lapRefsAtoB[0]);     //   -> var_120/var_110
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
            lpQueryBtoA->m_currVRef.volume = 0;            // stw r30, 0x40(r11)
            lpQueryBtoA->m_aggIndex             = 0;            // stw r30, 0xE0(r11)
            lpQueryBtoA->m_curSpatialMapQuery   = 0;            // stw r30, 0xE8(r11)
            lpQueryBtoA->m_instVolCount         = 0;            // stw r30, 0xD8(r11)
            lpQueryBtoA->m_tag                  = 0;            // stw r30, 0xEC(r11)
            // fattened fold box (vsubfp128 v0 / vaddfp128 v13; also written
            // back to the fold slots before the 32-byte copy into m_aabb)
            lvFoldMin = VpuSub(lvFoldMin, lvPad);
            lvFoldMax = VpuAdd(lvFoldMax, lvPad);
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
                    const math::vpu::Vector3 lvFatMin2 = VpuSub(VolRefBBoxMin(*lpRef2), lvPad); // vsubfp128
                    const math::vpu::Vector3 lvFatMax2 = VpuAdd(VolRefBBoxMax(*lpRef2), lvPad); // vaddfp128

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
                            const math::vpu::Vector3& lrMin1 = VolRefBBoxMin(*lpRef1);  // [r11] (r11=r10+0x50)
                            const math::vpu::Vector3& lrMax1 = VolRefBBoxMax(*lpRef1);  // [r11+0x10]
                            const bool lbSeparated =
                                   (lvFatMin2.mV.x > lrMax1.mV.x) || (lrMin1.mV.x > lvFatMax2.mV.x)
                                || (lvFatMin2.mV.y > lrMax1.mV.y) || (lrMin1.mV.y > lvFatMax2.mV.y)
                                || (lvFatMin2.mV.z > lrMax1.mV.z) || (lrMin1.mV.z > lvFatMax2.mV.z);
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


}

// ===========================================================================
// rw::collision::VolumeLineQuery -- the two construction entry points (2026-09-25, crash parity FX-FOLLOWUPS).
// Their one caller is FineIntersectionTestModule::Construct @0x828B0BF0 (0x828B0CC8 / 0x828B0D28).
// ===========================================================================

// @ 0x82BB3838 -- the five descriptor entries {0, 1} (the `addic. r8, r8, -1 ; bge` loop), then entry 0 =
// {0x1B0 * results + 0x80 * volumes + 0x110 + 0x2880, 16} (the `std` of the staged pair).
// NOT X360: host VolumeLineQuery width -- the header term is sizeof(VolumeLineQuery) (VolumeQueryHostLayout.hpp).
void* VolumeLineQuery::GetResourceDescriptor(void* lpOut, int liVolumes, int liResults)
{
    u32* lpau = static_cast<u32*>(lpOut);
    for (int liEntry = 0; liEntry < 5; ++liEntry)
    {
        lpau[2 * liEntry + 0] = 0;   // m_size
        lpau[2 * liEntry + 1] = 1;   // m_alignment
    }
    lpau[0] = (sizeof(VolumeLineQuery) + sizeof(VolRef) * static_cast<u32>(liVolumes) + (sizeof(VolRef) + sizeof(Volume) + sizeof(VolumeLineSegIntersectResult)) * static_cast<u32>(liResults) + 0x2880u);
    lpau[1] = 16;
    return lpOut;
}

// @ 0x82BB3888 -- `lwz r11, 0(r3)`: null block -> null. Otherwise the four capacities and the five carves,
// in the console's store order: m_stackMax (+0xD4) = volumes, m_primBufferSize (+0xE0) = results,
// m_resBufferSize (+0x1C) = results, m_instVolMax (+0xEC) = results; m_stackVRefBuffer (+0x44) = base + header,
// m_primVRefBuffer (+0xD8) = that + 0x80 * volumes, m_instVolPool (+0xE4) = that + 0x80 * results,
// m_resBuffer (+0x10) = that + 0x60 * results, m_spatialMapQueryMem (+0xF4) = that + 0xD0 * results. Nothing
// else is written (the query is primed per test by InitQuery).
// NOT X360: host VolumeLineQuery width -- the first carve is at sizeof(VolumeLineQuery), console 0x110.
void* VolumeLineQuery::Initialize(void** lppBuffer, int liVolumes, int liResults)
{
    VolumeLineQuery* lpQuery = static_cast<VolumeLineQuery*>(lppBuffer[0]);
    if (lpQuery == 0)
    {
        return 0;
    }

    lpQuery->m_stackMax       = static_cast<u32>(liVolumes);
    lpQuery->m_primBufferSize = static_cast<u32>(liResults);
    lpQuery->m_resBufferSize  = static_cast<u32>(liResults);
    lpQuery->m_instVolMax     = static_cast<u32>(liResults);

    u8* const lpBase = reinterpret_cast<u8*>(lpQuery);
    lpQuery->m_stackVRefBuffer    = reinterpret_cast<VolRef*>(lpBase + sizeof(VolumeLineQuery));
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

    void FillLineWalkVolRef(VolRef& entry, const Volume* volume,
                            const math::vpu::Matrix44Affine* transform, u32 tag, u8 tagBits)
    {
        entry.volume = volume;
        if (transform)
        {
            entry.tmContents = *transform;
            entry.tm = &entry.tmContents;
        }
        else entry.tm = 0;
        entry.tag = tag;
        entry.numTagBits = tagBits;
    }

    void ComposeLineTransform(math::vpu::Matrix44Affine& out,
                              const math::vpu::Matrix44Affine& left,
                              const math::vpu::Matrix44Affine& right)
    {
        const __vector4 tx = right.xAxis.mV, ty = right.yAxis.mV;
        const __vector4 tz = right.zAxis.mV, tw = right.wAxis.mV;
        const __vector4 lx = left.xAxis.mV, ly = left.yAxis.mV;
        const __vector4 lz = left.zAxis.mV, lw = left.wAxis.mV;
        __vector4 x = __vmulfp(__vspltw(lx, 0), tx);
        __vector4 y = __vmulfp(__vspltw(ly, 0), tx);
        __vector4 z = __vmulfp(__vspltw(lz, 0), tx);
        __vector4 w = __vmaddfp(__vspltw(lw, 0), tx, tw);
        x = __vmaddfp(__vspltw(lx, 1), ty, x);
        y = __vmaddfp(__vspltw(ly, 1), ty, y);
        z = __vmaddfp(__vspltw(lz, 1), ty, z);
        w = __vmaddfp(__vspltw(lw, 1), ty, w);
        out.xAxis.mV = __vmaddfp(__vspltw(lx, 2), tz, x);
        out.yAxis.mV = __vmaddfp(__vspltw(ly, 2), tz, y);
        out.zAxis.mV = __vmaddfp(__vspltw(lz, 2), tz, z);
        out.wAxis.mV = __vmaddfp(__vspltw(lw, 2), tz, w);
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
    if (lpVolume->GetType() != KU_LINE_WALK_VOLUMETYPE_AGGREGATE)
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
        if (m_currInput >= m_numInputs && m_currVRef.volume == 0 && (m_primNext + m_stackNext) == 0)
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
            if (luCurrInput >= luNumInputs && m_currVRef.volume == 0 && m_stackNext == 0)
            {
                break;
            }
            if (liFull != 0)
            {
                break;
            }

            if (m_currVRef.volume == 0 && m_stackNext == 0 && luCurrInput < luNumInputs)
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

            if (m_currVRef.volume == 0)
            {
                if (m_stackNext == 0)
                {
                    continue;
                }
                --m_stackNext;
                m_currVRef = m_stackVRefBuffer[m_stackNext];
            }

            const Volume* lpVolume = m_currVRef.volume;
            m_tag        = m_currVRef.tag;
            m_numTagBits = m_currVRef.numTagBits;
            if (lpVolume->GetType() != KU_LINE_WALK_VOLUMETYPE_AGGREGATE)
            {
                if (AddPrimitiveRef(lpVolume,
                                    m_currVRef.tm,
                                    m_currVRef.tag, m_currVRef.numTagBits) == 0)
                {
                    liFull = 1;
                    continue;
                }
                m_currVRef.volume = 0;
                continue;
            }

            math::vpu::Matrix44Affine composed;
            const math::vpu::Matrix44Affine* aggregateTransform = lpVolume->GetRelativeTransform();
            if (m_currVRef.tm)
            {
                ComposeLineTransform(composed, *lpVolume->GetRelativeTransform(), *m_currVRef.tm);
                aggregateTransform = &composed;
            }
            Aggregate* aggregate = lpVolume->mAggregateData.mpAggregate;
            if (!aggregate->LineIntersectionQuery(this, aggregateTransform))
            {
                liFull = 1;
                continue;
            }
            m_curSpatialMapQuery = 0;
            m_aggIndex = 0;
            m_currVRef.volume = 0;
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
            const Volume* lpPrimitive = lrEntry.volume;
            const math::vpu::Matrix44Affine* lpTransform = lrEntry.tm;
            if ((lpPrimitive->muFlags & KU_VOLUMEFLAG_ISENABLED) == 0) continue;
            const RwBool lbHit = lpPrimitive->LineSegIntersect(m_pt1, m_pt2, lpTransform, lrResult, m_fatness);
            if (lbHit == 0)
            {
                continue;
            }

            if (m_resultsSet != ALLLINEINTERSECTIONS && lrResult.lineParam < m_endClipVal)
            {
                m_endClipVal = lrResult.lineParam;
            }
            lrResult.vRef.volume = lrEntry.volume;
            lrResult.v = m_inputVols[m_currInput - 1];
            if (lpTransform)
            {
                lrResult.vRef.tmContents = *lpTransform;
                lrResult.vRef.tm = &lrResult.vRef.tmContents;
            }
            else lrResult.vRef.tm = 0;
            lrResult.vRef.tag = m_primVRefBuffer[m_primNext].tag;
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

    return liIntersections;
}

} // namespace collision
} // namespace rw
