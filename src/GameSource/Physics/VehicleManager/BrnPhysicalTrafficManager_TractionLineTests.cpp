// =================================================================================================
// GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_TractionLineTests.cpp
//
// THE GROUND, FOR TRAFFIC. The traffic leg of the traction-line chain, both halves:
//   PhysicalTrafficManager::AddTrafficTractionLineTests      @0x8261D580 (418 insns)
//   PhysicalTrafficManager::ReadTrafficTractionLineTestResults @0x8262D2B8 (291 insns)
//
// THEY ARE ONE LEG AND MUST NEVER BE SPLIT. All three harvests (race car, traffic, player
// stuck) walk ONE shared result cursor that EndVehicleTractionLineTests copies once and passes in
// turn (BrnVehicleManager_TractionLineTests.cpp @0x82633DA4). An Add without its Read leaves the
// next leg reading this leg's records; a Read without its Add reads the previous leg's.
// Both conductor gates (BrnPhysicsConductorGates.cpp:409 / :418) are retired together with this.
//
// WHY THIS ROUND NEEDS IT: PhysicalTrafficManager::ReadUpdatedBodies applies
// `mLinearVelocity.y -= KF_GRAVITY*dt` to every fully-physical traffic body every frame with
// nothing opposing it. The road reaction arrives ONLY through AddTractionPoint, which is what this
// harvest calls. Without this pair a promoted traffic car falls through the road -- the trap the
// race-car banner records at BrnVehicleManager_ReadUpdatedBodies.cpp:30-35, now armed for traffic.
//
// THE TRAFFIC COMMAND CARRIES **FIVE** LINES, NOT FOUR. Four wheel suspension probes plus a
// fifth, body-origin, straight-down probe whose result feeds
// SimpleVehiclePhysics::SetAboveGroundTestResult -- the above-ground (down-ray) latch this manager
// resets every frame in ResetAboveGroundTestResults and that ValidateTrafficContact reads. The
// `stw r10(5), 0xA8(r30)` at 0x8261D9F4 is the witness (both other producers write 4), and it is
// why StreamCommand::KI_MAX_LINES_PER_COMMAND is 5.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // GetTractionLine / AddTractionPoint / SetAboveGroundTestResult / EVehicleDrivenWheel
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"                       // KI_MAX_ACTIVE_RACE_CARS (the cache-slot bias)

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithTriangleListStreamJobDesc.h"  // StreamCommand
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"  // TriangleList::CheckAlignment
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"       // TriangleCacheInterface
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                  // Triangle4::AssertIsValid
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstddef>   // offsetof (the record layout gates)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The console's downward probe length for the fifth (body-origin) line: flt_82004A20 == 10.0f,
    // splatted and subtracted from the .y lane only (`vspltw v13,v13,0 ; vsubfp ; vrlimi128 v12,
    // v13, 4, 0` at 0x8261D9F0..0x8261DA10 -- mask 4 == insert lane 1 == .y).
    const f32 KF_TRAFFIC_ABOVE_GROUND_PROBE_LENGTH = 10.0f;

    // ---------------------------------------------------------------------------------------
    // The 192-byte line-test RESULT record as the TRAFFIC harvest reads it. Same stride and same
    // seats as the race-car model in BrnVehicleManager_TractionLineTests.cpp, but spelled with
    // FIVE line slots because this leg posts five and reads the fifth:
    //     lbz  r11, 0xB4(result + line)     -> +180 + line   u8   per-line hit flag  (lines 0..3)
    //     lbz  r11, 0xB8(result)            -> +184          u8   line 4's hit flag
    //     lvx128 v1, 16*line (result)       -> +0x00 + 16*l  16B  hit position
    //     lvx128 v2, 16*(line+5) (result)   -> +0x50 + 16*l  16B  hit normal
    //     lwzx r5, 4*(line+40) (result)     -> +0xA0 + 4*l   u32  surface / collision tag
    // The race-car model names +0x40 / +0xB0 "unread"; they are line 4's position and tag, which
    // this leg DOES read (0x8262D524..0x8262D540). File-local descriptive model, not a claim
    // about a shipped identifier -- the stream is untyped on both sides.
    // ---------------------------------------------------------------------------------------
    const s32 KI_TRAFFIC_LINES_PER_COMMAND = 5;

    struct TrafficTractionLineResultRecord
    {
        f32 mafHitPosition[KI_TRAFFIC_LINES_PER_COMMAND][4];   // +0x00
        f32 mafHitNormal[KI_TRAFFIC_LINES_PER_COMMAND][4];     // +0x50
        u32 mauSurfaceTag[KI_TRAFFIC_LINES_PER_COMMAND];       // +0xA0
        u8  mau8HitFlags[KI_TRAFFIC_LINES_PER_COMMAND];        // +0xB4
        u8  mau8UnreadTail[7];                                 // +0xB9 (never read by this leg)
    };

    static_assert(sizeof(TrafficTractionLineResultRecord) == 192,
                  "the line-test result stride is the 0xC0 the producer is constructed with");
    static_assert(offsetof(TrafficTractionLineResultRecord, mafHitNormal) == 0x50,
                  "hit normals at 16*(line+5) -- lvx128 v2, r11, r30 @0x8262D4FC");
    static_assert(offsetof(TrafficTractionLineResultRecord, mauSurfaceTag) == 0xA0,
                  "surface tags at 4*(line+40) -- lwzx r5, r9, r30 @0x8262D500");
    static_assert(offsetof(TrafficTractionLineResultRecord, mau8HitFlags) == 0xB4,
                  "hit flags at 180+line -- lbz r11, 0xB4(r11) @0x8262D4D0; line 4 at 0xB8");
}

