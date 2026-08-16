// ============================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManagerContactGeneration.cpp
//
// BrnPhysics::Vehicle::VehicleManager -- the per-frame vehicle contact-generation driver
// (big-five #2 wave, 2026-08-06). Home TU per the body's OWN baked assert path
// ("...gamesource\unity\../Physics/VehicleManager/BrnVehicleManagerContactGeneration.cpp") and
// the DecFIGS dwarfdump TU listing.
//
// This slice: StartVehicleContactGeneration @0x8262AEE8 (~1228 insns) -- the second of the
// big-five pair. Reconstructed from the BURNOUT_X360_ARTIST.XEX asm with the PS3 DecFIGS
// out-of-line build @0x78A754 as the structural oracle (its mangle is the signature
// authority; it keeps the accessors the X360 inlines out-of-line).
//
// Caller: PhysicsModule::Update @0x825B0640 -- STILL A LINK STUB, so nothing reaches this
// body at runtime yet; /OPT:REF strips it. Mounted for closure enforcement.
//
// ⭐⭐ Callee state (2026-08-14 walls leg 1): DoRaceCarWorldContactGeneration @0x825EB140 is
// REAL (its log-once gate deleted) and the collide-stream family behind it is REAL
// (CgsCollisionGenerator_CollideStreams.cpp) — one sphere/swept command per live car per frame
// now flows into ContactGeneratorJob::Execute, where the type-6/14 workers are LOUD NAMED
// GATES until the sphere-vs-Triangle4 contact kernels land. Still not reconstructed:
//   DoCarCarContactGeneration @0x8261BB38 (250 asm / 15 callees; PS3 0x75C0C8) — log-once gate
//   DoTrafficCarWorldContactGeneration @0x8261BF28 (⚠ .ida-exports HOLE; PS3 0x789760) — trap,
//     dead at runtime (no live traffic on the junkyard path)
// ⭐⭐ 2026-08-14 (walls leg 3): EndVehicleContactGeneration @0x8261AC38 + AddContactResultsToQueue
// @0x825EB350 (THE HARVEST) + DoRaceCarWorldContactValidation @0x825EB6C8 are REAL in this TU;
// ValidateRaceCarWorldContact @0x825C6088 is REAL in the _ValidateRaceCarWorldContact slice.
// (IsRaceCarHidden @0x825C2EA0 is REAL since 2026-08-11 — see the note at the foot of this TU.)
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                               // IOBufferStack::CreateIOBuffer<T>
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"         // SimpleDataStreamProducer (Begin)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h"    // OutOverlapPair (promoted ids)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"          // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CollisionGenerator (+ the collide-stream family)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"       // SphereList (DoRaceCarWorld's sphere pass)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSweptSphereList.h"  // SweptSphereList (the swept pass)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"     // TriangleList (CheckAlignment/ValidateTriangles)
#include "GameSource/Physics/BrnContactGenerationList.h"                                  // ContactGenList
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                  // DeformationManager (Add*Pair, FindModelIndexByEntityID, GetDeformableObject)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (GetVehiclePhysics)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (IsFrozen through the ExternallySimulatedBody base)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                 // Start/StopMonitor (DoRaceCarWorldContactValidation's own monitor)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"  // CollisionResultList / PrimitiveTestResult (the harvest)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"              // PotentialContactInterface::AddEvent(u32, ...) + the queue accessors
#include "SDKs/EATech/eajobs/job.h"                                                       // EA::Jobs::Job::WaitOn (EndVehicleContactGeneration)

#include <cstdlib>                                                                        // getenv -- the [cvalid] probe's opt-in latch

namespace BrnPhysics
{
namespace Vehicle
{
    namespace
    {
        // Owner byte / 14-bit entity index of a packed entity word (the standard
        // CgsEntityId geometry the asm decodes everywhere in this body).
        inline u32 GetWordOwner(u32 luEntityWord) { return luEntityWord >> 24; }
        inline u32 GetWordIndex(u32 luEntityWord) { return (luEntityWord >> 10) & 0x3FFFu; }

        // Pair-list builder capacities baked into the driver (`li 600` at the five Prepare
        // calls; `cmplwi 0x12C` at the two overflow gates).
        const u16 KU16_PAIR_BUILDER_CAPACITY  = 600;
        const u16 KU16_MAX_PAIR_TESTS         = 300;   // 0x12C

        // The stream-producer command capacity (`li 100` at the three Create* calls) and the
        // CollidePrimitivePairList arguments (`li 200 ; li 11 ; li 0`).
        const s32 KI_STREAM_MAX_COMMANDS      = 100;
        const u16 KU16_COLLIDE_MAX_RESULTS    = 200;
        const u32 KU_COLLIDE_FLAGS            = 11;    // == E_ENTITYTYPE_PROP_COLLISION_RACECAR's value; carried as the baked literal
        const u16 KU16_COLLIDE_TAG            = 0;

        // ⭐ 2026-08-14 (walls leg 1): DoRaceCarWorldContactGeneration's two contact-padding
        // floats, image-read this wave (x360rd): flt_82001D9C == 2.0f rides the in-place sphere
        // pass, flt_82001DA0 == 0.5f the swept (continuous) pass. Both go to the poster's f1 and
        // land in StreamCommand::mfPadding.
        const f32 KF_SPHERE_TRIANGLE_CONTACT_PADDING       = 2.0f;   // flt_82001D9C
        const f32 KF_SWEPT_SPHERE_TRIANGLE_CONTACT_PADDING = 0.5f;   // flt_82001DA0
    }

