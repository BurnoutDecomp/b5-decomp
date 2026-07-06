#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairListBuilder.h"

#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // CgsMemory::LinearMalloc::Malloc / GetAlignment
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

#include <cmath>   // std::sqrt

// CgsSceneManager::CgsCollision::PrimitivePairListBuilder — the writer bodies. Construct/Prepare
// bring up the packed-pair blob; AddCollisionHeader stamps the 16-byte record header; AddPrimitive /
// AddPrimitivePair bump-allocate the primitive payload(s), copy the caller's data in verbatim, and
// bump the record count. AllocateMemory is the bump allocator every append draws from.

namespace CgsSceneManager
{
namespace CgsCollision
{
    // -------------------------------------------------------------------------
    // PrimitivePairListBuilder::Construct @ 0x82814330
    //
    // Zero the four inherited PrimitivePairList bookkeeping members. The asm
    // materialises 0 once (li r11,0) and stores it as a word at +0x00 and as
    // halfwords at +0x04/+0x06/+0x08.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::Construct()
    {
        mpaDataStream   = nullptr;   // +0x00  (stw of 0)
        mu16UsedData    = 0;         // +0x04  (sth of 0)
        mu16NumTests    = 0;         // +0x06  (sth of 0)
        mu16MaxDataSize = 0;         // +0x08  (sth of 0)
    }

    // -------------------------------------------------------------------------
    // PrimitivePairListBuilder::Prepare @ 0x82814348
    //
    // Reserve a packed-pair blob big enough for lu16NumPrimitives pair records:
    // each record is a fixed 96 bytes (16-byte CollisionHeader + up to five
    // 16-byte primitive slots, the box/box worst case). The asm computes
    // 96 * count via ((count * 3) << 5) and bump-allocates it from lpMalloc.
    // -------------------------------------------------------------------------
    bool PrimitivePairListBuilder::Prepare(CgsMemory::LinearMalloc* lpMalloc, u16 lu16NumPrimitives)
    {
        const s32 liRequiredMemory = 96 * lu16NumPrimitives;

        u8* lpMemory = static_cast<u8*>(lpMalloc->Malloc(liRequiredMemory));
        CGS_ASSERT(lpMemory != nullptr, "lpMemory");
        CGS_ASSERT(lpMalloc->GetAlignment() >= 16, "lpMalloc->GetAlignment() >= 16");

        mpaDataStream   = lpMemory;
        mu16MaxDataSize = static_cast<u16>(liRequiredMemory);
        mu16UsedData    = 0;
        mu16NumTests    = 0;

        return true;
    }

    // -------------------------------------------------------------------------
    // AllocateMemory @ 0x82812098
    //
    // Bump allocator over the base list's data stream. Returns the current write
    // cursor (mpaDataStream + mu16UsedData) and advances mu16UsedData by liBytes.
    // The X360 asserts the stream pointer is non-null and that the post-bump used
    // count does not exceed the capacity (a signed 32-bit compare of the zero-
    // extended halfword used-count + liBytes against the halfword capacity).
    // -------------------------------------------------------------------------
    void* PrimitivePairListBuilder::AllocateMemory(s32 liBytes)
    {
        CGS_ASSERT(mpaDataStream != NULL, "mpaDataStream != NULL");
        CGS_ASSERT(static_cast<s32>(mu16UsedData) + liBytes <= static_cast<s32>(mu16MaxDataSize),
                   "Trying to use too much memory");

        void* lpMemory = mpaDataStream + mu16UsedData;
        mu16UsedData = static_cast<u16>(mu16UsedData + liBytes);
        return lpMemory;
    }

    // -------------------------------------------------------------------------
    // AddCollisionHeader(EVolumeType, f32, u16) @ 0x828143F0
    //
    // Bump-allocate a 16-byte CollisionHeader for a single-primitive record and
    // stamp it. The X360 sets both type bytes to lePrimType, header-type 0, the
    // checksum byte to 2*lePrimType (== A + B + HeaderType), primitive-A data size
    // to KAU16_VOLUME_SIZES[lePrimType], primitive-B data size 0, tag A to the
    // caller's tag and tag B to 0xFFFF.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddCollisionHeader(EVolumeType lePrimType,
                                                      f32 lfPadding,
                                                      u16 lu16PrimitiveTag)
    {
        CollisionHeader* lpHeader =
            static_cast<CollisionHeader*>(AllocateMemory(sizeof(CollisionHeader)));

        const u8 lu8Type = static_cast<u8>(lePrimType);

        lpHeader->mfPadding                 = lfPadding;
        lpHeader->mu8PrimTypeA              = lu8Type;
        lpHeader->mu8PrimTypeB              = lu8Type;
        lpHeader->mu8HeaderType             = 0;
        lpHeader->mu16PrimBDataSize         = 0;
        lpHeader->mu16PrimADataSize         = KAU16_VOLUME_SIZES[lu8Type];
        lpHeader->mu16PrimitiveATag         = lu16PrimitiveTag;
        lpHeader->mu16PrimitiveBTag         = 0xFFFF;
        // checksum byte == mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType == 2*type
        lpHeader->mu8DataPaddingAndCheckSum = static_cast<u8>(lu8Type << 1);
    }

