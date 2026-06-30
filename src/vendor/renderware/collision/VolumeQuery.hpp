#pragma once

// rw::collision::VolumeVolumeQuery / VolumeLineQuery — the two RenderWare collision
// query objects the CgsSceneManager FineIntersectionTestModule embeds. The
// construction-time entry points (GetResourceDescriptor / Initialize) are static
// factory functions the module calls to size + partition a caller-owned backing
// buffer; the runtime entry points (the in-place ctor + GetPrimitiveBBoxOverlaps /
// GetPrimitiveIntersections) drive the actual primitive-vs-primitive intersection
// pass.
//
// Ground truth (X360 BURNOUT_X360_ARTIST.XEX):
//   rw::collision::VolumeVolumeQuery::VolumeVolumeQuery         @ 0x82BB38F0  (in-place ctor)
//   rw::collision::VolumeVolumeQuery::Initialize               @ 0x82BB3980
//   rw::collision::VolumeVolumeQuery::GetResourceDescriptor    @ 0x82BB3A20
//   rw::collision::VolumeVolumeQuery::GetPrimitiveBBoxOverlaps  @ 0x82BB3AB0
//   rw::collision::VolumeVolumeQuery::GetPrimitiveIntersections @ 0x82BB3FF0
//   rw::collision::VolumeLineQuery::GetResourceDescriptor      @ 0x82BB3838
//   rw::collision::VolumeLineQuery::Initialize                 @ 0x82BB3888
//   rw::collision::VolumeLineQuery::GetAllIntersections        @ 0x82BB3820
//
// At the FineIntersectionTestModule::Construct call sites the descriptor/initialize
// entry points are invoked with the descriptor-output / buffer-table pointer in r3 and
// the volume/result counts in r4/r5 (no implicit `this`): they behave as static
// factory entry points that partition the caller-provided backing buffer. The buffer
// table (`void** ppBuffer`) holds the backing-store base at [0] with [1..4] zeroed.

#include "types.hpp"