// =================================================================================================
// PhysicalTrafficManager::AddTrafficTractionLineTests   @0x8261D580   (418 insns)
//
// One 176-byte stream command per LIVE traffic vehicle, in mUsedTrafficVehicles order. The
// Hex-Rays rendering is dominated by the inlined BitArray<20> walk and its CgsBitArray.h:203
// "invalid index" StrStreamBase assert; read as ASM it is the plain
// GetFirstNonZeroBit/GetNextNonZeroBit walk this file's sibling harvest uses, so it is written
// that way here.
//
// Per live vehicle:
//   1. 0x8261D7xx  the two BOUND asserts h:740 (`liVehicle >= 0`) and h:741, then
//      &mpaTrafficVehicles[i] and the "lpTrafficVehicle->IsFullyPhysical()" assert whose own
// __FILE__/__LINE__ is BrnPhysicalTrafficManager.cpp:1424. IT IS AN ASSERT, NOT A FILTER:
//      the console posts a command for EVERY set bit of mUsedTrafficVehicles and only complains
//      when one is not fully physical. Reproduced as an assert; turning it into a `continue`
//      would desynchronise the shared result cursor the harvest walks.
//   2. the cache window, inlined GetCache/GetNumCachedTriangleBatches over slot i + 8 (it even
//      carries their own asserts, CgsSceneManagerModuleIO.h:1286/:1295 and
//      CgsCachedTriangleList.h:153), so both are CALLED here rather than re-open-coded. The +8 is
//      the race-car block: VehicleManager claims cache slots 0..7 and traffic owns 8..27, exactly
//      as PrepareTriangleCache/UpdateTriangleCache already spell it.
//   3. one command off lpStreamProducer, seated with that window, then the shipped
//      TriangleList::CheckAlignment + per-batch Triangle4::AssertIsValid sweep.
//   4. four wheels: GetTractionLine(wheel, start, end) straight into maLineStart/maLineEnd[w].
//   5. THE FIFTH LINE: start = the body's world position (`lvx128 v0, r27, r15` with r15 == 0x40
//      == mTransform.wAxis), end = the same point with .y lowered by 10.0f.
//   6. miNumLines = 5, and the command counter is bumped once per VEHICLE (the return is a
//      COMMAND count, which is what StartVehicleTractionLineTests adds to miNumSPUTractionLineTests).
//
// lpTractionContactGen IS UNREAD past its null assert -- r4 is never touched again in the 418
// instructions. Declared and asserted, as the console has it.
// =================================================================================================
s32 PhysicalTrafficManager::AddTrafficTractionLineTests(
        CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
        CgsMemory::SimpleDataStreamProducer* lpStreamProducer,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface)
{
    CGS_ASSERT(lpTractionContactGen != 0, "lpTractionContactGen != NULL");   // .cpp:1405
    CGS_ASSERT(lpStreamProducer != 0, "lpStreamProducer != NULL");           // .cpp:1406
    CGS_ASSERT(lpCacheInterface != 0, "lpCacheInterface != NULL");           // .cpp:1407

    s32 liNumCommands = 0;

    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(liVehicle >= 0, "liVehicle >= 0");                                  // h:740
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // h:741

        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[liVehicle];

        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "lpTrafficVehicle->IsFullyPhysical()");                             // .cpp:1424

        // 2. this vehicle's window into the shared triangle cache (slots 8..27).
        const s32 liCacheSlot = liVehicle + VehicleManager::KI_MAX_ACTIVE_RACE_CARS;
        const CgsGeometric::Triangle4* const lpTriangles = lpCacheInterface->GetCache(liCacheSlot);
        const s32 liNumBatches = lpCacheInterface->GetNumCachedTriangleBatches(liCacheSlot);

        // the second copy of the type predicate, inlined from GetVehiclePhysics() -- kept as shipped
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "IsFullyPhysical()");                                               // h:391

        const SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;

        // 3. one command, seated with that window.
        CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc::StreamCommand*
            lpCommand = 0;
        lpStreamProducer->AllocateCommand(reinterpret_cast<void**>(&lpCommand));

        lpCommand->mpTriangles          = lpTriangles;
        lpCommand->miNumTriangleBatches = liNumBatches;

        // The console passes &cmd->mpTriangles -- the {const Triangle4*, s32} pair at +0xA0 IS a
        // TriangleList, which is why CheckAlignment takes it unchanged.
        CgsSceneManager::CgsCollision::TriangleList* const lpTriangleList =
            reinterpret_cast<CgsSceneManager::CgsCollision::TriangleList*>(&lpCommand->mpTriangles);
        lpTriangleList->CheckAlignment();

        for (s32 liBatch = 0; liBatch < lpCommand->miNumTriangleBatches; ++liBatch)
        {
            lpCommand->mpTriangles[liBatch].AssertIsValid();
        }

        // 4. four wheels, four suspension probes.
        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            Vector3 lvLineStart;
            Vector3 lvLineEnd;
            lpBody->GetTractionLine(static_cast<EVehicleDrivenWheel>(liWheel),
                                    lvLineStart, lvLineEnd);
            // The command's line arrays are Vector4 and GetTractionLine hands back Vector3; both
            // are the same 16-byte four-lane register, but distinct C++ types here, so all four
            // lanes are copied explicitly -- the console's lvx128/stvx128 pair moves the whole
            // register and the w lane must travel with it.
            lpCommand->maLineStart[liWheel].x = lvLineStart.x;
            lpCommand->maLineStart[liWheel].y = lvLineStart.y;
            lpCommand->maLineStart[liWheel].z = lvLineStart.z;
            lpCommand->maLineStart[liWheel].w = lvLineStart.w;
            lpCommand->maLineEnd[liWheel].x   = lvLineEnd.x;
            lpCommand->maLineEnd[liWheel].y   = lvLineEnd.y;
            lpCommand->maLineEnd[liWheel].z   = lvLineEnd.z;
            lpCommand->maLineEnd[liWheel].w   = lvLineEnd.w;
        }

        // 5. the fifth line: the body-origin straight-down above-ground probe.
        //    start = mTransform.wAxis (body+0x40), end = start with .y -= 10.0f, all other lanes
        //    carried through unchanged (the vrlimi128 mask-4 insert).
        const Matrix44Affine& lrBodyTransform = lpBody->GetTransform();
        const s32 liGroundLine = KI_TRAFFIC_LINES_PER_COMMAND - 1;   // == 4
        lpCommand->maLineStart[liGroundLine].x = lrBodyTransform.wAxis.x;
        lpCommand->maLineStart[liGroundLine].y = lrBodyTransform.wAxis.y;
        lpCommand->maLineStart[liGroundLine].z = lrBodyTransform.wAxis.z;
        lpCommand->maLineStart[liGroundLine].w = lrBodyTransform.wAxis.w;
        lpCommand->maLineEnd[liGroundLine]     = lpCommand->maLineStart[liGroundLine];
        lpCommand->maLineEnd[liGroundLine].y   =
            lrBodyTransform.wAxis.y - KF_TRAFFIC_ABOVE_GROUND_PROBE_LENGTH;

        // 6. AS SHIPPED: traffic posts all FIVE of the command's line slots.
        lpCommand->miNumLines = KI_TRAFFIC_LINES_PER_COMMAND;

        ++liNumCommands;
    }

    return liNumCommands;
}

