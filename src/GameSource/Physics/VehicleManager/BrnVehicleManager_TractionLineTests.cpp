// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_TractionLineTests.cpp
//
// ⭐⭐ THE GROUND -- the producer lifecycle and the race-car harvest of the traction-line chain.
//   VehicleManager::DoVehicleTractionLineAllocations    @0x825B5098  (52 insns)
//   VehicleManager::RunTractionLineTestJobs             @0x825B5168  (64 insns)
//   VehicleManager::DoVehicleTractionLineDecallocations @0x825B5268  (37 insns)
//   VehicleManager::ReadRaceCarTractionLineTestResults  @0x82618058  (231 insns)
//
// A Burnout car does not rest on CONTACTS -- contacts are the body-shell/crash path. It rests on
// TRACTION LINE TESTS: one downward line per wheel, tested against the cached world triangles, and
// the hit is fed to RaceCarPhysics::AddTractionPoint -> SimpleVehiclePhysics::AddTractionPoint ->
// Wheel::SetRoadContact, which is what raises Wheel::mRoadContact.mbIsOnGround -- which is the
// thing the already-landed UpdateSuspensionSprings pushes against. With this gated, the landed
// ReadUpdatedBodies gravity step (`mLinearVelocity.y -= KF_GRAVITY*dt`) has nothing opposing it.
//
// ⚠️⚠️ NOTHING IN THIS FILE RUNS TODAY, DELIBERATELY, AND THAT IS THE HONEST STATE.
// The only callers are StartVehicleTractionLineTests (still a conductor gate) and
// EndVehicleTractionLineTests (still a link stub). Both stay gated because the GENERATION half of
// the chain is blocked on machinery that is not in this tree, measured this wave:
//   * AddRaceCarTractionLineTests @0x825E9640 and PhysicalTrafficManager::AddTrafficTraction-
//     LineTests @0x8261D580 build each command around a TRIANGLE LIST taken from a per-object
//     triangle cache -- they assert "lpCacheInterface != NULL" / "mpTriangleCacheManager != NULL" /
//     "mpaTriangleCache != NULL" and then dereference it. CgsSceneManager::TriangleCacheManager is
//     absent here (~1,688 console insns), as is SimpleVehiclePhysics::GetTractionLine @0x825D85C0.
//   * the tests themselves run in ContactGeneratorJob::ExecuteLineWithTriangleListStream
//     @0x82921968 (589) over CgsGeometric::IntersectLinePolygonSoup* -- also absent.
// So these four are mounted for LINK CLOSURE, not for effect: the mount proves every symbol they
// reach resolves, which is the one class of defect no per-TU compile gate can catch.
// ⛔ They are NOT wired into the conductor and MUST NOT be until the generation half lands --
// EndVehicleTractionLineTests dereferences mpTractionLineStreamProducer with no null guard, and
// UpdateVehiclePhysics reaches it unconditionally every frame.
//
// ⚠️ AND THE HARVEST MUST NOT RUN AHEAD OF THE JOB EITHER. ReadRaceCarTractionLineTestResults
// walks the LIVE-CAR bitset, not a result count -- it trusts that AddRaceCarTractionLineTests
// posted exactly one command per live car, and the iterator's own bound is the parent's
// miNumAddedCommands. Running it with the generation half gated would read whatever the collision
// arena last held and hand it to AddTractionPoint as a road surface: a fabricated ground of exactly
// the silent-drop class this project keeps paying for. Hence: land it, mount it, leave it unwired.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // EVehicleDrivenWheel

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"
#include "SharedClasses/World/BrnCollisionTag.h"   // KU_COLLISION_MASK_SURFACE_ID (the assert bound)
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor

