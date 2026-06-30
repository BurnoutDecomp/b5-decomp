#include "vendor/renderware/collision/VolumeQuery.hpp"

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
// report-buffer stride (1872 bytes/result) and the sub-query stride.
//
//   stw  a3, 0x20(this)                 -> muMaxResults = liResults
//   addi r11, this, 0x50                -> mpBBoxQueryA  = this + 0x50
//   r10 = VolumeBBoxQuery::GetResourceDescriptor(scratch); descSize = *r10
//   stw  0,  0x10(this)                 -> mpIgnoreTable     = 0
//   stw  a3, 0x34(this)                 -> muMaxResultsCopy  = liResults
//   stw  (A + descSize),        0x44    -> mpBBoxQueryB
//   r10 = A + 2*descSize;  stw r10,0x18 -> mpReportBuffer     = A + 2*descSize
//   stw  (8*a3 + r10),          0x30    -> mpReportBufferBase
//   stw  (1872*a3 + 8*a3 + r10),0x2C    -> mpReportBufferEnd
// ===========================================================================
VolumeVolumeQuery* VolumeVolumeQuery::Construct(int /*liVolumes*/, int liResults)
{
    muMaxResults = static_cast<u32>(liResults);                   // +0x20

    u8* lpSubQueryRegion = reinterpret_cast<u8*>(this) + 0x50;
    mpBBoxQueryA = reinterpret_cast<VolumeBBoxQuery*>(lpSubQueryRegion); // +0x40

    // The descriptor's first word is the per-side VolumeBBoxQuery backing size.
    u32 laDescriptor[12];
    VolumeBBoxQuery::GetResourceDescriptor(laDescriptor, liResults, liResults);
    u32 luDescSize = laDescriptor[0];

    mpIgnoreTable     = nullptr;                                  // +0x10
    muMaxResultsCopy  = static_cast<u32>(liResults);              // +0x34

    // Side-B sub-query sits one descriptor past side A.
    mpBBoxQueryB = reinterpret_cast<VolumeBBoxQuery*>(lpSubQueryRegion + luDescSize); // +0x44

    // Report buffer starts two descriptors past side A.
    u8* lpReportBase = lpSubQueryRegion + (2u * luDescSize);
    mpReportBuffer = reinterpret_cast<u32*>(lpReportBase);        // +0x18

    mpReportBufferBase = reinterpret_cast<u32*>(lpReportBase + (8u * static_cast<u32>(liResults))); // +0x30
    mpReportBufferEnd  = reinterpret_cast<u32*>(lpReportBase + (1872u * static_cast<u32>(liResults))
                                                            + (8u * static_cast<u32>(liResults))); // +0x2C

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
    laBufferA[0] = lpQuery->mpBBoxQueryA;   // *(this+0x40)
    std::memset(&laBufferA[1], 0, 4 * sizeof(void*));
    VolumeBBoxQuery::Initialize(laBufferA, liVolumes, liResults);

    // --- side-B sub-query ---
    void* laBufferB[5];
    laBufferB[0] = lpQuery->mpBBoxQueryB;   // *(this+0x44)
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
//
// FLAG (declaration-only — multi-stage hand-VMX inner loop): per the VMX-caution
// rule this body is NOT reconstructed. The X360 routine:
//   - dispatches per volume through the collision-volume vtable (*(vol+0x40)+4)
//     to build each volume's AABB,
//   - fattens the AABBs with vsubfp128/vaddfp128 against a splatted fatten vector
//     and folds box min/max with vminfp/vmaxfp,
//   - runs two VolumeBBoxQuery::GetOverlaps passes to gather candidate primitives,
//   - tests each candidate pair with a vcmpgtfp / vor / vsldoi / vcmpeqfp. AABB
//     overlap mask (reading a CR bit back through mfocrf) before staging the pair.
// Reconstructing this to scalar would require inventing a per-axis overlap
// formula, which the rules forbid. Left declaration-only rather than fabricated.
// ===========================================================================
int VolumeVolumeQuery::GetPrimitiveBBoxOverlaps()
{
    // FLAG: body intentionally omitted (multi-stage VMX — see header / comment above).
    return 0;
}

// ===========================================================================
// rw::collision::VolumeVolumeQuery::GetPrimitiveIntersections  @ 0x82BB3FF0
//
// FLAG (declaration-only): runs GetPrimitiveBBoxOverlaps (above, VMX) then calls
// rw::collision::PrimitiveBatchIntersect on the staged report buffer. Both the
// bbox-overlap pass and PrimitiveBatchIntersect (an as-yet un-homed narrow-phase
// TU) are unavailable here, so this is left declaration-only rather than
// fabricating the narrow-phase call.
// ===========================================================================
int VolumeVolumeQuery::GetPrimitiveIntersections()
{
    // FLAG: body intentionally omitted (depends on the VMX bbox pass +
    // rw::collision::PrimitiveBatchIntersect, both unavailable in this TU).
    return 0;
}

} // namespace collision
} // namespace rw