    // ==========================================================================================
    // StartVehicleContactGeneration @ 0x8262AEE8   (PS3 DecFIGS 0x78A754 -- signature authority)
    //
    // Kick this frame's vehicle contact generation:
    //   1) Prepare the five primitive-pair builders (wheel, part, hinged, traffic-simple-traffic,
    //      racecar-simple-traffic -- the console's call order) with capacity 600 out of the
    //      per-frame linear allocator.
    //   2) Create the ContactGenList + CollisionGenerator IO buffers ("Contact Gen List" /
    //      "Contact Generator"), Construct the list (the console CreateIOBuffer runs
    //      T::Construct; the PC template placement-news only -- the InputBuffer precedent),
    //      Prepare the generator on its embedded 2 MB arena, clear mOverlappingRaceCars +
    //      miFirstPartContactGenEntry + miNumTrafficSphereWorldTests, and create the
    //      sphere-sphere collide stream.
    //   3) Walk the scene's overlap pairs:
    //        * car-vs-car pairs: rewrite traffic ids global->physical (unmapped == skip), skip
    //          same-articulated-train traffic pairs (both joints unbroken + same joint index),
    //          canonicalise the pair so the race car is side A (queue index 7/8/13 == exactly
    //          the custom-queue index the bridge drains), for racecar-racecar pairs mark the
    //          symmetric mOverlappingRaceCars bits and skip type-2/type-2 or hidden cars, then
    //          DoCarCarContactGeneration + AddHingedBodyPartPairs;
    //        * part-vs-car / wheel-vs-car pairs: overflow-gate the builder, rewrite the car's
    //          traffic id, then AddRaceCarBodyPartPair / AddRaceCarWheelPair with the OTHER
    //          side's full volume-instance id.
    //   4) Collide the two simple-traffic pair lists that have tests (queue-marker AddEntry
    //      pairs (TRAFFIC,TRAFFIC) / (RACECAR,TRAFFIC)), create the two triangle-cache collide
    //      streams, then run the per-race-car (skip frozen) and per-traffic world contact
    //      generation over the used-car bitsets.
    //   5) Begin the three stream producers and kick the three collide jobs (the job handles
    //      land in the mp*StreamJob members).
    // ==========================================================================================
    void VehicleManager::StartVehicleContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
        const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* lpOverlapPairs,
        f32 lfTimeStep,
        BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
        CgsModule::IOBufferStack* lpIOBufferStack,
        CgsMemory::LinearMalloc* lpLinearMalloc,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* /*lpPotentialContactInterface*/ )
    {
        typedef CgsSceneManager::SceneManagerIO::OutOverlapPair OutOverlapPair;

        // ---- (1) the five pair-list builders, console call order --------------------------------
        mDetachedWheelPrimPairBuilder.Prepare(lpLinearMalloc, KU16_PAIR_BUILDER_CAPACITY);      // +172488
        mDetachedPartPrimPairBuilder.Prepare(lpLinearMalloc, KU16_PAIR_BUILDER_CAPACITY);       // +172476
        mHingedPartVsVehiclePairBuilder.Prepare(lpLinearMalloc, KU16_PAIR_BUILDER_CAPACITY);    // +172500
        mTrafficSimpleTrafficPrimPairBuilder.Prepare(lpLinearMalloc, KU16_PAIR_BUILDER_CAPACITY); // +172552
        mRaceCarSimpleTrafficPrimPairBuilder.Prepare(lpLinearMalloc, KU16_PAIR_BUILDER_CAPACITY); // +172564

        // ---- (2) the two IO buffers + the generator arena + the cleared state -------------------
        lpIOBufferStack->CreateIOBuffer(&mpContactGenList, "Contact Gen List");
        CGS_ASSERT(mpContactGenList != nullptr, "mpContactGenList");                            // :79
        // (the CreateIOBuffer<T> template itself runs T::Construct, exactly as the console does
        //  -- @0x82614CB0; the former hand call here would have run it twice.)

        miFirstPartContactGenEntry = 0;                                                         // +172516

        lpIOBufferStack->CreateIOBuffer(&mpContactGenerator, "Contact Generator");
        mpContactGenerator->Prepare();          // the inlined arena feed (see the header banner)

        mOverlappingRaceCars.UnSetAll();                                                        // +172544 `std 0`
        miNumTrafficSphereWorldTests = 0;                                                       // +172580

        mpSphereSphereStreamProducer =
            mpContactGenerator->CreateCollideSphereListWithSphereListStream(KI_STREAM_MAX_COMMANDS); // +172520

        // ---- (3) the overlap-pair walk ----------------------------------------------------------
        for (s32 liPair = 0; liPair < lpOverlapPairs->GetLength(); ++liPair)
        {
            const OutOverlapPair& lrPair = lpOverlapPairs->GetEvent(liPair);

            const u64 luIdA64  = lrPair.muVolumeInstanceIdA.muId;
            const u64 luIdB64  = lrPair.muVolumeInstanceIdB.muId;
            const u32 luWordA  = static_cast<u32>(luIdA64 >> 32);
            const u32 luWordB  = static_cast<u32>(luIdB64 >> 32);
            const u32 luOwnerA = GetWordOwner(luWordA);
            const u32 luOwnerB = GetWordOwner(luWordB);

            const bool lbAIsCar = (luOwnerA == 1u || luOwnerA == 2u);
            const bool lbBIsCar = (luOwnerB == 1u || luOwnerB == 2u);

            if (lbAIsCar && lbBIsCar)
            {
                // ---- car vs car -----------------------------------------------------------------
                u32 luPhysA      = luWordA;
                u32 luPhysB      = luWordB;
                u64 luQwA        = luIdA64;   // the ORIGINAL (global) ids ride to DoCarCar
                u64 luQwB        = luIdB64;
                u16 lu16QueueIdx = 7;         // racecar-racecar unless rewritten below

                // Traffic A: global -> physical (127 == unmapped, skip the pair).
                if (luOwnerA == 2u)
                {
                    const u32 luIndex = GetWordIndex(luWordA);
                    CGS_ASSERT(luIndex < 600u,
                               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // BrnPhysicalTrafficManager.h:944
                    const u8 lu8Physical =
                        mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luIndex];
                    if (lu8Physical == 127u)
                    {
                        continue;
                    }
                    luPhysA = (static_cast<u32>(lu8Physical) << 10) | 0x02000000u;
                }

                // Traffic B: same rewrite.
                if (luOwnerB == 2u)
                {
                    const u32 luIndex = GetWordIndex(luWordB);
                    CGS_ASSERT(luIndex < 600u,
                               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // :944
                    const u8 lu8Physical =
                        mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luIndex];
                    if (lu8Physical == 127u)
                    {
                        continue;
                    }
                    luPhysB = (static_cast<u32>(lu8Physical) << 10) | 0x02000000u;

                    // Both traffic: skip pairs on the SAME unbroken articulated train (cab +
                    // trailer share a joint index; the X360 reads the vehicles' miJointIndex
                    // @+44 directly -- the PS3 keeps GetArticulatedJointIndex out-of-line).
                    if (GetWordOwner(luPhysA) == 2u)
                    {
                        const PhysicalTrafficVehicle* lpVehicleA =
                            mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(GetWordIndex(luPhysA)));
                        const PhysicalTrafficVehicle* lpVehicleB =
                            mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(GetWordIndex(luPhysB)));
                        if (lpVehicleA->HasNonBrokenJoint() && lpVehicleB->HasNonBrokenJoint()
                            && lpVehicleA->miJointIndex == lpVehicleB->miJointIndex)
                        {
                            continue;
                        }
                        lu16QueueIdx = 13;   // traffic-traffic -> custom queue [13]
                    }
                }

                // Canonicalise: the race car is side A (swap the physical AND global ids).
                if (GetWordOwner(luPhysB) == 1u)
                {
                    const u32 luSwap = luPhysA; luPhysA = luPhysB; luPhysB = luSwap;
                    const u64 luSwap64 = luQwA; luQwA = luQwB; luQwB = luSwap64;
                }
                if (GetWordOwner(luPhysA) == 1u && GetWordOwner(luPhysB) == 2u)
                {
                    lu16QueueIdx = 8;    // racecar-traffic -> custom queue [8]
                }
                CGS_ASSERT(!(GetWordOwner(luPhysA) == 2u && GetWordOwner(luPhysB) == 1u),
                           "!( lCarAPhysicsId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && "
                           "lCarBPhysicsId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR )");     // :174

                // Both RACE CARS (per the ORIGINAL record ids): mark the symmetric overlap bits,
                // then skip type-2/type-2 and hidden cars.
                if (luOwnerA == 1u && luOwnerB == 1u)
                {
                    const u32 luIndexA = GetWordIndex(luWordA);
                    const u32 luIndexB = GetWordIndex(luWordB);

                    // BitArray<64>::SetBit carries its own bounds tripwire (CgsBitArray.h:222).
                    mOverlappingRaceCars.SetBit(static_cast<s32>(8u * luIndexA + luIndexB));
                    mOverlappingRaceCars.SetBit(static_cast<s32>(8u * luIndexB + luIndexA));

                    if (maeRaceCarTypes[luIndexA] == 2 && maeRaceCarTypes[luIndexB] == 2)
                    {
                        continue;
                    }
                    if (IsRaceCarHidden(static_cast<s32>(luIndexA))
                        || IsRaceCarHidden(static_cast<s32>(luIndexB)))
                    {
                        continue;
                    }
                }

                DoCarCarContactGeneration(CgsSceneManager::EntityId(static_cast<u32>(luQwA >> 32)),
                                          CgsSceneManager::EntityId(static_cast<u32>(luQwB >> 32)),
                                          CgsSceneManager::EntityId(luPhysA),
                                          CgsSceneManager::EntityId(luPhysB),
                                          lpDeformationManager,
                                          mpContactGenList,
                                          mpContactGenerator,
                                          mpSphereSphereStreamProducer,
                                          lu16QueueIdx,
                                          lfTimeStep);
                lpDeformationManager->AddHingedBodyPartPairs(EntityId{ luPhysA }, EntityId{ luPhysB },
                                                             &mHingedPartVsVehiclePairBuilder);
                continue;
            }