#include <cstddef>                                                 // offsetof (the layout gates)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // Console literal `li r4, 0x88` at 0x825B512C -- the stream's command capacity.
    // ⚠ FLAG: 136 does NOT decompose cleanly into this tree's known quotas (8 race cars +
    // KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC == 20 + the player stuck-in-collision tests do not sum to
    // it), so it is carried as the shipped literal under a descriptive name rather than
    // reconstructed from parts. Do not "derive" it; if a future wave finds the real expression in
    // the console header, replace this and say so.
    const s32 KI_MAX_TRACTION_LINE_STREAM_COMMANDS = 136;

    // dword_82F2A10C -- the assert bound in ReadRaceCarTractionLineTestResults' baked message
    // "static_cast<int32_t>( lu8SurfaceId ) < KI_NUM_USED_SURFACES". Read straight out of the X360
    // image with x360rd (0x82F2A10C == 20); it sits statically initialised among live .data, i.e.
    // it is a constant the compiler had to give an address to, not a runtime counter (a runtime
    // counter would live zeroed in .bss). Consistent with BrnWorld::KU_MAX_SURFACE_ID == 63: 20 of
    // the 64 encodable surface ids are in use.
    const s32 KI_NUM_USED_SURFACES = 20;

    // ---------------------------------------------------------------------------------------
    // The 192-byte line-test RESULT record, laid out from the seats
    // ReadRaceCarTractionLineTestResults reads at 0x8261817C..0x826181B0. The console's own type
    // name for this record is NOT recoverable from the image (the stream is untyped on both sides),
    // so this is a FILE-LOCAL descriptive model, not a claim about a shipped identifier:
    //     lbz  r11, 0xB4(result + wheel)      -> +180 + wheel   u8   per-wheel hit flag
    //     lvx128 v1, 16*wheel (result)        -> +0   + 16*w    16B  hit position
    //     lvx128 v2, 16*(wheel+5) (result)    -> +80  + 16*w    16B  hit normal
    //     lwzx r5, 4*(wheel+40) (result)      -> +160 + 4*w     u32  surface / collision tag
    // The 16 bytes at +64 and the tail past +184 are never read by this harvest and are left as
    // named padding rather than guessed at. sizeof is gated below against the 192 the producer is
    // constructed with.
    // ---------------------------------------------------------------------------------------
    struct TractionLineTestResultRecord
    {
        f32 mafHitPosition[BrnPhysics::Vehicle::eNumDrivenWheels][4];   // +0x00
        f32 mafUnread40[4];                                             // +0x40 (not read here)
        f32 mafHitNormal[BrnPhysics::Vehicle::eNumDrivenWheels][4];     // +0x50
        f32 mafUnread90[4];                                             // +0x90 (not read here)
        u32 mauSurfaceTag[BrnPhysics::Vehicle::eNumDrivenWheels];       // +0xA0
        u8  mau8UnreadB0[4];                                            // +0xB0 (not read here)
        u8  mau8HitFlags[BrnPhysics::Vehicle::eNumDrivenWheels];        // +0xB4
        u8  mau8UnreadTail[8];                                          // +0xB8 (not read here)
    };
}

    // =============================================================================================
    // VehicleManager::DoVehicleTractionLineAllocations  @0x825B5098
    // Three preconditions then one factory call. The console's `sub_82810B98` is
    // BaseCollisionGenerator::CreateLineWithTriangleListStream (identified in
    // CgsCollisionGenerator_LineStream.cpp, not guessed).
    // =============================================================================================
    void VehicleManager::DoVehicleTractionLineAllocations(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen)
    {
        CGS_ASSERT(lpInputBufferStack != nullptr, "lpInputBufferStack != NULL");        // :1949
        CGS_ASSERT(lpTractionContactGen != nullptr, "lpTractionContactGen != NULL");    // :1950
        CGS_ASSERT(mpTractionLineStreamProducer == nullptr,
                   "mpTractionLineStreamProducer == NULL");                             // :1951

        mpTractionLineStreamProducer =
            lpTractionContactGen->CreateLineWithTriangleListStream(
                KI_MAX_TRACTION_LINE_STREAM_COMMANDS);

        CGS_ASSERT(mpTractionLineStreamProducer != nullptr,
                   "mpTractionLineStreamProducer != NULL");                             // :1961
    }

    // =============================================================================================
    // VehicleManager::RunTractionLineTestJobs  @0x825B5168
    // ⭐ The big straight-line store block at 0x825B51E4..0x825B5224 is NOT open-coded: it is
    // SimpleDataStreamProducer::Begin INLINED -- the six geometry copies (private +0x20..+0x34 ->
    // mShared +0x00..+0x14), the shared poster pointer (+0x18 <- producer+0x80), mbIsStreaming
    // (+0x100 <- 1), then DataStreamCommandPoster::Begin. That method is already reconstructed in
    // this tree (CgsSimpleDataStreamProducer_Begin.cpp, from the three inline copies inside
    // StartVehicleContactGeneration), and the store set here matches it field for field, so it is
    // called by name rather than re-emitted.
    // =============================================================================================
    void VehicleManager::RunTractionLineTestJobs(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen)
    {
        CGS_ASSERT(lpTractionContactGen != nullptr, "lpTractionContactGen != NULL");    // :2144
        CGS_ASSERT(mpTractionLineStreamProducer != nullptr,
                   "mpTractionLineStreamProducer != NULL");                             // :2145

        CgsDev::PerfMonCpu::StartMonitor(gs_iLineTestsBeginPM);
        mpTractionLineStreamProducer->Begin();
        CgsDev::PerfMonCpu::StopMonitor(gs_iLineTestsBeginPM);

        CgsDev::PerfMonCpu::StartMonitor(gs_iLineTestsRunStreamPM);
        // ⚠ AS SHIPPED: the returned job is stored unconditionally, null included. That null IS the
        // console's "nothing to dispatch" answer, and EndVehicleTractionLineTests' `if (job)` guard
        // is written for it.
        mpTractionLineTestsJob =
            lpTractionContactGen->RunLineWithTriangleListStream(mpTractionLineStreamProducer);
        CgsDev::PerfMonCpu::StopMonitor(gs_iLineTestsRunStreamPM);
    }

    // =============================================================================================
    // VehicleManager::DoVehicleTractionLineDecallocations  @0x825B5268
    // Despite the name there is no free: the producer and its two buffers are carved out of the
    // collision generator's own bump arena, which BaseCollisionGenerator::Finish resets wholesale
    // each frame. This releases the SEAT (`li r11, 0 ; stw r11, 0(this+172584)`), nothing else.
    // =============================================================================================
    void VehicleManager::DoVehicleTractionLineDecallocations(
            CgsModule::IOBufferStack* lpInputBufferStack)
    {
        CGS_ASSERT(lpInputBufferStack != nullptr, "lpInputBufferStack != NULL");        // :2296
        CGS_ASSERT(mpTractionLineStreamProducer != nullptr,
                   "mpTractionLineStreamProducer != NULL");                             // :2297

        mpTractionLineStreamProducer = nullptr;
    }

    // =============================================================================================
    // VehicleManager::ReadRaceCarTractionLineTestResults  @0x82618058   ⭐ THE PAYOFF
    //
    // The Hex-Rays rendering of this function is a maze because the whole BitArray<8> walk is
    // inlined (the cntlzd/lowest-set-bit dance at 0x826180E4 and again at 0x826183C0, plus the
    // 64-bit word stepping and the CgsBitArray.h:203 "invalid index" assert). Read as ASM it is a
    // plain GetFirstNonZeroBit/GetNextNonZeroBit walk over mUsedRaceCars, which is exactly how the
    // already-landed UpdateVehiclePhysics writes the same inline -- so it is written that way here.
    //
    // Per live car: read the current 192-byte result record; for each of the four wheels whose hit
    // flag is set, hand {position, normal, surface tag} to RaceCarPhysics::AddTractionPoint; then
    // advance the shared cursor one record (the console's `++miResultIndex; GetCurrent()` pair at
    // 0x82618204..0x82618210 == SimpleDataStreamResultIterator::GetNext, whose value it discards).
    //
    // The console loads position/normal with full 16-byte `lvx128`; this tree's Vector3 is the
    // same four-lane 16-byte register, so all four lanes are carried (no narrowing).
    // =============================================================================================
    void VehicleManager::ReadRaceCarTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator* lpResultIterator)
    {
        CGS_ASSERT(mpTractionLineStreamProducer != nullptr,
                   "mpTractionLineStreamProducer != NULL");                             // :2175

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            const TractionLineTestResultRecord* const lpResult =
                static_cast<const TractionLineTestResultRecord*>(lpResultIterator->GetCurrent());

            RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];

            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                if (lpResult->mau8HitFlags[liWheel] == 0)
                    continue;

                const EVehicleDrivenWheel leWheel = static_cast<EVehicleDrivenWheel>(liWheel);

                // Both are full 16-byte `lvx128` loads on the console, and this tree's Vector3 is
                // the same 16-byte four-lane register, so the fourth lane is carried through
                // rather than dropped -- the value handed to AddTractionPoint is bit-identical to
                // the console's v1/v2.
                const f32* const lpfPosition = lpResult->mafHitPosition[liWheel];
                const f32* const lpfNormal   = lpResult->mafHitNormal[liWheel];
                const Vector3 lvPosition = { lpfPosition[0], lpfPosition[1],
                                             lpfPosition[2], lpfPosition[3] };
                const Vector3 lvNormal   = { lpfNormal[0],   lpfNormal[1],
                                             lpfNormal[2],   lpfNormal[3] };

                lrCar.AddTractionPoint(leWheel, lvPosition, lvNormal,
                                       lpResult->mauSurfaceTag[liWheel]);

                // The console re-reads the surface id back out of the wheel AddTractionPoint just
                // wrote -- it validates what was STORED, not what was passed:
                //     lhz r11, 0x156(wheel) ; srwi r11, r11, 4 ; clrlwi r10, r11, 26
                // 0x156 is the SECOND halfword of mRoadContact.mCollisionTag, i.e. the material
                // tag, and >>4 & 0x3F is exactly BrnWorld::KU_COLLISION_MASK_SURFACE_ID (0x3F0).
                // ⚠️ FLAG -- A REAL TYPE FORK, not a shortcut here. Wheel::RoadContact::
                // mCollisionTag is the ONE-FIELD PLACEHOLDER `::CollisionTag { u32 muValue; }` from
                // BrnCommonTypes.h:29, NOT BrnWorld::CollisionTag {u16 mu16GroupTag; u16
                // mu16MaterialTag;} (BrnCollisionTag.h:48) which owns GetSurfaceId and the mask
                // constants. Same 4 bytes, different shape, and the placeholder is what every
                // RoadContact in the tree carries. The mask is applied to the u32 directly, which
                // is equivalent because the surface-id field lives inside the material halfword
                // (the console's own `lwzx r5` hands this harvest the whole 32-bit tag anyway).
                // Retiring the fork means re-typing RoadContact::mCollisionTag across the vehicle
                // headers -- its own change, flagged here rather than smuggled in.
                const u32 luStoredTag = lrCar.GetWheel(leWheel).mRoadContact.mCollisionTag.muValue;
                const s32 liSurfaceId =
                    static_cast<s32>((luStoredTag & BrnWorld::KU_COLLISION_MASK_SURFACE_ID) >> 4);
                CGS_ASSERT(liSurfaceId < KI_NUM_USED_SURFACES,
                           "static_cast<int32_t>( lu8SurfaceId ) < KI_NUM_USED_SURFACES");
            }

            // AS SHIPPED: the advance happens even for a car whose four wheels all missed, because
            // the stream holds one record per COMMAND and one command was posted per live car.
            lpResultIterator->GetNext();
        }
    }

    // Layout gate for the result record above: the producer is constructed with a 192-byte result
    // stride (CgsCollisionGenerator_LineStream.cpp), and every seat this harvest reads is derived
    // from that record, so a silent size change here would silently mis-seat every wheel.
    static_assert(sizeof(TractionLineTestResultRecord) == 192,
                  "the line-test result stride is the 0xC0 the console constructs the stream with");
    static_assert(offsetof(TractionLineTestResultRecord, mafHitNormal) == 0x50,
                  "hit normals sit at 16*(wheel+5) -- lvx128 v2, r10, r29 @0x826181A8");
    static_assert(offsetof(TractionLineTestResultRecord, mauSurfaceTag) == 0xA0,
                  "surface tags sit at 4*(wheel+40) -- lwzx r5, r9, r29 @0x826181AC");
    static_assert(offsetof(TractionLineTestResultRecord, mau8HitFlags) == 0xB4,
                  "hit flags sit at 180+wheel -- lbz r11, 0xB4(r11) @0x82618170");
}
}