// =================================================================================================
// PhysicalTrafficManager::ReadTrafficTractionLineTestResults   @0x8262D2B8   (291 insns)
//
// One record per command, in the same mUsedTrafficVehicles order Add posted them, off the SHARED
// cursor. Per vehicle:
//   * lines 0..3 whose hit flag (+180+line) is set -> SimpleVehiclePhysics::AddTractionPoint with
//     {position, normal, surface tag}. THAT is what raises Wheel::mRoadContact.mbIsOnGround and
//     gives the suspension something to push against.
//   * line 4's hit flag (+184) -> SetAboveGroundTestResult(position @+0x40, normal @+0x90,
//     tag>>16, tag&0xFFFF) -- the down-ray latch (`lwz r11,0xB0(r30) ; srwi r4,r11,16 ;
//     clrlwi r5,r11,16` at 0x8262D528..0x8262D534).
//   * advance the cursor ONE record, unconditionally -- the stream holds one record per COMMAND
//     and one command was posted per live vehicle, hit or miss.
//
// The doubled type predicate (assert / re-read / assert / assert, .cpp:1501 + h:382 + h:391) is the
// same inlining artifact ReadUpdatedBodies carries, and is kept for the same reason: folding it
// away removes shipped asserts.
// =================================================================================================
void PhysicalTrafficManager::ReadTrafficTractionLineTestResults(
        CgsMemory::SimpleDataStreamResultIterator* lpResultIterator)
{
    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle != TotalPhysicalTrafficBitArray::KI_INVALID_BITINDEX;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(liVehicle >= 0, "liVehicle >= 0");                                  // h:740
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // h:741

        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[liVehicle];

        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "lpTrafficVehicle->IsFullyPhysical()");                             // .cpp:1501
        CGS_ASSERT(lrVehicle.mu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                          // h:382
        CGS_ASSERT(lrVehicle.mu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL,
                   "IsFullyPhysical()");                                               // h:391

        SimpleVehiclePhysics* const lpBody = lrVehicle.mpVehicleBody;

        const TrafficTractionLineResultRecord* const lpResult =
            static_cast<const TrafficTractionLineResultRecord*>(lpResultIterator->GetCurrent());

        for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
        {
            if (lpResult->mau8HitFlags[liWheel] == 0)
            {
                continue;
            }

            const f32* const lpfPosition = lpResult->mafHitPosition[liWheel];
            const f32* const lpfNormal   = lpResult->mafHitNormal[liWheel];
            const Vector3 lvPosition = { lpfPosition[0], lpfPosition[1],
                                         lpfPosition[2], lpfPosition[3] };
            const Vector3 lvNormal   = { lpfNormal[0],   lpfNormal[1],
                                         lpfNormal[2],   lpfNormal[3] };

            lpBody->AddTractionPoint(static_cast<EVehicleDrivenWheel>(liWheel),
                                     lvPosition, lvNormal,
                                     lpResult->mauSurfaceTag[liWheel]);
        }

        const s32 liGroundLine = KI_TRAFFIC_LINES_PER_COMMAND - 1;   // == 4
        if (lpResult->mau8HitFlags[liGroundLine] != 0)
        {
            const f32* const lpfPosition = lpResult->mafHitPosition[liGroundLine];
            const f32* const lpfNormal   = lpResult->mafHitNormal[liGroundLine];
            const Vector3 lvPosition = { lpfPosition[0], lpfPosition[1],
                                         lpfPosition[2], lpfPosition[3] };
            const Vector3 lvNormal   = { lpfNormal[0],   lpfNormal[1],
                                         lpfNormal[2],   lpfNormal[3] };

            const u32 luTag = lpResult->mauSurfaceTag[liGroundLine];
            lpBody->SetAboveGroundTestResult(lvPosition, lvNormal,
                                             static_cast<u16>(luTag >> 16),
                                             static_cast<u16>(luTag & 0xFFFFu));
        }

        lpResultIterator->GetNext();
    }
}
}
}
