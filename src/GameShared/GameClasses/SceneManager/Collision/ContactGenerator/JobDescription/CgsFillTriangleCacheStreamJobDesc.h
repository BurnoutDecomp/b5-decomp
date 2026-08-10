#pragma once

// =================================================================================================
// CgsSceneManager::CgsCollision::FillTriangleCacheStreamJobDesc — the STREAMED form of the
// triangle-cache fill job, plus the two fixed-stride records that flow through its stream.
//
// PS3 DWARF attests the class and its Prepare:
//   _ZN15CgsSceneManager12CgsCollision30FillTriangleCacheStreamJobDesc7PrepareE
//      PKN12CgsGeometric25PolygonSoupListSpatialMapEPN9CgsMemory24SimpleDataStreamProducerE
// and the two X360 accessors pin the member offsets (both are the ICF-folded "return this" +
// one load idiom): @0x82916F78 `lwz r3, 0(r3)` -> mpSpatialMap, @0x82916FA8 `lwz r3, 4(r3)`
// -> mpStreamProducer.
//
// ⚠️ SEPARATE HEADER ON PURPOSE (2026-08-10, cache-fill wave). The obvious home would be the
// sibling CgsFillTriangleCacheJobDesc.h, but that header drags CgsCollisionJobDescription.h and
// CgsTriangleList.h, and pulling either into a TU that also sees CgsTriangle4.h trips TWO
// pre-existing ODR forks in this tree (both reported with this wave, neither fixed here):
//   * `CgsSceneManager::CgsCollision::CollisionJobDescription` is defined TWICE, as a 256-byte
//     opaque `u8 macBuffer[256]` in CgsCollisionBatch.h:23 and as the real bookkeeping struct in
//     CgsCollisionJobDescription.h:46 -- and CollisionBatch embeds it BY VALUE, so sizeof
//     (CollisionBatch) differs between TUs depending on which header they saw;
//   * `CgsGeometric::Triangle4` is a NAMESPACE in CgsTriangleList.h:20 and a STRUCT in
//     CgsTriangle4.h:50.
// The stream descriptor does NOT derive from CollisionJobDescription (its two members are its
// whole object, per the accessors above), so nothing is lost by homing it apart.
//
// ⚠️ The two NESTED record names are BY ANALOGY, flagged: the attested sibling
// `LineWithTriangleListStreamJobDesc::StreamResult` (from the PS3 mangle of
// VehicleManagerDebugComponent::RecordStuckInCollisionLineTestResult) establishes the
// `<...StreamJobDesc>::Stream{Command,Result}` convention, but neither of THIS descriptor's
// nested types carries a symbol in either export set. The LAYOUTS are not by analogy -- every
// field is read out of the X360 asm and corroborated by the DWARF signature of the synchronous
// twin `BaseCollisionGenerator::FillTriangleCache(const PolygonSoupListSpatialMap*,
// const Sphere*, Triangle4*, u16, u32, u16)`, which takes exactly the command's three fields.
// =================================================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"   // CgsGeometric::Sphere (by value)

namespace CgsGeometric
{
    // Both pointer-use only here. Full homes:
    //   PolygonSoupListSpatialMap -- Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h
    //   Triangle4                 -- Geometric/Primitives/CgsTriangle4.h
    struct PolygonSoupListSpatialMap;
    struct Triangle4;
}

// The stream this descriptor drives (pointer use only). Full home:
// GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h.
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct FillTriangleCacheStreamJobDesc
    {
        // One posted fill request. sizeof == 32, which is not inferred: the factory
        // BaseCollisionGenerator::CreateStreamProducer @0x828109F8 hands
        // SimpleDataStreamProducer::GetRequiredBufferSizes a command size of 0x20
        // (`li r4, 0x20` @0x82810ACC) and a result size of 0x10 (`li r6, 0x10` @0x82810AC4).
        //
        // Seats written by TriangleCacheManager::StartUpdateTriangleCaches
        // @0x828BEED0..0x828BEEE0:
        //   +0x00  mCacheSphere              two ld/std pairs copying CacheSlot::mLastCachedSphere
        //   +0x10  mpDestinationTriangles    `stw r11, 0x10(cmd)` = &mpaTriangleCache[slot window]
        //   +0x14  mu16MaxNumTriangleBatches `sth r10, 0x14(cmd)` with r10 = 0x2C = 44
        // (Sphere is alignas(16), so the host sizeof is 32 as well -- checked, not assumed.)
        struct StreamCommand
        {
            CgsGeometric::Sphere     mCacheSphere;               // +0x00
            CgsGeometric::Triangle4* mpDestinationTriangles;     // +0x10
            u16                      mu16MaxNumTriangleBatches;  // +0x14
        };

        // One fill answer, read back by TriangleCacheManager::EndUpdateTriangleCaches
        // @0x828BF2C8/0x828BF2D4: `lhz r10, 0(result)` -> the slot's batch count (widened to s32
        // on store) and `lwz r11, 4(result)` -> the overflow count (narrowed to s16 on store).
        // sizeof == 16 (the producer's result size, above). ⚠️ The two READ fields only span the
        // first 8 bytes; the trailing 8 are named as UNREAD rather than as padding, and are
        // carried explicitly because 16 is the record's contract with the producer (the stream
        // strides by miAlignedResultSize, but the SIZE is what CreateStreamProducer declared).
        // Measured, not assumed: a first cut of this struct came out 8 bytes and the wave's
        // runtime probe printed `sizeofStreamResult=8` -- caught and corrected.
        struct StreamResult
        {
            u16 mu16NumCachedTriangleBatches;  // +0x00
            u16 mu16Unread02;                  // +0x02  never read by EndUpdateTriangleCaches
            s32 miOverflow;                    // +0x04
            u32 mauUnread08[2];                // +0x08  never read by EndUpdateTriangleCaches
        };

        const CgsGeometric::PolygonSoupListSpatialMap* mpSpatialMap;     // +0x00
        CgsMemory::SimpleDataStreamProducer*           mpStreamProducer; // +0x04
    };
}
}
