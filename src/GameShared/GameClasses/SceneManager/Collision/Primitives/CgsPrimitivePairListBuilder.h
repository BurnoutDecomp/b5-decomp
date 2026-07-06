#pragma once

// CgsSceneManager::CgsCollision::PrimitivePairListBuilder — the writer side of a
// PrimitivePairList: it bump-allocates a packed-pair blob (via Prepare) and appends
// {CollisionHeader, primitive-A data, primitive-B data} records to it (via the
// AddPrimitive / AddPrimitivePair family). It derives from PrimitivePairList and adds
// NO data members of its own — every body operates purely on the four inherited base
// bookkeeping fields (mpaDataStream/mu16UsedData/mu16NumTests/mu16MaxDataSize) and the
// inherited CollisionHeader / EVolumeType / KAU16_VOLUME_SIZES.
//
// Each single-primitive record is { CollisionHeader (16 bytes), primitive data }.
// Each primitive-pair record is { CollisionHeader (16 bytes), primitive-A data,
// primitive-B data }. AddCollisionHeader stamps the 16-byte header; the Add* helpers
// then bump-allocate the primitive payload(s) and copy the caller's data in. A worst-case
// box/box pair record is 16 (header) + 5*16 = 96 bytes — the per-primitive reservation
// used by Prepare (X360 computes 96*count as ((count*3)<<5)).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"   // CgsGeometric::Sphere (16-byte)

namespace CgsMemory { class LinearMalloc; }

namespace CgsGeometric
{
    // Box — 80-byte packed oriented box primitive: a 3-row orientation basis
    // (rows 0/16/32 = right/up/at), a position/centre row (48) and a half-dimensions
    // row (64). Copied verbatim (five 16-byte vectors) by AddPrimitive(Box*) /
    // AddPrimitivePair. Matches KAU16_VOLUME_SIZES[E_VOLUME_TYPE_BOX] = 80. Only ever
    // forward-declared elsewhere in the tree (BrnDeformableObject.h uses
    // `struct CgsGeometric::Box;`); this is its provisional layout home. The
    // near-identical-box assert in AddPrimitivePair reads these five rows.
    struct alignas(16) Box
    {
        Vector4 maRows[5];   // +0x00 basisX, +0x10 basisY, +0x20 basisZ, +0x30 pos, +0x40 halfDims
    };
}

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitivePairListBuilder : public PrimitivePairList
    {
        // --- lifecycle ---
        void Construct();                                   // @0x82814330 — zero the four base bookkeeping fields
        bool Prepare(CgsMemory::LinearMalloc* lpMalloc, u16 lu16NumPrimitives); // @0x82814348 — bump-allocate a 96*count blob

        // AddPrimitive(Sphere*) @ 0x82814508 — append a single-sphere record: stamp a
        // sphere CollisionHeader, bump-allocate 16 bytes, copy the sphere, bump the count.
        void AddPrimitive(CgsGeometric::Sphere* lpSphere, f32 lfPadding, u16 lu16PrimitiveTag);

        // AddPrimitivePair(Box*, Box*) @ 0x82814708 — append a box-vs-box pair record:
        // (debug) assert the two boxes are not near-identical, stamp a box/box header,
        // bump-allocate two 80-byte box payloads, copy both boxes, bump the count.
        void AddPrimitivePair(CgsGeometric::Box* lpBoxA, CgsGeometric::Box* lpBoxB,
                              f32 lfPadding, u16 lu16PrimitiveTagA, u16 lu16PrimitiveTagB);

    private:
        // AddCollisionHeader(EVolumeType, f32, u16) @ 0x828143F0 — bump-allocate a 16-byte
        // CollisionHeader for a single-primitive record and stamp it.
        void AddCollisionHeader(EVolumeType lePrimType, f32 lfPadding, u16 lu16PrimitiveTag);

        // AddCollisionHeader(EVolumeType, EVolumeType, f32, u16, u16) @ 0x82814480 — the
        // two-type header stamp used by the pair helpers (owned by sub_82814480; declared
        // for coherence, defined in its own reconstruction).
        void AddCollisionHeader(EVolumeType lePrimTypeA, EVolumeType lePrimTypeB,
                                f32 lfPadding, u16 lu16PrimitiveTagA, u16 lu16PrimitiveTagB);

        // AllocateMemory @ 0x82812098 — bump allocator over mpaDataStream. Returns the
        // current write cursor (mpaDataStream + mu16UsedData) and advances mu16UsedData by
        // liBytes. Asserts the stream exists and the capacity is not exceeded.
        void* AllocateMemory(s32 liBytes);
    };
}
}