namespace rw
{
namespace collision
{
    // -----------------------------------------------------------------------
    // VolumeBBoxQuery — declaration-only external dependency.
    //
    // FLAG: VolumeBBoxQuery is its own (as-yet un-homed) RenderWare collision TU.
    // VolumeVolumeQuery embeds two of them (one per query side) and drives them via
    // these three static-style entry points. They are declared here so the
    // VolumeVolumeQuery factory/ctor bodies can call them by name; their bodies live
    // in the VolumeBBoxQuery TU and are NOT reconstructed here (calling them is a
    // declaration-only cross-TU dependency, not a fabricated body).
    class VolumeBBoxQuery
    {
    public:
        // Fills the rw::ResourceDescriptor block at lpOut (lpOut[0] = backing size).
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);
        // Partitions the backing buffer at *lppBuffer and returns the query handle.
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);
        // Runs the cached query and returns the number of overlapping primitives.
        static int GetOverlaps(VolumeBBoxQuery* lpThis);
    };

    // -----------------------------------------------------------------------
    // VolumeVolumeQuery — stateful primitive-vs-primitive intersection query.
    //
    // The query object is constructed in place inside a caller-owned backing buffer;
    // the module keeps the returned handle (the buffer base interpreted as the query).
    // It carves two VolumeBBoxQuery sub-queries (one per volume side) plus a report
    // buffer out of the tail of that backing store.
    //
    // LAYOUT NOTE: no DWARF hints exist for this TU, so the member offsets below are
    // reconstructed from the store/load offsets in the X360 ctor (0x82BB38F0) and the
    // runtime pass (0x82BB3AB0). Members are accessed by name; gaps the asm never
    // touches are honest opaque pads sized only to land the attested fields. The two
    // embedded VolumeBBoxQuery sub-queries are reached through mpBBoxQueryA/B (the
    // ctor seeds them at this+0x50 and this+0x50+descSize).
    class VolumeVolumeQuery
    {
    public:
        // --- static factory entry points (called with no implicit `this`) -------

        // @ 0x82BB3A20 -- fills the 5-entry rw::ResourceDescriptor block at lpOut with
        // the backing-buffer size/alignment needed for liVolumes volumes and liResults
        // results. lpOut[0] receives the total size, lpOut[1] the 16-byte alignment.
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);

        // @ 0x82BB3980 -- partitions the backing buffer (*lppBuffer at [0]) into the
        // query's working arrays, constructs it in place, initialises the two embedded
        // VolumeBBoxQuery sub-queries, and returns the constructed query handle.
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);

        // --- in-place construction (this in r3) ---------------------------------

        // @ 0x82BB38F0 -- construct the query in place over the backing buffer that
        // begins at `this`. Caches the result count, points the two sub-query handles
        // at this+0x50 / this+0x50+descSize, and lays the report buffer out behind them.
        VolumeVolumeQuery* Construct(int liVolumes, int liResults);

        // --- runtime pass (this in r3) ------------------------------------------

        // @ 0x82BB3AB0 -- find every primitive-vs-primitive bounding-box overlap for
        // the current volume pair and stage them in the report buffer; returns the
        // number of overlapping primitive pairs.
        //
        // FLAG (declaration-only): the body is a multi-stage hand-VMX inner loop
        // (per-volume vtable dispatch into the collision-volume bounding-box builder,
        // slab-fatten vsubfp/vaddfp on the AABBs, vminfp/vmaxfp box folds, and a
        // vcmpgtfp/vor/vsldoi/vcmpeqfp AABB-overlap mask test) interleaved with two
        // VolumeBBoxQuery::GetOverlaps passes. Reconstructing it to scalar would
        // require inventing a per-axis overlap formula, so it is left declaration-only
        // and FLAGGED per the VMX-caution rule rather than paraphrased.
        int GetPrimitiveBBoxOverlaps();

        // @ 0x82BB3FF0 -- run GetPrimitiveBBoxOverlaps then hand the staged report
        // buffer to rw::collision::PrimitiveBatchIntersect for the exact narrow-phase
        // intersection test; returns the intersection count.
        //
        // FLAG (declaration-only): depends on GetPrimitiveBBoxOverlaps (above, VMX,
        // declaration-only) and on rw::collision::PrimitiveBatchIntersect (an as-yet
        // un-homed narrow-phase TU). Left declaration-only rather than fabricated.
        int GetPrimitiveIntersections();

        // --- members (reconstructed from the ctor / runtime store offsets) ------
        void*  mppVolumesA;        // +0x00  volume-A array base (indexed by the cursor)
        void*  mppVolumesB;        // +0x04  volume-B array base (may be null -> 0 paired)
        u32    muNumVolumes;       // +0x08  volume-pair count (loop bound)
        u32    muVolumeCursor;     // +0x0C  current volume-pair index (advanced per step)
        void*  mpIgnoreTable;      // +0x10  optional collision-mask bit table (may be null)
        f32    mfBoxFatten;        // +0x14  AABB fatten value applied on both sides
        u32*   mpReportBuffer;     // +0x18  report-buffer cursor (overlap pairs staged here)
        u32    muNumPrimitivePairs;// +0x1C  running overlap-pair count (the return value)
        u32    muMaxResults;       // +0x20  result capacity (a3 at construction)
        u32    muPad24;            // +0x24  (unwritten by the ctor; aligns to +0x28)
        u32    muNumReportEntries; // +0x28  number of populated report entries
        u32*   mpReportBufferEnd;  // +0x2C  report-buffer end-of-region marker
        u32*   mpReportBufferBase; // +0x30  report-buffer base (8*maxResults past the queries)
        u32    muMaxResultsCopy;   // +0x34  result capacity (cached again @ +0x34)
        void*  mpVolumeA;          // +0x38  current volume-A handle (drives vtable dispatch)
        void*  mpVolumeB;          // +0x3C  current volume-B handle / paired tag
        VolumeBBoxQuery* mpBBoxQueryA; // +0x40  embedded sub-query for side A (= this+0x50)
        VolumeBBoxQuery* mpBBoxQueryB; // +0x44  embedded sub-query for side B
        u8     maPad48[8];         // +0x48..+0x4F  pad up to the embedded sub-query region
        // +0x50 onward: the embedded VolumeBBoxQuery sub-queries + report buffer the
        // ctor partitions out of the backing store (opaque to this TU; reached via
        // mpBBoxQueryA/B and mpReportBuffer above).
    };

    class VolumeLineQuery
    {
    public:
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);
    };
}
}
