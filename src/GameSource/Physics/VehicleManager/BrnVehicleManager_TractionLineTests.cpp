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
// ⭐⭐⭐ SUPERSEDED 2026-08-11 (lifetime wave) -- THE PRODUCER LIFETIME IS WHOLE AND IT RUNS.
// StartVehicleTractionLineTests @0x82629CE0 (conductor gate DELETED),
// AddRaceCarTractionLineTests @0x825E9640, EndVehicleTractionLineTests @0x82633CD8 (link stub
// DELETED) and SimpleVehiclePhysics::GetTractionLine @0x825D85C0 (export hole, lifted from the
// image + the PS3 export; body in BrnSimpleVehiclePhysics.cpp) all landed in ONE commit, which is
// the only way this leg can land: End dereferences mpTractionLineStreamProducer with no null
// guard and UpdateVehiclePhysics reaches it unconditionally every frame.
// The two SIDE legs -- traffic (AddTraffic 418 <-> ReadTraffic 291) and player-stuck
// (AddPlayerStuck 171 + UpdatePlayerStuckTest 87 + UpdatePlayerStuckSpheres 147 <-> ReadPlayerStuck
// 118) -- are named gates in BrnPhysicsConductorGates.cpp, gated in MATCHED Add<->Read PAIRS
// because all three harvests share one result cursor.
//
// ⚠️ WHAT IT PRODUCES TODAY IS ZERO COMMANDS, AND THAT IS THE CORRECT ANSWER, NOT A STUB.
// AddRaceCarTractionLineTests walks mUsedRaceCars, and the ONLY write to that bitset in this tree
// is BrnVehicleManager_Construct.cpp's UnSetAll -- its setter, VehicleManager::ProcessCreateEvents
// @0x82616770, is still a named gate. RUNTIME-WITNESSED this wave, with the lifetime live:
//     PROBE lt start liveRaceCars=0 commandsPosted=0 producer=1 job=0 sizeofCmd=176 cacheIface=1
// i.e. every frame opens a REAL producer, posts nothing, dispatches nothing (job=0 is the
// console's own "nothing to dispatch" null, and End's `if (job)` guard takes it), harvests
// nothing and releases the seat. The first car the create path adds starts producing traction
// lines with no further work in this file.
//
// ---- the paragraph below is the pre-2026-08-11 state, kept for the reasoning it records ----
// ⚠️⚠️ NOTHING IN THIS FILE RUNS TODAY, DELIBERATELY, AND THAT IS THE HONEST STATE.
// The only callers are StartVehicleTractionLineTests (still a conductor gate) and
// EndVehicleTractionLineTests (still a link stub). Both stay gated because the GENERATION half of
// the chain is blocked on machinery that is not in this tree, measured this wave:
//   * AddRaceCarTractionLineTests @0x825E9640 and PhysicalTrafficManager::AddTrafficTraction-
//     LineTests @0x8261D580 build each command around a TRIANGLE LIST taken from a per-object
//     triangle cache -- they assert "lpCacheInterface != NULL" / "mpTriangleCacheManager != NULL" /
//     "mpaTriangleCache != NULL" and then dereference it.
//     ⚠️ CORRECTED 2026-08-10: "CgsSceneManager::TriangleCacheManager is absent here (~1,688
//     console insns)" was wrong. The manager is present and mounted; its READ side (Prepare /
//     GetTrianglesForCachedObject / the TriangleCacheInterface accessors, 178 insns) was already
//     bodied and its slot bookkeeping (901 insns) landed 2026-08-10. What is missing is the FILL
//     half -- StartUpdateTriangleCaches 278 + EndUpdateTriangleCaches 475 + the module's
//     StartUpdateTriangleCache 73 (all WorldLinkStubs gates on an ALREADY-LIVE per-frame path),
//     VehicleManager::UpdateTriangleCache 240 / PrepareTriangleCache 37, and the
//     PolygonSoupTesterJob fill path -- so the cache is allocated but empty.
//     SimpleVehiclePhysics::GetTractionLine @0x825D85C0 is still absent: a genuine export hole of
//     **174 instructions** (dir gap 0x825D8490+76 -> 0x825D8878; no hit in 30,084 exports).
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
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"                // the traffic pair
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"        // GetTriangleCacheInterface
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // EVehicleDrivenWheel

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithTriangleListStreamJobDesc.h"  // StreamCommand
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"   // CheckAlignment
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"        // TriangleCacheInterface
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                   // Triangle4::AssertIsValid
#include "SDKs/EATech/eajobs/job.h"                                                     // EA::Jobs::Job::WaitOn
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"
#include "SharedClasses/World/BrnCollisionTag.h"   // KU_COLLISION_MASK_SURFACE_ID (the assert bound)
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // gpDebugPrint ([traction] probe)

