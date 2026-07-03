#include "vendor/renderware/collision/VolumeQuery.hpp"

#include "vendor/renderware/collision/AABBox.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"        // VolRef1xN / PPIR / PrimitiveBatchIntersect
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"   // the embedded sub-queries

#include <cstring>  // memset

namespace rw
{
namespace collision
{

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

    u8* lpSubQueryRegion = reinterpret_cast<u8*>(this) + 0x50;
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
    // groups occupy the leading 8*results bytes (console words; see the
    // capacity FLAG at GetPrimitiveBBoxOverlaps); the 0x750-stride
    // PrimitivePairIntersectResult array sits behind them, and the GPInstance
    // instancing scratch behind that.
    u8* lpReportBase = lpSubQueryRegion + (2u * luDescSize);
    m_volRefPairBuffer = reinterpret_cast<VolRef1xN*>(lpReportBase);        // +0x18

    m_intersectionBuffer = reinterpret_cast<PrimitivePairIntersectResult*>(
        lpReportBase + (8u * static_cast<u32>(liResults)));                 // +0x30
    m_instancingSPR = reinterpret_cast<GPInstance*>(
        lpReportBase + (1872u * static_cast<u32>(liResults))
                     + (8u * static_cast<u32>(liResults)));                 // +0x2C

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
// + 192, where bboxDescSize is the side sub-query's descriptor size.
// ===========================================================================
void* VolumeVolumeQuery::GetResourceDescriptor(void* lpOut, int /*liVolumes*/, int liResults)
{
    // Side sub-query descriptor (first word = its backing size).
    u32 laBBoxDesc[12];
    VolumeBBoxQuery::GetResourceDescriptor(laBBoxDesc, liResults, liResults);
    u32 luBBoxDescSize = laBBoxDesc[0];

    // Total backing size for the two sub-queries + the report buffer.
    u32 luTotalSize = 2u * (luBBoxDescSize + 40u)
                    + 2072u * static_cast<u32>(liResults)
                    + 192u;

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

    const VolumeVTableView* GetVolumeVTable(const Volume* lpVolume)
    {
        return *reinterpret_cast<const VolumeVTableView* const*>(
            reinterpret_cast<const u8*>(lpVolume) + 0x40);
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
// FLAG (x64 capacity width): the staging budget keeps the console BYTE
// arithmetic exactly (budget = 8*m_volRefPairBufferSize; 12 bytes per group
// header, 4 per staged pair -- console words). On x64 the VolRef1xN groups
// are pointer-widened (16-byte header + 8 bytes/pair), so the same metering
// admits more staged bytes than the console constants imply; the backing
// store carved by Construct uses the same console units, so the whole
// query-object sizing chain (GetResourceDescriptor -> Construct -> this
// metering) needs one coordinated widening pass. Kept console-exact per the
// translation contract (identical staging DECISIONS), not silently improved.
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
int VolumeVolumeQuery::GetPrimitiveIntersections()
{
    GetPrimitiveBBoxOverlaps();                                 // bl 0x82BB3AB0 (result unused)

    return PrimitiveBatchIntersect(
        m_intersectionBuffer,                                   // r3 <- lwz 0x30(r31)
        m_intersectionBufferMaxSize,                            // r4 <- lwz 0x34(r31)
        m_instancingSPR,                                        // r5 <- lwz 0x2C(r31)
        m_volRefPairBuffer,                                     // r6 <- lwz 0x18(r31)
        static_cast<s32>(m_volRef1xNCount),                     // r7 <- lwz 0x28(r31)
        m_padding);                                             // f1 <- lfs 0x14(r31)
}

} // namespace collision
} // namespace rw