    // -------------------------------------------------------------------------
    // AddPrimitive(Sphere*) @ 0x82814508
    //
    // Append a single-sphere record: stamp a sphere CollisionHeader (padding/tag
    // forwarded), bump-allocate 16 bytes for the sphere payload, copy the caller's
    // sphere verbatim (one 16-byte vector; the asm does a two-dword ld/std copy),
    // and bump the record count.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitive(CgsGeometric::Sphere* lpSphere,
                                                f32 lfPadding,
                                                u16 lu16PrimitiveTag)
    {
        AddCollisionHeader(E_VOLUME_TYPE_SPHERE, lfPadding, lu16PrimitiveTag);

        CgsGeometric::Sphere* lpPrimitive =
            static_cast<CgsGeometric::Sphere*>(AllocateMemory(sizeof(CgsGeometric::Sphere)));
        *lpPrimitive = *lpSphere;

        ++mu16NumTests;
    }

    // -------------------------------------------------------------------------
    // AddPrimitivePair(Box*, Box*) @ 0x82814708
    //
    // Append a box-vs-box pair record. First (debug) the two boxes are tested for
    // near-identity across all five 16-byte rows of the packed box: the three
    // orientation-basis rows (0/16/32) are near-identical when dot(rowA,rowB)
    // exceeds a near-1 threshold (flt_82005450), and the position row (48) and
    // half-dimensions row (64) are near-identical when |rowA - rowB| < 0.1
    // (flt_82004014). If all five agree the asm fires an assert. Then it stamps a
    // two-type (BOX/BOX) CollisionHeader, bump-allocates two 80-byte box payloads,
    // copies box A and box B verbatim, and bumps the record count.
    //
    // NOTE: flt_82005450 (the axis-alignment threshold) is not recoverable from the
    // dossier rodata; it is a near-1 dot-product threshold used only by the debug
    // assert and is named below.
    // -------------------------------------------------------------------------
    void PrimitivePairListBuilder::AddPrimitivePair(CgsGeometric::Box* lpBoxA,
                                                    CgsGeometric::Box* lpBoxB,
                                                    f32 lfPadding,
                                                    u16 lu16PrimitiveTagA,
                                                    u16 lu16PrimitiveTagB)
    {
        // Axis-alignment threshold for the three basis rows (dot > threshold).
        static const f32 KF_BOX_AXIS_ALIGNED_THRESHOLD = 0.999f; // flt_82005450 (near 1)
        static const f32 KF_BOX_NEAR_IDENTICAL_DISTANCE = 0.1f;   // flt_82004014

        const auto Dot3 = [](const Vector4& lvA, const Vector4& lvB) -> f32
        {
            const f32* la = reinterpret_cast<const f32*>(&lvA);
            const f32* lb = reinterpret_cast<const f32*>(&lvB);
            return la[0] * lb[0] + la[1] * lb[1] + la[2] * lb[2];
        };
        const auto Length3 = [](const Vector4& lvA, const Vector4& lvB) -> f32
        {
            const f32* la = reinterpret_cast<const f32*>(&lvA);
            const f32* lb = reinterpret_cast<const f32*>(&lvB);
            const f32 lfDx = la[0] - lb[0];
            const f32 lfDy = la[1] - lb[1];
            const f32 lfDz = la[2] - lb[2];
            const f32 lfLenSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
            return std::sqrt(lfLenSq); // vrsqrtefp + Newton refine -> length; 0 -> 0
        };

        // Rows 0/16/32: orientation basis aligned when dot(rowA,rowB) > threshold.
        const bool lbAxis0 = Dot3(lpBoxA->maRows[0], lpBoxB->maRows[0]) > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        const bool lbAxis1 = Dot3(lpBoxA->maRows[1], lpBoxB->maRows[1]) > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        const bool lbAxis2 = Dot3(lpBoxA->maRows[2], lpBoxB->maRows[2]) > KF_BOX_AXIS_ALIGNED_THRESHOLD;
        // Row 64 (half-dimensions) and row 48 (position): near when |A - B| < 0.1.
        const bool lbDims  = KF_BOX_NEAR_IDENTICAL_DISTANCE > Length3(lpBoxA->maRows[4], lpBoxB->maRows[4]);
        const bool lbPos   = KF_BOX_NEAR_IDENTICAL_DISTANCE > Length3(lpBoxA->maRows[3], lpBoxB->maRows[3]);

        CGS_ASSERT(!(lbAxis0 && lbAxis1 && lbAxis2 && lbDims && lbPos),
                   "Attempting to collide two near-identical boxes\n");

        // Two-type (BOX/BOX) header for the pair record.
        AddCollisionHeader(E_VOLUME_TYPE_BOX, E_VOLUME_TYPE_BOX,
                           lfPadding, lu16PrimitiveTagA, lu16PrimitiveTagB);

        CgsGeometric::Box* lpPrimitiveA =
            static_cast<CgsGeometric::Box*>(AllocateMemory(sizeof(CgsGeometric::Box)));
        *lpPrimitiveA = *lpBoxA;

        CgsGeometric::Box* lpPrimitiveB =
            static_cast<CgsGeometric::Box*>(AllocateMemory(sizeof(CgsGeometric::Box)));
        *lpPrimitiveB = *lpBoxB;

        ++mu16NumTests;
    }
}
}
