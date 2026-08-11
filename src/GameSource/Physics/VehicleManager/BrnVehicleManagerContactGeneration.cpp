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
// ⚠⚠ The four Do*/IsRaceCarHidden callees at the bottom are TRAP STUBS (named, not landed):
//   DoCarCarContactGeneration @0x8261BB38 (250 asm / 15 callees; PS3 0x75C0C8)
//   DoRaceCarWorldContactGeneration @0x825EB140 (131 asm / 14 callees; PS3 0x788190)
//   DoTrafficCarWorldContactGeneration @0x8261BF28 (⚠ .ida-exports HOLE; PS3 0x789760)
//   IsRaceCarHidden @0x825C2EA0 (⚠ .ida-exports HOLE; no PS3 twin surfaced either --
//     signature from the two register-truth call sites)
// RECONSTRUCT-NEXT.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"                               // IOBufferStack::CreateIOBuffer<T>
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamProducer.h"         // SimpleDataStreamProducer (Begin)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h"    // OutOverlapPair (promoted ids)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"          // TriangleCacheInterface
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // CollisionGenerator (+ the collide-stream family)
#include "GameSource/Physics/BrnContactGenerationList.h"                                  // ContactGenList
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"                  // DeformationManager (Add*Pair, FindModelIndexByEntityID, GetDeformableObject)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (GetVehiclePhysics)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (IsFrozen through the ExternallySimulatedBody base)

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
        mpContactGenList->Construct();          // the console CreateIOBuffer runs T::Construct

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
        // ⚠⚠ PC-BUILD GUARD (conductor wave 2026-08-09): the three CreateCollide*Stream
        // factories are still one-shot gates (CgsCollisionGenerator_StreamStubs.cpp) that
        // return NULL, so Begin() here would AV on the first conducted frame (measured:
        // 0xC0000005 at SimpleDataStreamProducer::Begin, boot +8s). Until the stream family
        // lands, a null producer means the collide-stream leg cannot run -- log once, skip
        // the Begin/Run trio, leave the three job pointers null (EndVehicleContactGeneration
        // is itself still a gate, so nothing consumes them yet). GUARD TESTS THE EXACT
        // POINTERS THE STUBS LEAVE NULL. Delete with the stream family.
        if (mpSphereTriangleStreamProducer == 0 || mpSweptSphereTriangleStreamProducer == 0 ||
            mpSphereSphereStreamProducer == 0)
        {
            static bool s_bLogged = false;
            if (!s_bLogged)
            {
                s_bLogged = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "conductor gate: StartVehicleContactGeneration's "
                                                  "collide-stream leg inert [FLAG PC boot gate -- "
                                                  "CreateCollide*Stream stubs returned null]\n";
            }
            mpSphereTriangleStreamJob      = 0;
            mpSweptSphereTriangleStreamJob = 0;
            mpSphereSphereStreamJob        = 0;
            return;
        }
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
        CGS_ASSERT(false, "TRAP: VehicleManager::DoCarCarContactGeneration @0x8261BB38 "
                          "not reconstructed (big-five #2 closure stub)\n");
    }

    void VehicleManager::DoRaceCarWorldContactGeneration(
        s32 /*liRaceCarIndex*/, BrnPhysics::Deformation::DeformationManager* /*lpDeformationManager*/,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* /*lpTriangleCacheInterface*/,
        BrnPhysics::ContactGenList* /*lpContactGenList*/,
        CgsSceneManager::CgsCollision::CollisionGenerator* /*lpContactGenerator*/,
        CgsMemory::SimpleDataStreamProducer* /*lpSphereTriangleStream*/,
        CgsMemory::SimpleDataStreamProducer* /*lpSweptSphereTriangleStream*/,
        u32 /*luQueueIndex*/)
    {
        CGS_ASSERT(false, "TRAP: VehicleManager::DoRaceCarWorldContactGeneration @0x825EB140 "
                          "not reconstructed (big-five #2 closure stub)\n");
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
}
}