            // ---- deformation part / detached wheel vs car ---------------------------------------
            const bool lbAIsPart  = (luOwnerA == 6u || luOwnerA == 7u);
            const bool lbBIsPart  = (luOwnerB == 6u || luOwnerB == 7u);
            const bool lbAIsWheel = (luOwnerA == 9u || luOwnerA == 10u);
            const bool lbBIsWheel = (luOwnerB == 9u || luOwnerB == 10u);

            if ((lbAIsPart && lbBIsCar) || (lbBIsPart && lbAIsCar))
            {
                if (mDetachedPartPrimPairBuilder.GetNumTests() >= KU16_MAX_PAIR_TESTS)
                {
                    CGS_ASSERT(false, "Too many part Vs. Car contact tests\n");                  // :281
                    continue;
                }

                // The CAR side's entity word (traffic rewritten global -> physical, unmapped ==
                // skip); the OTHER side's FULL volume-instance id rides to the builder.
                const bool lbCarIsA   = lbAIsCar;
                u32        luCarWord  = lbCarIsA ? luWordA : luWordB;
                if (GetWordOwner(luCarWord) == 2u)
                {
                    const u32 luIndex = GetWordIndex(luCarWord);
                    CGS_ASSERT(luIndex < 600u,
                               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // :944
                    const u8 lu8Physical =
                        mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luIndex];
                    if (lu8Physical == 127u)
                    {
                        continue;
                    }
                    luCarWord = (static_cast<u32>(lu8Physical) << 10) | 0x02000000u;
                }

                lpDeformationManager->AddRaceCarBodyPartPair(
                    EntityId{ luCarWord },
                    lbCarIsA ? lrPair.muVolumeInstanceIdB : lrPair.muVolumeInstanceIdA,
                    &mDetachedPartPrimPairBuilder);
            }
            else if ((lbAIsWheel && lbBIsCar) || (lbBIsWheel && lbAIsCar))
            {
                if (mDetachedWheelPrimPairBuilder.GetNumTests() >= KU16_MAX_PAIR_TESTS)
                {
                    CGS_ASSERT(false, "Too many wheel Vs. Car contact tests\n");                 // :325
                    continue;
                }

                const bool lbCarIsA   = lbAIsCar;
                u32        luCarWord  = lbCarIsA ? luWordA : luWordB;
                if (GetWordOwner(luCarWord) == 2u)
                {
                    const u32 luIndex = GetWordIndex(luCarWord);
                    CGS_ASSERT(luIndex < 600u,
                               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // :944
                    const u8 lu8Physical =
                        mPhysicalTrafficManager.mu8GlobalToPhysicalEntityIndexMap[luIndex];
                    if (lu8Physical == 127u)
                    {
                        continue;
                    }
                    luCarWord = (static_cast<u32>(lu8Physical) << 10) | 0x02000000u;
                }

                lpDeformationManager->AddRaceCarWheelPair(
                    EntityId{ luCarWord },
                    lbCarIsA ? lrPair.muVolumeInstanceIdB : lrPair.muVolumeInstanceIdA,
                    &mDetachedWheelPrimPairBuilder);
            }
            // any other owner pairing: dropped, exactly as the console falls through
        }

        // ---- (4) the two simple-traffic pair lists + the triangle-cache streams -----------------
        if (mTrafficSimpleTrafficPrimPairBuilder.GetNumTests() != 0)                             // +172558
        {
            mpContactGenerator->CollidePrimitivePairList(&mTrafficSimpleTrafficPrimPairBuilder,
                                                         KU16_COLLIDE_MAX_RESULTS, KU_COLLIDE_FLAGS,
                                                         KU16_COLLIDE_TAG);
            mpContactGenList->AddEntry(EntityId{ 0x02000000u }, EntityId{ 0x02000000u }, 0, 0);
        }
        if (mRaceCarSimpleTrafficPrimPairBuilder.GetNumTests() != 0)                             // +172570
        {
            mpContactGenerator->CollidePrimitivePairList(&mRaceCarSimpleTrafficPrimPairBuilder,
                                                         KU16_COLLIDE_MAX_RESULTS, KU_COLLIDE_FLAGS,
                                                         KU16_COLLIDE_TAG);
            mpContactGenList->AddEntry(EntityId{ 0x01000000u }, EntityId{ 0x02000000u }, 0, 0);
        }

        mpSphereTriangleStreamProducer =
            mpContactGenerator->CreateCollideSphereListWithTriangleListStream(KI_STREAM_MAX_COMMANDS);      // +172524
        mpSweptSphereTriangleStreamProducer =
            mpContactGenerator->CreateCollideSweptSphereListWithTriangleListStream(KI_STREAM_MAX_COMMANDS); // +172528

        // ---- the per-race-car world pass (skip frozen cars) -------------------------------------
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != -1;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            // The inlined EntityId build's own bound tripwire (CgsEntityId.h:116).
            CGS_ASSERT(static_cast<u32>(liCar) < (1u << 14),
                       "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");                    // CgsEntityId.h:116