#include <cstddef>                                                 // offsetof (the layout gates)
#include <cstdlib>                                                 // getenv/atoi ([traction] opt-in)
#include <cmath>                                                   // sqrtf ([traction] line length)

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

    // -----------------------------------------------------------------------------------------
    // ---- [traction] PC bring-up instrument -- DELETE WHEN world collision is proven map-wide --
    //
    // OPT-IN (BRN_TRACTION_PROBE=<period in frames>) exactly like [tricache] in
    // CgsTriangleCacheManager_Update.cpp: the latch reads 0 on its first call in a default run and
    // every print below is unreachable thereafter, so a default run and every golden gate stay
    // byte-identical to a build without it.
    //
    // ⭐ WHY THIS INSTRUMENT. The [tricache] leg proved the cache is INNOCENT -- it tracks the car
    // and holds 8-10 batches through the frames where the car sinks. The one link it could not see
    // is the LINE ITSELF: a batch count says triangles were offered, not that a wheel's probe
    // segment ever reached them. This prints, per wheel, the two endpoints
    // SimpleVehiclePhysics::GetTractionLine @0x825D85C0 produced, the segment length, and the hit
    // flag + hit height the job wrote back -- i.e. the question "did the probe reach ground" is
    // answered by the probe's own geometry rather than inferred from a downstream number.
    //
    // ⚠️ THE HALVES ARE CAPTURED IN DIFFERENT FUNCTIONS AND PRINTED ONCE. The line geometry only
    // exists in AddRaceCarTractionLineTests (Start half) and the results only in
    // ReadRaceCarTractionLineTestResults (End half); both walk mUsedRaceCars in the same order in
    // the same frame, so Add stashes into the array below and Read prints the pair. Nothing here
    // is console state -- `len` is derived for the probe.
    //
    // ⛔ IT PRINTS THE CAR INDEX ON EVERY LINE, AND EVERY LIVE CAR. The `[move-probe]` disaster
    // this campaign already paid for twice was a probe that truthfully printed a PARKED car while
    // the player drove 165 m away; cars 0 and 1 sit at the junkyard all session and the local
    // player is car 2. Naming the index on each line is what makes that visible instead of silent.
    // -----------------------------------------------------------------------------------------
    const s32 KI_TRACTION_PROBE_MAX_CARS = 8;   // == BitArray<8u>, mUsedRaceCars' width

    struct TractionProbeCarRecord
    {
        s32 miBatches;
        f32 mafStart[BrnPhysics::Vehicle::eNumDrivenWheels][3];
        f32 mafEnd[BrnPhysics::Vehicle::eNumDrivenWheels][3];
    };

    TractionProbeCarRecord s_aTractionProbe[KI_TRACTION_PROBE_MAX_CARS] = {};
    u32  s_uTractionProbeFrame  = 0u;
    bool s_bTractionProbeSample = false;   // this frame is a sample frame (set in Add, read in Read)

    bool TractionProbeArmed()
    {
        static s32 siArmed = -1;
        if (siArmed < 0)
        {
            const char* lpcEnv = getenv("BRN_TRACTION_PROBE");
            siArmed = (lpcEnv != NULL && lpcEnv[0] != '0') ? 1 : 0;
        }
        return (siArmed == 1) && (CgsDev::Log::gpDebugPrint != NULL);
    }

    u32 TractionProbePeriod()
    {
        static u32 suPeriod = 0u;
        if (suPeriod == 0u)
        {
            suPeriod = 60u;
            const char* lpcEnv = getenv("BRN_TRACTION_PROBE");
            if (lpcEnv != NULL)
            {
                const int liValue = atoi(lpcEnv);
                if (liValue > 1)
                {
                    suPeriod = static_cast<u32>(liValue);
                }
            }
        }
        return suPeriod;
    }
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

            // ---- [traction] the pair: the line Add posted, and what came back for it ----------
            if (s_bTractionProbeSample && liCar >= 0 && liCar < KI_TRACTION_PROBE_MAX_CARS)
            {
                const TractionProbeCarRecord& lrRec = s_aTractionProbe[liCar];
                for (s32 liW = 0; liW < eNumDrivenWheels; ++liW)
                {
                    const f32 lfDx = lrRec.mafEnd[liW][0] - lrRec.mafStart[liW][0];
                    const f32 lfDy = lrRec.mafEnd[liW][1] - lrRec.mafStart[liW][1];
                    const f32 lfDz = lrRec.mafEnd[liW][2] - lrRec.mafStart[liW][2];
                    const f32 lfLen = sqrtf(lfDx * lfDx + lfDy * lfDy + lfDz * lfDz);
                    const bool lbHit = (lpResult->mau8HitFlags[liW] != 0);
                    *CgsDev::Log::gpDebugPrint
                        << "[traction] n " << static_cast<s32>(s_uTractionProbeFrame)
                        << " car " << liCar << " w " << liW
                        << " batches " << lrRec.miBatches
                        << " start " << lrRec.mafStart[liW][0] << " " << lrRec.mafStart[liW][1]
                        << " " << lrRec.mafStart[liW][2]
                        << " end " << lrRec.mafEnd[liW][0] << " " << lrRec.mafEnd[liW][1]
                        << " " << lrRec.mafEnd[liW][2]
                        << " len " << lfLen
                        << (lbHit ? " HIT y " : " MISS y ")
                        << lpResult->mafHitPosition[liW][1]
                        << "\n";
                }
            }
            // ---- end [traction] ---------------------------------------------------------------

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

    // =============================================================================================
    // VehicleManager::AddRaceCarTractionLineTests  @0x825E9640  (313 insns)
    // ⭐⭐ THE GENERATION HALF. One 176-byte stream command per LIVE race car.
    //
    // The Hex-Rays rendering is dominated by the inlined BitArray<8> walk and its CgsBitArray.h:203
    // "invalid index" StrStreamBase assert; read as ASM (0x825E96F8 onward) it is the same plain
    // GetFirstNonZeroBit/GetNextNonZeroBit walk over mUsedRaceCars that the harvest below uses, so
    // it is written that way here.
    //
    // Per live car the console does exactly four things, and all four are BY NAME here:
    //   1. 0x825E97EC..0x825E9838  the cache window. `lwz r11,4(mgr) ; +48*car ; lwz r29,0x24(r11)`
    //      then `lwz r10,0(mgr) ; mulli 0xE0 ; add` -- i.e. the slot's miIndexIntoTriangleCache
    //      into the shared Triangle4 array. That pair IS TriangleCacheInterface::GetCache(car),
    //      inlined (it even carries GetCache's own two asserts, CgsSceneManagerModuleIO.h:1286 and
    //      CgsCachedTriangleList.h:153), so it is CALLED here rather than re-open-coded.
    //      ⚠️ NOTE FOR THE LEDGER: this is why TriangleCacheInterface::GetCache @0x82277810 is NOT
    //      part of this leg's closure -- its four console callers are the player-stuck leg, the
    //      effects module and the two deformation contact generators, none of them this one.
    //   2. 0x825E9878  allocate the command off mpTractionLineStreamProducer, seat mpTriangles +
    //      miNumTriangleBatches (the slot's miNumCachedTriangleBatches, +0x28), then the shipped
    //      TriangleList::CheckAlignment + per-batch Triangle4::AssertIsValid sweep.
    //   3. 0x825E98E4..0x825E9928  four wheels: GetTractionLine(wheel, start, end) straight into
    //      maLineStart[w] / maLineEnd[w]. The stride between the two arrays is 16*(w+5), which is
    //      the fifth witness that a command carries FIVE line slots.
    //   4. 0x825E9948  miNumLines = 4. (AddTrafficTractionLineTests writes 5.)
    //
    // ⚠️ AS SHIPPED, THE RETURN IS A COMMAND COUNT, NOT A LINE COUNT: `HIDWORD(v57)` is bumped once
    // per CAR (0x825E993C..0x825E9944), and StartVehicleTractionLineTests adds it to
    // miNumSPUTractionLineTests. Four lines per car are invisible to that total.
    // =============================================================================================
    s32 VehicleManager::AddRaceCarTractionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator* lpTractionContactGen,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpCacheInterface)
    {
        CGS_ASSERT(lpTractionContactGen != nullptr, "lpTractionContactGen != NULL");     // :1982
        CGS_ASSERT(lpCacheInterface != nullptr, "lpCacheInterface != NULL");             // :1983
        CGS_ASSERT(mpTractionLineStreamProducer != nullptr,
                   "mpTractionLineStreamProducer != NULL");                              // :1984

        s32 liNumCommands = 0;

        // ---- [traction] one sample decision per frame; Add is the frame's first half ----------
        ++s_uTractionProbeFrame;
        s_bTractionProbeSample =
            ((s_uTractionProbeFrame % TractionProbePeriod()) == 0u) && TractionProbeArmed();
        // ---- end [traction] -------------------------------------------------------------------

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            // 1. this car's window into the shared triangle cache.
            const CgsGeometric::Triangle4* const lpTriangles = lpCacheInterface->GetCache(liCar);
            const s32 liNumBatches = lpCacheInterface->GetNumCachedTriangleBatches(liCar);

            // 2. one command, seated with that window.
            CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc::StreamCommand*
                lpCommand = nullptr;
            mpTractionLineStreamProducer->AllocateCommand(
                reinterpret_cast<void**>(&lpCommand));

            lpCommand->mpTriangles          = lpTriangles;
            lpCommand->miNumTriangleBatches = liNumBatches;

            // The console passes &cmd->mpTriangles -- the {const Triangle4*, s32} pair at +0xA0 IS
            // a TriangleList, which is why CheckAlignment takes it unchanged.
            CgsSceneManager::CgsCollision::TriangleList* const lpTriangleList =
                reinterpret_cast<CgsSceneManager::CgsCollision::TriangleList*>(
                    &lpCommand->mpTriangles);
            lpTriangleList->CheckAlignment();

            for (s32 liBatch = 0; liBatch < lpCommand->miNumTriangleBatches; ++liBatch)
            {
                lpCommand->mpTriangles[liBatch].AssertIsValid();
            }

            // 3. four wheels, four probes.
            const RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];
            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
            {
                Vector3 lvLineStart;
                Vector3 lvLineEnd;
                lrCar.GetTractionLine(static_cast<EVehicleDrivenWheel>(liWheel),
                                      lvLineStart, lvLineEnd);
                // ⚠ The command's line arrays are Vector4 and GetTractionLine hands back
                // Vector3; both are the same 16-byte four-lane register (rw/math/vpu/types.h),
                // but they are distinct C++ types here, so all four lanes are copied explicitly
                // rather than reinterpret_cast -- the console's `lvx128`/`stvx128` pair moves the
                // whole register and the w lane must travel with it.
                lpCommand->maLineStart[liWheel].x = lvLineStart.x;
                lpCommand->maLineStart[liWheel].y = lvLineStart.y;
                lpCommand->maLineStart[liWheel].z = lvLineStart.z;
                lpCommand->maLineStart[liWheel].w = lvLineStart.w;
                lpCommand->maLineEnd[liWheel].x   = lvLineEnd.x;
                lpCommand->maLineEnd[liWheel].y   = lvLineEnd.y;
                lpCommand->maLineEnd[liWheel].z   = lvLineEnd.z;
                lpCommand->maLineEnd[liWheel].w   = lvLineEnd.w;
            }

            // ---- [traction] stash this car's half; printed by the harvest in End --------------
            if (s_bTractionProbeSample && liCar >= 0 && liCar < KI_TRACTION_PROBE_MAX_CARS)
            {
                TractionProbeCarRecord& lrRec = s_aTractionProbe[liCar];
                lrRec.miBatches = liNumBatches;
                for (s32 liW = 0; liW < eNumDrivenWheels; ++liW)
                {
                    lrRec.mafStart[liW][0] = lpCommand->maLineStart[liW].x;
                    lrRec.mafStart[liW][1] = lpCommand->maLineStart[liW].y;
                    lrRec.mafStart[liW][2] = lpCommand->maLineStart[liW].z;
                    lrRec.mafEnd[liW][0]   = lpCommand->maLineEnd[liW].x;
                    lrRec.mafEnd[liW][1]   = lpCommand->maLineEnd[liW].y;
                    lrRec.mafEnd[liW][2]   = lpCommand->maLineEnd[liW].z;
                }
            }
            // ---- end [traction] ---------------------------------------------------------------

            // 4. AS SHIPPED: a race car posts FOUR of the command's five line slots.
            lpCommand->miNumLines = eNumDrivenWheels;

            ++liNumCommands;
        }

        return liNumCommands;
    }

    // =============================================================================================
    // VehicleManager::StartVehicleTractionLineTests  @0x82629CE0  (78 insns)
    // ⭐⭐ THE PRODUCER LIFETIME, OPEN. Its conductor gate is DELETED as of 2026-08-11.
    //
    // ⛔⛔ THIS AND EndVehicleTractionLineTests MUST LAND TOGETHER AND CAN NEVER BE SPLIT. Start
    // allocates mpTractionLineStreamProducer; End dereferences it with NO null guard
    // (`lwz r29, 0(this+172584) ; addi r3, r29, 0x80 ; bl DataStreamCommandPoster::End`) and
    // UpdateVehiclePhysics reaches End unconditionally every frame. Three earlier waves refused a
    // partial here and all three were right.
    //
    // ⚠️ WHAT RUNS TODAY, HONESTLY: the lifetime is real and executes every frame, but
    // AddRaceCarTractionLineTests walks mUsedRaceCars and its setter (ProcessCreateEvents
    // @0x82616770) is still a named gate, so the frame opens a producer, posts ZERO commands,
    // dispatches, waits and harvests zero records. That is the CORRECT empty answer, not a stub.
    //
    // The five PerfMon handles are the console's own file-scope slots (dword_82F2A158 /
    // dword_82F2A15C here, three more in End), hoisted to external linkage this wave per
    // BrnVehicleManagerPerfMonHandles.h's additive rule.
    // =============================================================================================
    void VehicleManager::StartVehicleTractionLineTests(
            CgsModule::IOBufferStack* lpInputBufferStack,
            const VehicleInputInterface* lpInputInterface,
            Deformation::DeformationManager* lpDeformationManager,
            f32 lfTimeStep)
    {
        CGS_ASSERT(mpContactGenerator != nullptr, "mpContactGenerator != NULL");          // :2346

        // 0x82629CFC/0x82629D14: the counter is zeroed BEFORE the assert's fall-through, then the
        // three Add* returns are accumulated into it.
        miNumSPUTractionLineTests = 0;

        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* const lpCacheInterface =
            lpInputInterface->GetTriangleCacheInterface();

        CgsDev::PerfMonCpu::StartMonitor(gs_iTractionGetLinesPM);

        UpdatePlayerStuckInCollisionTest(lpCacheInterface, lfTimeStep);
        DoVehicleTractionLineAllocations(lpInputBufferStack, mpContactGenerator);

        miNumSPUTractionLineTests +=
            AddRaceCarTractionLineTests(mpContactGenerator, lpCacheInterface);
        miNumSPUTractionLineTests +=
            mPhysicalTrafficManager.AddTrafficTractionLineTests(
                mpContactGenerator, mpTractionLineStreamProducer, lpCacheInterface);
        miNumSPUTractionLineTests +=
            AddPlayerStuckInCollisionLineTests(mpContactGenerator, lpCacheInterface,
                                               lpDeformationManager);

        CgsDev::PerfMonCpu::StopMonitor(gs_iTractionGetLinesPM);

        CgsDev::PerfMonCpu::StartMonitor(gs_iTractionLineTestsPM);
        RunTractionLineTestJobs(mpContactGenerator);
        CgsDev::PerfMonCpu::StopMonitor(gs_iTractionLineTestsPM);
    }

    // =============================================================================================
    // VehicleManager::EndVehicleTractionLineTests  @0x82633CD8  (68 insns)
    // ⭐⭐ THE PRODUCER LIFETIME, CLOSED. Its link stub is DELETED as of 2026-08-11.
    //
    // Wait on the dispatched job (null-guarded -- RunTractionLineTestJobs stores the dispatcher's
    // answer unconditionally and a null IS "nothing was dispatched"), close the stream, then hand
    // ONE result cursor to the three harvests in turn so each resumes where the last stopped, and
    // finally release the producer seat.
    //
    // ⚠️ THE CURSOR IS COPIED BY VALUE ONCE (`ld r11, 0x38(producer) ; std r11, <stack>` at
    // 0x82633DA4) and the SAME stack copy is passed to all three -- the producer's own iterator is
    // never advanced. That is why the two gated harvests must stay paired with their gated
    // producers: a Read that runs without its Add walks the previous leg's records.
    //
    // ⚠️ AND THE STREAM IS CLOSED BEFORE ANY HARVEST (0x82633D7C, ahead of the three `bl`s), which
    // is what makes the result buffer stable while the cursor walks it.
    // =============================================================================================
    void VehicleManager::EndVehicleTractionLineTests(
            CgsModule::IOBufferStack* lpInputBufferStack,
            const VehicleInputInterface* /*lpInputInterface*/)
    {
        // ⚠️ The second parameter is genuinely UNREAD by the console body -- r5 is never touched
        // in any of the 68 instructions -- but the CALLER sets it (`mr r5, r29` @0x8264565C) and
        // the PS3 DWARF types it, so it is declared and deliberately unnamed here rather than
        // deleted. See BrnVehicleManager.h for the retraction of the 2026-08-10 "one parameter".
        CGS_ASSERT(mpContactGenerator != nullptr, "mpContactGenerator != NULL");          // :2387

        CgsDev::PerfMonCpu::StartMonitor(gs_iLineTestsFinishPM);
        if (mpTractionLineTestsJob != nullptr)
        {
            // 0x82633D44..0x82633D50: the console passes r4=0, r5=0, r6=-1 alongside `this`.
            // Those three are the X360 EA::Jobs spin-wait's own defaulted arguments; this tree's
            // job.h models WaitOn() with `this` only (job.h:94, X360 0x82BCB238), which is the
            // same entry the cache-fill wave already calls this way.
            mpTractionLineTestsJob->WaitOn();
            mpTractionLineTestsJob = nullptr;
        }
        CgsDev::PerfMonCpu::StopMonitor(gs_iLineTestsFinishPM);

        CgsDev::PerfMonCpu::StartMonitor(gs_iLineTestsEndPM);
        mpTractionLineStreamProducer->End();
        CgsDev::PerfMonCpu::StopMonitor(gs_iLineTestsEndPM);

        CgsDev::PerfMonCpu::StartMonitor(gs_iTractionProcessResultsPM);

        CgsMemory::SimpleDataStreamResultIterator lResultIterator =
            mpTractionLineStreamProducer->GetResultIterator();

        ReadRaceCarTractionLineTestResults(&lResultIterator);
        mPhysicalTrafficManager.ReadTrafficTractionLineTestResults(&lResultIterator);
        ReadPlayerStuckTractionLineTestResults(&lResultIterator);

        DoVehicleTractionLineDecallocations(lpInputBufferStack);

        CgsDev::PerfMonCpu::StopMonitor(gs_iTractionProcessResultsPM);
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
