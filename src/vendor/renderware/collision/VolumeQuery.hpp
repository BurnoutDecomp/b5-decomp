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
//
// MEMBER NAMES (wave-2 correction): the DecFIGS DWARF DOES carry this class
// (references/DecFIGS/dwarfdump/SDKs/EATech/include/cmn/rw/collision/
// volumevolumequery.h:191-220), and every X360 load/store offset of the ctor
// (0x82BB38F0) and the runtime bodies (0x82BB3AB0/0x82BB3FF0) lands exactly on
// the canonical console layout -- so the wave-1 guessed member names were
// renamed to the canonical DWARF ones. Two wave-1 guesses were semantically
// wrong and are corrected by the DWARF + the 0x82BB3FF0 call site:
//   +0x04 is the input MATRIX array (not a "volume-B array");
//   +0x3C is the query MATRIX (not a "current volume-B handle");
//   +0x2C is the GPInstance instancing scratch (not a report-buffer end);
//   +0x30 is the PrimitivePairIntersectResult buffer (the ctor's "1872
//         bytes/result" == the 0x750 PPIR stride).
//
// This header stays LIGHT deliberately: the pointer-bearing member block below
// needs only forward declarations, so the CgsSceneManager consumers (which
// carry the BrnCommonTypes vpu vocabulary) never pull the SDKs/EATech vpu
// headers through it. The full VolumeBBoxQuery class (which embeds AABBox /
// VolRef by value) lives in VolumeBBoxQuery.hpp.

#include "types.hpp"
#include "vendor/renderware/collision/BitTable.hpp"   // rw::BitTable::Storage (m_cullTable)

namespace rw
{
namespace math
{
namespace vpu
{
    // Forward declaration only (pointer members below). NOTE: two vpu
    // vocabularies exist in the tree (SDKs/EATech class vs the vendor-include
    // struct); this fwd decl binds to whichever definition the including TU
    // carries -- the members below are pointers either way.
    class Matrix44Affine;
}
}

namespace collision
{
    struct Volume;                        // CollisionVolume.hpp
    struct VolRef1xN;                     // GPInstance.hpp
    struct GPInstance;                    // GPInstance.hpp
    struct PrimitivePairIntersectResult;  // GPInstance.hpp

    // Full class (canonical DWARF member block, embeds AABBox/VolRef by
    // value): vendor/renderware/collision/VolumeBBoxQuery.hpp.
    class VolumeBBoxQuery;

    // -----------------------------------------------------------------------
    // VolumeVolumeQuery — stateful primitive-vs-primitive intersection query.
    //
    // The query object is constructed in place inside a caller-owned backing buffer;
    // the module keeps the returned handle (the buffer base interpreted as the query).
    // It carves two VolumeBBoxQuery sub-queries (one per volume side) plus a report
    // buffer out of the tail of that backing store.
    // -----------------------------------------------------------------------
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

        // @ 0x82BB3AB0 -- broad phase: for every remaining input volume, fatten
        // both sides' AABBs by m_padding, run the two embedded VolumeBBoxQuery
        // passes, and stage every primitive pair whose fattened AABBs overlap as
        // packed VolRef1xN groups in m_volRefPairBuffer. Returns the staged pair
        // count. (Reconstructed in wave 2's dedicated VMX pass; the wave-1
        // declaration-only FLAG is lifted.)
        int GetPrimitiveBBoxOverlaps();

        // @ 0x82BB3FF0 -- run GetPrimitiveBBoxOverlaps then hand the staged
        // groups to rw::collision::PrimitiveBatchIntersect for the exact
        // narrow-phase test; returns the intersection count. (Canonical DWARF
        // return is uint32_t; kept `int` as committed to avoid rippling the
        // CgsSceneManager call sites -- the value is the s32 count either way.)
        int GetPrimitiveIntersections();

        // --- members (canonical DWARF volumevolumequery.h:191-220 names; console
        //     offsets in the comments; pointers widen on x64) --------------------
        const Volume**                    m_inputVols;                 // +0x00
        const math::vpu::Matrix44Affine** m_inputMats;                 // +0x04
        u32                               m_numInputs;                 // +0x08
        u32                               m_currInput;                 // +0x0C
        // Canonical type is `const BitTable*`; the X360 runtime handle is the
        // BitTable::Storage image (the 0x82BB3AB0 body reads muHeight at +4 and
        // the packed words at +0xC), so it is typed as the Storage pointer.
        const rw::BitTable::Storage*      m_cullTable;                 // +0x10
        f32                               m_padding;                   // +0x14
        // Canonical type is `VolRefPair*`; the X360 asm stages/consumes the
        // variable-length VolRef1xN groups (see the PrimitiveBatchIntersect
        // note in GPInstance.hpp), so it is typed VolRef1xN*.
        VolRef1xN*                        m_volRefPairBuffer;          // +0x18
        u32                               m_volRefPairCount;           // +0x1C
        u32                               m_volRefPairBufferSize;      // +0x20
        VolRef1xN*                        m_volRef1xNBuffer;           // +0x24 (unwritten by the ctor)
        u32                               m_volRef1xNCount;            // +0x28
        GPInstance*                       m_instancingSPR;             // +0x2C
        PrimitivePairIntersectResult*     m_intersectionBuffer;        // +0x30
        s32                               m_intersectionBufferMaxSize; // +0x34
        const Volume*                     m_queryVol;                  // +0x38
        const math::vpu::Matrix44Affine*  m_queryMtx;                  // +0x3C
        VolumeBBoxQuery*                  m_bBoxQueryAtoB;             // +0x40 (= this+0x50)
        VolumeBBoxQuery*                  m_bBoxQueryBtoA;             // +0x44
        u8                                maPad48[8];                  // +0x48..+0x4F
        // +0x50 onward: the embedded VolumeBBoxQuery sub-queries + report buffer the
        // ctor partitions out of the backing store (opaque to this header; reached
        // via m_bBoxQueryAtoB/BtoA and m_volRefPairBuffer above).
    };

    class VolumeLineQuery
    {
    public:
        static void* GetResourceDescriptor(void* lpOut, int liVolumes, int liResults);
        static void* Initialize(void** lppBuffer, int liVolumes, int liResults);
    };
}
}