            const s32 liModelIndex = lpDeformationManager->FindModelIndexByEntityID(
                EntityId{ (static_cast<u32>(liCar) << 10) | 0x01000000u });
            // ⚠️ [FLAG PC bring-up] the console CGS_ASSERT(liModelIndex != -1, "liIndex != -1")
            // (BrnDeformationManager.h:560) is DEGRADED to the log-once below (conductor,
            // 2026-08-11, create-drain wave, boot-measured: with the first live car it fired per
            // frame -- 178 halts in 80 s -- because the deformation model table is permanently -1
            // on this build: ProcessAddDeformationModelEvents lives in the UNMOUNTED
            // BrnDeformationManager.cpp, and behind that sit the mpAttribs Prepare-skip guard and
            // the parked NULL model handle. On the console the model is always registered and the
            // assert never fires. The `continue` is the honest deferral consequence: race-car
            // world CONTACT generation (the crash/body-shell path) stays off; traction lines do
            // not need it. RESTORE the console assert WHEN the deformation-manager mount lands.
            if (liModelIndex == -1)
            {
                static bool sbLoggedNoDeformationModel = false;
                if (!sbLoggedNoDeformationModel)
                {
                    sbLoggedNoDeformationModel = true;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] StartVehicleContactGeneration: no deformation "
                               "model for live race car (table is -1 until the deformation-manager "
                               "mount) -- car SKIPPED for world contact generation, console assert "
                               "'liIndex != -1' degraded to this line. Reported once, not per frame\n";
                }
                continue;   // host bounds guard; the console's assert is fire-and-continue
            }

            // Skip frozen cars: the asm reads the model's vehicle-physics frame flag @+112
            // (mbFrozen through the ExternallySimulatedBody base).
            if (lpDeformationManager->GetDeformableObject(liModelIndex)->GetVehiclePhysics()->IsFrozen())
            {
                continue;
            }

            DoRaceCarWorldContactGeneration(liCar, lpDeformationManager, lpTriangleCacheInterface,
                                            mpContactGenList, mpContactGenerator,
                                            mpSphereTriangleStreamProducer,
                                            mpSweptSphereTriangleStreamProducer,
                                            5u);   // the baked queue selector (`li 5` stack arg)
        }

        // ---- the per-traffic world pass ---------------------------------------------------------
        for (s32 liTraffic = mPhysicalTrafficManager.mUsedTrafficVehicles.GetFirstNonZeroBit();
             liTraffic != -1;
             liTraffic = mPhysicalTrafficManager.mUsedTrafficVehicles.GetNextNonZeroBit(liTraffic))
        {
            DoTrafficCarWorldContactGeneration(liTraffic, lpDeformationManager, lpTriangleCacheInterface,
                                               mpContactGenList, mpContactGenerator,
                                               mpSphereTriangleStreamProducer,
                                               9u,   // the baked queue selector (`li 9`) == the bridge's traffic-world queue
                                               lpLinearMalloc);
        }

        // ---- (5) begin the three producers, kick the three collide jobs -------------------------
        // ⭐ 2026-08-14 (walls leg 1): the PC-BUILD GUARD that sat here since the conductor wave
        // (2026-08-09) is DELETED per its own instruction ("Delete with the stream family") --
        // the three CreateCollide*Stream factories are REAL now
        // (CgsCollisionGenerator_CollideStreams.cpp) and assert rather than return null.
        mpSphereTriangleStreamProducer->Begin();        // the three inlined Begin bodies
        mpSweptSphereTriangleStreamProducer->Begin();   // (see CgsSimpleDataStreamProducer_Begin.cpp)
        mpSphereSphereStreamProducer->Begin();

        mpSphereTriangleStreamJob =
            mpContactGenerator->RunCollideSphereListWithTriangleListStream(mpSphereTriangleStreamProducer, 0);      // +172536
        mpSweptSphereTriangleStreamJob =
            mpContactGenerator->RunCollideSweptSphereListWithTriangleListStream(mpSweptSphereTriangleStreamProducer, 0); // +172540
        mpSphereSphereStreamJob =
            mpContactGenerator->RunCollideSphereListWithSphereListStream(mpSphereSphereStreamProducer);             // +172532
    }

    // =================================================================================================
    // The four TRAP STUBS (closure enforcement) -- see the TU banner. RECONSTRUCT-NEXT.
    // =================================================================================================
    void VehicleManager::DoCarCarContactGeneration(
        CgsSceneManager::EntityId /*lGlobalIdA*/, CgsSceneManager::EntityId /*lGlobalIdB*/,
        CgsSceneManager::EntityId /*lPhysicsIdA*/, CgsSceneManager::EntityId /*lPhysicsIdB*/,
        BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/,
        BrnPhysics::ContactGenList* /*lpContactGenList*/,
        CgsSceneManager::CgsCollision::CollisionGenerator* /*lpContactGenerator*/,
        CgsMemory::SimpleDataStreamProducer* /*lpSphereSphereStream*/,
        u16 /*lu16QueueIndex*/, f32 /*lfTimeStep*/)
    {
        // ⭐ DEGRADED TRAP → log-once gate (2026-08-14 deformation-mount wave). The moment the
        // deformation model table went live (table != -1) this body became REACHED PER FRAME for
        // overlapping car pairs; an assert trap here would storm. The honest state: the car-car
        // contact-generation leg (~250 asm / 15 callees) is STILL NOT RECONSTRUCTED — this gate
        // produces NO contacts, loudly, once. Reconstruct with the collide-stream family wave.
        static bool s_bLoggedCarCarGate = false;
        if (!s_bLoggedCarCarGate)
        {
            s_bLoggedCarCarGate = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: DoCarCarContactGeneration @0x8261BB38 reached (model table "
                       "is LIVE) but not reconstructed -- car-car contacts NOT generated [FLAG PC "
                       "boot gate]. Reported once, not per frame\n";
        }
    }

    // ==============================================================================================
    // ⭐⭐ DoRaceCarWorldContactGeneration @0x825EB140 (131) — REAL as of walls leg 1 (2026-08-14).
    // THE GATE THAT WAS HERE (log-once, "world contacts NOT generated") IS DELETED.
    //
    // One live unfrozen race car per call, per frame: take the car's window into the triangle
    // cache, take its deformation spheres from the LIVE model table (the mount the previous wave
    // landed), and post ONE sphere-list-vs-triangle-list command into this frame's collide
    // stream — swept (continuous) spheres when the car is fast enough (IsUsingSweptSpheres),
    // in-place spheres otherwise. Then mark the car in the contact-gen list so the harvest
    // (EndVehicleContactGeneration, REAL as of walls leg 3) knows a world entry exists for it.
    //
    // The asm, top to bottom (every callee LANDED as of this wave — the queries were sliced to
    // the mounted BrnDeformationManager_ContactQueries.cpp, GetSpheresForCar lifted from its
    // export hole, the posters written in CgsCollisionGenerator_CollideStreams.cpp):
    //   0x825EB168  index < 0x4000 tripwire            (the inlined EntityId build, CgsEntityId.h:116)
    //   0x825EB190  interface GetCache (inlined: assert "mpTriangleCacheManager != NULL" + forward)
    //   0x825EB1DC  GetNumCachedTriangleBatches
    //   0x825EB1F0  assert lpCachedTriangles           (:943 — the cache MUST hold this car;
    //                                                   PrepareTriangleCache registered it at create)
    //   0x825EB218  IsUsingSweptSpheres(entityId)
    //   0x825EB234  out-slot zero-initialised, THEN GetSpheresForCar / GetSweptSpheresForCar —
    //               the plain getter returns -1 gracefully WITHOUT writing the slot, so the
    //               assert "lpSpheres" (:952 / :979) is the caller's own null test on the slot.
    //   0x825EB280  TriangleList::CheckAlignment + ValidateTriangles over the {cache, batches} pair
    //   0x825EB2B0  AddSphereListWithTriangleListToStream(spheres, tris, 200, 2.0f, queueIdx, 1,
    //               sphere stream)      — or the swept twin with 0.5f into the swept stream
    //   0x825EB344  ContactGenList::AddEntry(EntityId{0}, EntityId{car}, 0, 1)  (@0x825B59B0 —
    //               the WORLD marker entry: side A owner 0 == E_ENTITYTYPE_WORLD)
    //
    // ⭐ WHAT THIS CHANGES AT RUNTIME, STATED PLAINLY: commands now FLOW — one per live car per
    // frame — and the Run* dispatcher drains them into ContactGeneratorJob::Execute, where case 6
    // (ExecuteSphereListWithTriangleListStream @0x829235C8) is a LOUD NAMED GATE: the sphere-vs-
    // Triangle4 contact kernel (IntersectTriangle4Sphere_HackyBurnoutVersion @0x829238E8-family)
    // is the next leg. Contacts are NOT generated yet, and nothing pretends they are — the gate
    // line in the log is the honest witness that the plumbing carried a real command.
    // ==============================================================================================
    void VehicleManager::DoRaceCarWorldContactGeneration(
        s32 liRaceCarIndex, BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
        BrnPhysics::ContactGenList* lpContactGenList,
        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
        CgsMemory::SimpleDataStreamProducer* lpSphereTriangleStream,
        CgsMemory::SimpleDataStreamProducer* lpSweptSphereTriangleStream,
        u32 luQueueIndex)
    {
        typedef CgsSceneManager::CgsCollision::TriangleList    TriangleList;
        typedef CgsSceneManager::CgsCollision::SphereList      SphereList;
        typedef CgsSceneManager::CgsCollision::SweptSphereList SweptSphereList;

        // The inlined EntityId build's own bound tripwire (0x825EB168).
        CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < (1u << 14),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");           // CgsEntityId.h:116
        const u32 luEntityWord = (static_cast<u32>(liRaceCarIndex) << 10) | 0x01000000u;

        // The car's window into the shared triangle cache (GetCache inlines the
        // "mpTriangleCacheManager != NULL" tripwire the asm carries at 0x825EB1A4).
        const CgsGeometric::Triangle4* lpCachedTriangles =
            lpTriangleCacheInterface->GetCache(liRaceCarIndex);
        const s32 liNumTriangleBatches =
            lpTriangleCacheInterface->GetNumCachedTriangleBatches(liRaceCarIndex);
        CGS_ASSERT(lpCachedTriangles != nullptr, "lpCachedTriangles");              // :943

        const bool lbUseSweptSpheres =
            lpDeformationManager->IsUsingSweptSpheres(EntityId{ luEntityWord });

        // ---- [cgen] PC bring-up instrument -- DELETE WHEN world collision is proven map-wide ----
        // OPT-IN (BRN_WALL_PROBE=1), same latch as [cvalid] downstream.
        //
        // ⭐ WHY: [cvalid] proved raw world-contact candidates stop the moment the car leaves the
        // junkyard, and the run log shows exactly ONE contact-generator gate firing --
        // ExecuteSweptSphereListWithTriangleListStream @0x82925238, the kernel behind the SWEPT
        // branch below. That makes "the car went fast, took the swept branch, and the swept kernel
        // is a gate" the obvious story, but it is still an INFERENCE from two separate numbers,
        // and walls leg 12's control reverses into a wall at 31 m/s and DOES collide, which the
        // story does not obviously explain. This prints the branch actually taken, per car, so the
        // selector is WITNESSED rather than reasoned about.
        {
            static s32 siCGenProbe = -1;
            if (siCGenProbe < 0)
            {
                const char* lpcEnv = getenv("BRN_WALL_PROBE");
                siCGenProbe = (lpcEnv != nullptr && lpcEnv[0] != '0') ? 1 : 0;
            }
            static u32 suCGenFrame = 0u;
            static bool sabWasSwept[8] = { false, false, false, false, false, false, false, false };
            ++suCGenFrame;
            const s32 liSlot = (liRaceCarIndex >= 0 && liRaceCarIndex < 8) ? liRaceCarIndex : 0;
            const bool lbEdge = (lbUseSweptSpheres != sabWasSwept[liSlot]);
            sabWasSwept[liSlot] = lbUseSweptSpheres;
            if (siCGenProbe == 1 && CgsDev::Log::gpDebugPrint != nullptr
                && (lbEdge || (suCGenFrame % 300u) == 0u))
            {
                // ⚠️ THE LABELS WERE UPDATED WITH THE CODE (swept leg, 2026-08-16). They used to
                // read "SWEPT(gated kernel)", which was true when the swept worker was a boot
                // gate and became a LIE the moment it landed. A diagnostic that describes a
                // state the build has left is this campaign's most repeated bug.
                *CgsDev::Log::gpDebugPrint
                    << "[cgen] n " << static_cast<s32>(suCGenFrame)
                    << " car " << liRaceCarIndex
                    << (lbEdge ? " EDGE" : "")
                    << (lbUseSweptSpheres ? " SWEPT(real kernel)" : " INPLACE(real kernel)")
                    << " batches " << liNumTriangleBatches << "\n";
            }
        }
        // ---- end [cgen] --------------------------------------------------------------------------

        if (!lbUseSweptSpheres)
        {
            // ---- the in-place sphere pass (0x825EB23C..0x825EB2B0) --------------------------
            const CgsGeometric::Sphere* lpSpheres = 0;    // 0x825EB234 stw 0 (the callee may not write it)
            const s32 liNumSpheres =
                lpDeformationManager->GetSpheresForCar(EntityId{ luEntityWord }, &lpSpheres);
            CGS_ASSERT(lpSpheres != nullptr, "lpSpheres");                          // :952

            SphereList lSphereList;
            lSphereList.mpSpheres    = reinterpret_cast<u8*>(
                const_cast<CgsGeometric::Sphere*>(lpSpheres));
            lSphereList.miNumSpheres = liNumSpheres;

            TriangleList lTriangleList;
            lTriangleList.mpTriangles    = const_cast<CgsGeometric::Triangle4*>(lpCachedTriangles);
            lTriangleList.miNumTriangles = liNumTriangleBatches;
            lTriangleList.CheckAlignment();
            lTriangleList.ValidateTriangles();

            lpContactGenerator->AddSphereListWithTriangleListToStream(
                &lSphereList, &lTriangleList,
                KU16_COLLIDE_MAX_RESULTS,                    // li r6, 0xC8
                KF_SPHERE_TRIANGLE_CONTACT_PADDING,          // lfs f1, flt_82001D9C (2.0f)
                luQueueIndex,                                // lwz r8, arg_54 (the baked `li 5`)
                1,                                           // li r9, 1
                lpSphereTriangleStream);                     // mr r10, r24
        }
        else
        {
            // ---- the swept (continuous) pass (0x825EB2B8..0x825EB32C) -----------------------
            const CgsGeometric::SweptSphere* lpSweptSpheres = 0;
            const s32 liNumSweptSpheres =
                lpDeformationManager->GetSweptSpheresForCar(EntityId{ luEntityWord },
                                                            &lpSweptSpheres);
            CGS_ASSERT(lpSweptSpheres != nullptr, "lpSpheres");                     // :979

            SweptSphereList lSweptSphereList;
            lSweptSphereList.mpaSweptSpheres = reinterpret_cast<u8*>(
                const_cast<CgsGeometric::SweptSphere*>(lpSweptSpheres));
            lSweptSphereList.miNumSpheres    = liNumSweptSpheres;

            TriangleList lTriangleList;
            lTriangleList.mpTriangles    = const_cast<CgsGeometric::Triangle4*>(lpCachedTriangles);
            lTriangleList.miNumTriangles = liNumTriangleBatches;
            lTriangleList.CheckAlignment();
            lTriangleList.ValidateTriangles();

            lpContactGenerator->AddSweptSphereListWithTriangleListToStream(
                &lSweptSphereList, &lTriangleList,
                KU16_COLLIDE_MAX_RESULTS,
                KF_SWEPT_SPHERE_TRIANGLE_CONTACT_PADDING,    // lfs f1, flt_82001DA0 (0.5f)
                luQueueIndex,
                1,
                lpSweptSphereTriangleStream);                // mr r10, r23
        }

        // 0x825EB330..0x825EB344: the world marker entry — side A owner 0 (E_ENTITYTYPE_WORLD),
        // side B this car, flags (0, 1).
        lpContactGenList->AddEntry(EntityId{ 0u }, EntityId{ luEntityWord }, 0, 1);
    }

    void VehicleManager::DoTrafficCarWorldContactGeneration(
        s32 /*liTrafficIndex*/, BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* /*lpTriangleCacheInterface*/,
        BrnPhysics::ContactGenList* /*lpContactGenList*/,
        CgsSceneManager::CgsCollision::CollisionGenerator* /*lpContactGenerator*/,
        CgsMemory::SimpleDataStreamProducer* /*lpSphereTriangleStream*/,
        u32 /*luQueueIndex*/, CgsMemory::LinearMalloc* /*lpLinearMalloc*/)
    {
        CGS_ASSERT(false, "TRAP: VehicleManager::DoTrafficCarWorldContactGeneration @0x8261BF28 "
                          "(.ida-exports HOLE; PS3 0x789760) not reconstructed (big-five #2 closure stub)\n");
    }

    // ⭐⭐ TRAP STUB DELETED 2026-08-11 (physics->output publish wave). VehicleManager::
    // IsRaceCarHidden @0x825C2EA0 is REAL, in BrnVehicleManager_WriteOutVehicleStats.cpp beside
    // its per-frame caller. It was NOT an ".ida-exports HOLE" -- the banner at the top of this
    // file said so because the address has no JSON, but the function is in the IDB and a targeted
    // headless IDA 9.3 pull produced all 104 instructions. MISSING JSON != MISSING FUNCTION.
    // If a definition for it reappears here the link will say so (LNK2005).

    // ==============================================================================================
    // ⭐⭐ AddContactResultsToQueue @0x825EB350 (222) — THE HARVEST (walls leg 3, 2026-08-14).
    // PS3 DecFIGS 0x70F454 (392; the mangle is the signature authority). DWARF h:1116.
    //
    // Walk the contact-gen entries [0, miFirstPartContactGenEntry) — the pre-part window the
    // vehicle passes appended this frame. Result list i BELONGS to entry i: every
    // Do*ContactGeneration appends exactly one ContactGenList entry and carves exactly one
    // CollisionResultList (PrepareNewPrimitiveTestResultsList returns mu16NumUsedResultLists++),
    // so the two indices advance in lockstep by construction. For each of the list's
    // mu16NumResults 80-byte PrimitiveTestResults, build one 76-byte PotentialContact and post it
    // to maCustomEventQueues[UserTagA] (== 5 on the race-car world path — Start's baked queue
    // selector riding the list's tag):
    //   mPointOnA <- record.mPrimitive0Contact (+0x20, the TRIANGLE contact point)
    //   mPointOnB <- record.mPrimitive1Contact (+0x30, the SPHERE contact point)
    //   mNormal   <- NEGATED (vxor 0x80000000 splat, all four lanes) record normal:
    //               UserTagB != 0 selects mPrimitive0Normal (+0x00), else mPrimitive1Normal (+0x10)
    //               (the world path passes UserTagB == 1 -> the triangle-side normal, flipped so
    //                it points A->B == triangle->sphere == world->car)
    //   muVolumeInstanceIdA/B <- the entry's entity word (high 32) | (the entry's per-side volume-
    //               instance base offset + the record's primitive index) — both consoles load the
    //               entry word as a 32-bit read of the BE muId's high half
    //   muPolyTagA/B <- record.muPrimitive0Tag/muPrimitive1Tag (+0x40/+0x44)
    //   mu16PrimitiveIndexA/B <- record.muPrimitive0Index/muPrimitive1Index RAW (+0x48/+0x4A)
    // "Bad Part index" tripwire (:1317): on the part queue (UserTagA == 1) a triangle-side tag
    // >= 50 is not a valid part index. The streamed value tail is lowered to the static prefix
    // per the standing project rule.
    // ==============================================================================================
    void VehicleManager::AddContactResultsToQueue(
        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
        BrnPhysics::ContactGenList* lpContactGenList,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactsInterface)
    {
        typedef CgsSceneManager::CgsCollision::CollisionResultList CollisionResultList;
        typedef CgsSceneManager::CgsCollision::PrimitiveTestResult PrimitiveTestResult;
        typedef CgsSceneManager::SceneManagerIO::PotentialContact  PotentialContact;

        const s32 liNumEntries = miFirstPartContactGenEntry;                     // +172516
        for (s32 liEntry = 0; liEntry < liNumEntries; ++liEntry)
        {
            // GetResultList carries the console's own bounds tripwire
            // ("luIndex < mu16NumUsedResultLists", CgsCollisionGenerator.h:303) and returns the
            // 16-byte list header BY VALUE, exactly as @0x825B2AE0 does.
            const CollisionResultList lResultList =
                lpContactGenerator->GetResultList(static_cast<u16>(liEntry));
            const u16 lu16NumResults = lResultList.mu16NumResults;
            if (lu16NumResults == 0)
            {
                continue;
            }

            // GetEntry carries "liEntry < miNumEntries" (BrnContactGenerationList.h:89 — the line
            // both consoles assert at, per record; hoisted to the fetch here, identical failure
            // surface for in-range counts).
            const ContactGenList::ContactGenEntry& lrEntry = lpContactGenList->GetEntry(liEntry);
            const u32 luEntityWordA = static_cast<u32>(lrEntry.mIdA.muId >> 32);
            const u32 luEntityWordB = static_cast<u32>(lrEntry.mIdB.muId >> 32);

            // meResultType == 0 lists carry 80-byte PrimitiveTestResults; the console walks the
            // records at the raw 80 stride (R31 += 80), NOT through the 112-stride GetResult.
            const PrimitiveTestResult* lpaResults =
                reinterpret_cast<const PrimitiveTestResult*>(lResultList.mpResults);

            for (u16 lu16Result = 0; lu16Result < lu16NumResults; ++lu16Result)
            {
                // The console's per-record cursor bound (CgsCollisionResultList.h:148).
                CGS_ASSERT(lu16Result < lResultList.mu16NumResults, "lu16Index < mu16NumResults");
                const PrimitiveTestResult& lrRecord = lpaResults[lu16Result];

                PotentialContact lContact;
                // Full 16-byte lane copies (lvx/stvx on the console) -- Vector3Plus source,
                // Vector3 destination, w lane carried verbatim.
                lContact.mPointOnA = Vector3{ lrRecord.mPrimitive0Contact.x,
                                              lrRecord.mPrimitive0Contact.y,
                                              lrRecord.mPrimitive0Contact.z,
                                              lrRecord.mPrimitive0Contact.w };   // triangle point
                lContact.mPointOnB = Vector3{ lrRecord.mPrimitive1Contact.x,
                                              lrRecord.mPrimitive1Contact.y,
                                              lrRecord.mPrimitive1Contact.z,
                                              lrRecord.mPrimitive1Contact.w };   // sphere point

                // vxor with the 0x80000000 splat == all-four-lane sign flip (the SwapEntityOrder
                // precedent), of the tag-selected source normal.
                const Vector3& lrNormalSrc = (lResultList.mu16UserTagB != 0)
                    ? lrRecord.mPrimitive0Normal
                    : lrRecord.mPrimitive1Normal;
                lContact.mNormal.x = -lrNormalSrc.x;
                lContact.mNormal.y = -lrNormalSrc.y;
                lContact.mNormal.z = -lrNormalSrc.z;
                lContact.mNormal.w = -lrNormalSrc.w;

                CGS_ASSERT(!(lResultList.mu32UserTagA == 1u && lrRecord.muPrimitive0Tag >= 50u),
                           "Bad Part index: ");                                  // :1317

                lContact.muVolumeInstanceIdA.muId = (static_cast<u64>(luEntityWordA) << 32)
                    | static_cast<u32>(lrEntry.mIdAVolInstOffset + lrRecord.muPrimitive0Index);
                lContact.muVolumeInstanceIdB.muId = (static_cast<u64>(luEntityWordB) << 32)
                    | static_cast<u32>(lrEntry.mIdBVolInstOffset + lrRecord.muPrimitive1Index);

                lContact.muPolyTagA          = lrRecord.muPrimitive0Tag;
                lContact.muPolyTagB          = lrRecord.muPrimitive1Tag;
                lContact.mu16PrimitiveIndexA = lrRecord.muPrimitive0Index;
                lContact.mu16PrimitiveIndexB = lrRecord.muPrimitive1Index;

                // sub_825E73D0 == the out-of-line PotentialContactInterface::AddEvent(u32, ...)
                // (assert + overflow warning + bounds-gated append).
                lpPotentialContactsInterface->AddEvent(lResultList.mu32UserTagA, lContact);
            }
        }
    }

    // ==============================================================================================
    // ⭐⭐ EndVehicleContactGeneration @0x8261AC38 (661) — REAL as of walls leg 3 (2026-08-14).
    // THE CONDUCTOR GATE OF THIS NAME (BrnPhysicsConductorGates.cpp) IS DELETED.
    //
    // The async-generation join + harvest, mirroring Start step (5) in reverse:
    //   1) WaitOn the three collide-stream jobs (+172536 sphere-tri, +172540 swept, +172532
    //      sphere-sphere — null-guarded; on this single-threaded build the batches already ran
    //      inline and WaitOn's null-backend guard returns immediately, the checked traction-leg
    //      precedent);
    //   2) End() the three stream producers (+172524/+172528/+172520 — the console inlines
    //      DataStreamCommandPoster::End(producer+0x80) + the mbIsStreaming drop, which is
    //      byte-for-byte SimpleDataStreamProducer::End());
    //   3) AddContactResultsToQueue — the harvest (above);
    //   4) the HIDE_ONLINE tail: unhide network race cars that stopped overlapping (BitArray walk
    //      over mHiddenRaceCars; per car: OBB overlap re-test via Box::Set + BoxOverlappingTest
    //      against every mOverlappingRaceCars partner, the mRaceCarsAddedForCollision /
    //      mNetworkCarsAddedForCollisionThisFrame / mNetworkCarsRecievedFirstUpdate gates, the
    //      mauNetworkCarHiddenFramesRemaining countdown, the mbPlayerCarInJunkYard hold, then
    //      "HIDE_ONLINE: Making race car N, type T visible and collidable\n" + UnSetBit).
    //      ⭐ GATED, NOT RECONSTRUCTED, and the gate is PROVABLY UNREACHABLE TODAY: the only
    //      in-tree writer of mHiddenRaceCars is Construct's UnSetAll (grep witness, walls leg 3),
    //      so no bit can be set on this offline build — the tail is network-only behaviour.
    //      CgsGeometric::Box / Box::Set @0x825E6918 / BoxOverlappingTest (PS3 0x7A7B18) are also
    //      absent from the tree; reconstruct them WITH this tail when the network path lands.
    // ==============================================================================================
    void VehicleManager::EndVehicleContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* /*lpTriangleCacheInterface*/,
        const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>* /*lpOverlapPairs*/,
        f32 /*lfTimeStep*/,
        BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/,
        CgsModule::IOBufferStack* /*lpIOBufferStack*/,
        CgsMemory::LinearMalloc* /*lpLinearMalloc*/,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface)
    {
        // ---- (1) join the three collide jobs (the console's order: sphere-tri, swept, ss) ------
        if (mpSphereTriangleStreamJob)      { mpSphereTriangleStreamJob->WaitOn(); }      // +172536
        if (mpSweptSphereTriangleStreamJob) { mpSweptSphereTriangleStreamJob->WaitOn(); } // +172540
        if (mpSphereSphereStreamJob)        { mpSphereSphereStreamJob->WaitOn(); }        // +172532

        // ---- (2) close the three producers (the console's order: sphere-tri, swept, ss) --------
        mpSphereTriangleStreamProducer->End();        // +172524
        mpSweptSphereTriangleStreamProducer->End();   // +172528
        mpSphereSphereStreamProducer->End();          // +172520

        // ---- (3) THE HARVEST -------------------------------------------------------------------
        AddContactResultsToQueue(mpContactGenerator, mpContactGenList, lpPotentialContactInterface);

        // ⭐ [FLAG PC boot witness] one-shot: the first frame the harvest posts anything, say so
        // with the count — the leg-3 successor of leg-2's "24 PrimitiveTestResult(s)" witness.
        {
            static bool sbLoggedHarvest = false;
            if (!sbLoggedHarvest)
            {
                const s32 liRawWorldContacts =
                    lpPotentialContactInterface->GetRaceCarWithWorldQueue().GetLength();
                if (liRawWorldContacts > 0)
                {
                    sbLoggedHarvest = true;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint
                            << "⭐ world contacts HARVESTED: " << static_cast<u32>(liRawWorldContacts)
                            << " PotentialContact(s) in the race-car-vs-world queue [5] [FLAG PC "
                               "boot witness]. Reported once, not per frame\n";
                }
            }
        }

        // ---- (4) the HIDE_ONLINE tail — LOUD NAMED GATE (see the banner: provably dead) --------
        if (mHiddenRaceCars.GetFirstNonZeroBit() != -1)
        {
            static bool sbLoggedHideOnlineGate = false;
            if (!sbLoggedHideOnlineGate)
            {
                sbLoggedHideOnlineGate = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: EndVehicleContactGeneration's HIDE_ONLINE unhide tail "
                           "@0x8261AC38 reached (a hidden race car exists) but not reconstructed "
                           "-- car NOT unhidden [FLAG PC boot gate]. Needs CgsGeometric::Box::Set "
                           "@0x825E6918 + BoxOverlappingTest. Reported once, not per frame\n";
            }
        }
    }

    // ==============================================================================================
    // ⭐ StartPartContactGeneration @0x8262C220 (114) — PARTIAL as of walls leg 3 (2026-08-14);
    // the conductor gate of this name (BrnPhysicsConductorGates.cpp) is DELETED.
    //
    // REAL: the function's FIRST store — `miFirstPartContactGenEntry = mpContactGenList->
    // GetNumEntries()` (X360 0x8262C238: `lwz r11, 0xC08(list) ; stw r11, 0x2A1D4(this)`).
    // That boundary stamp is what AddContactResultsToQueue walks: the vehicle-pass entries
    // occupy [0, stamp), the part-pass entries append after it. Without the stamp the harvest
    // reads a permanent 0 and drains nothing — measured on the first leg-3 boot (both new
    // witnesses silent, 24 kernel results parked), which is exactly how this line was found.
    //
    // GATED (log-once): everything after the stamp — the three pair-list Collide legs
    // (part/wheel/hinged builders, queue tags 3/4/2, with their marker AddEntry calls), the
    // part-vs-triangle collide stream create (@sub_82811DD0)/Begin/Run, and the two
    // DeformationManager Do*WorldContactGeneration drivers. That is the PART contact family
    // (EndPartContactGeneration @0x8261B690 is its harvest twin) — a later leg.
    // ==============================================================================================
    void VehicleManager::StartPartContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* /*lpTriangleCacheInterface*/,
        f32 /*lfTimeStep*/,
        BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/,
        CgsModule::IOBufferStack* /*lpIOBufferStack*/,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* /*lpPotentialContactInterface*/,
        CgsMemory::LinearMalloc* /*lpLinearMalloc*/)
    {
        // The boundary stamp (REAL): the vehicle-pass entry count, read by the harvest.
        miFirstPartContactGenEntry = mpContactGenList->GetNumEntries();          // +172516

        static bool s_bLoggedPartGate = false;
        if (!s_bLoggedPartGate)
        {
            s_bLoggedPartGate = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: VehicleManager::StartPartContactGeneration @0x8262C220 "
                       "(114) PARTIAL -- the miFirstPartContactGenEntry boundary stamp is real, "
                       "the part-contact generation tail is not reconstructed [FLAG PC boot "
                       "gate]. Reported once, not per frame\n";
        }
    }

    // ==============================================================================================
    // ⭐⭐ DoRaceCarWorldContactValidation @0x825EB6C8 (416) — REAL as of walls leg 3 (2026-08-14).
    // PS3 DecFIGS 0x70FA74 (804). THE CONDUCTOR GATE OF THIS NAME IS DELETED.
    //
    // Drain the RAW race-car-vs-world queue [5] the harvest just filled; every surviving contact
    // is appended to the VALIDATED queue [6] the bridge (BridgeContactsToSimulation ->
    // ReadPotentialVehicleWorldContact) and crash prediction consume. Per contact:
    //   * assert owners (A == WORLD :1400, B == RACECAR :1401 — the harvest posts (world, car));
    //   * SwapEntityOrder (the committed inline: swap points/ids/tags/indices, negate the normal)
    //     so the CAR is side A downstream;
    //   * assert the car is live in mUsedRaceCars (:1409 "Recieved a potential contact for an
    //     inactive race car");
    //   * NaN-check the normal before (:1410) and after (:1415) validation;
    //   * IsRaceCarCrashing bypasses validation entirely (crashing cars keep every contact);
    //     otherwise ValidateRaceCarWorldContact (@0x825C6088, the slice TU) gates — and may
    //     REWRITE the contact in place (wall-normal flatten / wheel-plane projection);
    //   * append survivors to [6] via the same out-of-line AddEvent(6, ...) shape the console
    //     inlines here (overflow warning with the literal 6 + bounds-gated AddEventSafe).
    // Bracketed by the manager's OWN monitor miRaceCarWorldContactValidationPM (+172460) — the
    // caller's miPhysicsUpdateValidateRaceCarWorldContactPM bracket in PhysicsModule::Update is a
    // SECOND, outer monitor; both are the console's.
    // The DeformationManager* parameter is UNUSED in both console bodies (checked, not assumed).
    // ==============================================================================================
    void VehicleManager::DoRaceCarWorldContactValidation(
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpPotentialContactInterface,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCache,
        f32 lfTimeStep,
        BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/)
    {
        typedef CgsSceneManager::SceneManagerIO::PotentialContact PotentialContact;
        typedef BrnPhysics::PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue Queue;

        CgsDev::PerfMonCpu::StartMonitor(miRaceCarWorldContactValidationPM);     // +172460

        CGS_ASSERT(lpPotentialContactInterface != nullptr, "lpPotentialContactInterface != NULL"); // :1382
        CGS_ASSERT(lpTriCache != nullptr, "lpTriCache != NULL");                                   // :1383
        CGS_ASSERT(lpPotentialContactInterface->GetRaceCarWithWorldQueueValidated().GetLength() == 0,
                   "lpPotentialContactInterface->GetRaceCarWithWorldQueueValidated()->GetLength() == 0"); // :1384

        const Queue& lrRawQueue = lpPotentialContactInterface->GetRaceCarWithWorldQueue();   // [5]
        const s32 liNumContacts = lrRawQueue.GetLength();
        s32 liValidatedContacts = 0;

        for (s32 liContact = 0; liContact < liNumContacts; ++liContact)
        {
            PotentialContact lContact = lrRawQueue.GetEvent(liContact);          // 80-byte copy

            CGS_ASSERT((lContact.muVolumeInstanceIdA.muId >> 56) == 0u,
                       "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_WORLD");   // :1400
            CGS_ASSERT((lContact.muVolumeInstanceIdB.muId >> 56) == 1u,
                       "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :1401

            lContact.SwapEntityOrder();   // (world, car) -> (car, world); normal sign-flips

            const u32 luRaceCarIndex =
                static_cast<u32>(lContact.muVolumeInstanceIdA.muId >> 42) & 0x3FFFu;
            CGS_ASSERT(mUsedRaceCars.IsBitSet(luRaceCarIndex),
                       "Recieved a potential contact for an inactive race car");  // :1409 [sic]

            CGS_ASSERT(lContact.mNormal.x == lContact.mNormal.x
                           && lContact.mNormal.y == lContact.mNormal.y
                           && lContact.mNormal.z == lContact.mNormal.z,
                       "Normal is invalid before call to Validate\n");            // :1410

            // The console's own line-8273 bound rides inside IsRaceCarCrashing (slice TU).
            if (!IsRaceCarCrashing(static_cast<s32>(luRaceCarIndex))
                && !ValidateRaceCarWorldContact(&lContact, lpTriCache, lfTimeStep))
            {
                continue;   // rejected — nothing appended
            }

            CGS_ASSERT(lContact.mNormal.x == lContact.mNormal.x
                           && lContact.mNormal.y == lContact.mNormal.y
                           && lContact.mNormal.z == lContact.mNormal.z,
                       "Normal is invalid after call to Validate\n");             // :1415

            // The console inlines exactly the AddEvent(6, ...) shape here (warning literal 6).
            lpPotentialContactInterface->AddEvent(6u, lContact);
            ++liValidatedContacts;
        }

        // ---- [cvalid] PC bring-up instrument -- DELETE WHEN world collision is proven map-wide ---
        // OPT-IN (BRN_WALL_PROBE=1, the switch that already arms the body-shell trace it pairs
        // with), so a default run and every golden gate stay byte-identical to a build without it.
        //
        // ⭐ WHY HERE. [wall] proved the driven car contributes ZERO world contacts to the
        // penetration solver from the moment it starts moving -- it drives through the wall of a
        // solid structure at 30 m/s without one contact, and the EDGE latch rules out sampling.
        // This line is the SPLIT that says which half is at fault, and it is the only place both
        // numbers exist at once:
        //     raw 0                -> the GENERATION half never produced a candidate;
        //     raw > 0, valid 0     -> ValidateRaceCarWorldContact is eating them.
        // Printed only on frames that actually have raw candidates, so it is near-silent while the
        // car drives on open road (the body shell touches nothing) and dense exactly at an impact.
        {
            static s32 siCValidProbe = -1;
            if (siCValidProbe < 0)
            {
                const char* lpcEnv = getenv("BRN_WALL_PROBE");
                siCValidProbe = (lpcEnv != nullptr && lpcEnv[0] != '0') ? 1 : 0;
            }
            static u32 suCValidFrame = 0u;
            ++suCValidFrame;
            if (siCValidProbe == 1 && liNumContacts > 0 && CgsDev::Log::gpDebugPrint != nullptr)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[cvalid] n " << static_cast<s32>(suCValidFrame)
                    << " raw " << liNumContacts
                    << " valid " << liValidatedContacts << "\n";
            }
        }
        // ---- end [cvalid] ------------------------------------------------------------------------

        // ⭐ [FLAG PC boot witness] one-shot: first frame anything survives validation.
        {
            static bool sbLoggedValidated = false;
            if (!sbLoggedValidated && liValidatedContacts > 0)
            {
                sbLoggedValidated = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "⭐ world contacts VALIDATED: " << static_cast<u32>(liValidatedContacts)
                        << " of " << static_cast<u32>(liNumContacts)
                        << " raw contact(s) accepted into the Validated queue [6] [FLAG PC boot "
                           "witness]. Reported once, not per frame\n";
            }
        }

        CgsDev::PerfMonCpu::StopMonitor(miRaceCarWorldContactValidationPM);
    }
}
}
