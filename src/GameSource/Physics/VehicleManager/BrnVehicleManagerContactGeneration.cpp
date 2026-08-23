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
// Caller: PhysicsModule::Update @0x825B0640 is a REAL BODY (BrnPhysicsModuleUpdateFunctions.cpp,
// mounted) and it DRIVES THIS TU EVERY NON-CATCHUP FRAME. Everything in here is reachable at
// runtime; this is not a closure-enforcement mount. (This banner said "STILL A LINK STUB" for
// several waves after the caller landed -- gotcha 9 + gotcha 10, and it is why the wave-Q7
// car-car witness looked unreachable to readers.)
//
// ⭐⭐ Callee state (2026-08-14 walls leg 1): DoRaceCarWorldContactGeneration @0x825EB140 is
// REAL (its log-once gate deleted) and the collide-stream family behind it is REAL
// (CgsCollisionGenerator_CollideStreams.cpp) — one sphere/swept command per live car per frame
// now flows into ContactGeneratorJob::Execute, where the type-6/14 workers are LOUD NAMED
// GATES until the sphere-vs-Triangle4 contact kernels land.
//
// ⭐⭐ 2026-08-22 (wave T3, cluster C5): DoTrafficCarWorldContactGeneration @0x8261BF28 (226)
// IS REAL -- the entry that stood here calling it a trap is gone with it (the export hole was
// closed by the wave-T3 scout). Its SWEPT arm is one named gate; see the body. NEW in this TU:
// DoTrafficWorldContactOrdering @0x825C8F18, whose home this file is (its own baked assert
// __FILE__ strings say so, :1458/:1475/:1476/:1493/:1494).
//
// ⭐⭐ 2026-08-19 (wave Q7, cluster `carcar`): DoCarCarContactGeneration @0x8261BB38 (251) IS REAL
// and its log-once gate is DELETED. The entry that stood in the list above ("250 asm / 15 callees;
// PS3 0x75C0C8 — log-once gate") went with it: a banner that still calls a landed body a gate is
// this campaign's most repeated defect. Its sphere-sphere arm is complete end to end — its poster
// AddSphereListWithSphereListToStream @0x828119F0 landed with it, in
// CgsCollisionGenerator_CollideStreams.cpp — so the sphere-sphere collide stream carries real
// commands for the first time. ⭐ Its SIMPLE-TRAFFIC arm is REAL as of wave T3 (2026-08-22): the
// gate's blocker -- "GetSimpleVehicleBox @0x82602A20 has no body in the tree" -- stopped being
// true at 8ea763bc; the arm needed no reconstruction, only the stale banner deleted.
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
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"     // SimpleVehiclePhysics (DoCarCar's GetVehiclePhysics receiver)
#include "rw/math/vpu/vector3_operation.h"                                                // Magnitude / operator- (DoCarCar's relative-speed padding)
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
        //
        // ⚠️ THE LAST TWO ARE USER TAGS, NOT A FLAG WORD (renamed wave Q7, 2026-08-19; they used to
        // be spelled as a "flags" word and a single "tag"). CollidePrimitivePairList's third and fourth
        // parameters are `luUserTagA` (u32) and `lu16UserTagB` (u16) -- the DWARF's own names --
        // and it passes them STRAIGHT THROUGH to PrepareNewPrimitiveTestResultsList, which stores
        // them into the CollisionResultList header as the caller's tags. Nothing in the tree reads
        // either as a bit field. tagA == 11 is how the harvest tells this caller's result lists
        // apart from StartPartContactGeneration's (which passes 3 / 4 / 2).
        const s32 KI_STREAM_MAX_COMMANDS      = 100;
        const u16 KU16_COLLIDE_MAX_RESULTS    = 200;
        const u32 KU_COLLIDE_USER_TAG_A       = 11;    // luUserTagA -- the baked `li r5, 11`
        const u16 KU16_COLLIDE_USER_TAG_B     = 0;     // lu16UserTagB -- the baked `li r6, 0`

        // ⭐ 2026-08-14 (walls leg 1): DoRaceCarWorldContactGeneration's two contact-padding
        // floats, image-read this wave (x360rd): flt_82001D9C == 2.0f rides the in-place sphere
        // pass, flt_82001DA0 == 0.5f the swept (continuous) pass. Both go to the poster's f1 and
        // land in StreamCommand::mfPadding.
        const f32 KF_SPHERE_TRIANGLE_CONTACT_PADDING       = 2.0f;   // flt_82001D9C
        const f32 KF_SWEPT_SPHERE_TRIANGLE_CONTACT_PADDING = 0.5f;   // flt_82001DA0

        // ⭐ 2026-08-19 (wave Q7, cluster `carcar`): DoCarCarContactGeneration's constants.
        // The float is the SAME rodata word flt_82001DA0 (0.5f) the swept padding above reads --
        // one literal in the image, several roles, so each role gets its own name rather than one
        // name that would be a lie at the other sites. (The gated simple-traffic arm reads the
        // same word a third time, as AddPrimitivePair's f1; no constant is minted for it until
        // that arm lands, so nothing here is a promise the code does not keep.)
        const f32 KF_MIN_CARCAR_CONTACT_PADDING  = 0.5f;   // flt_82001DA0 (the fsel floor)

        // DeformableObject::GetWorldSpaceSpheres returns GetNumSensors(), which is the deformation
        // spec's sensor count PLUS FOUR APPENDED WHEEL SPHERES (BrnDeformableObject.h:308-313).
        // Car-vs-car drops those four (`addi r7, r29, -4` twice, 0x8261BCF4 / 0x8261BD34): wheels
        // collide through mDetachedWheelPrimPairBuilder, not through the body-shell sphere test.
        const s32 KI_NUM_APPENDED_WHEEL_SPHERES  = 4;

        // The sphere-sphere command's UserTagB (`li r9, 0` @0x8261BCF8). The world path passes 1
        // -- AddContactResultsToQueue reads UserTagB to choose WHICH side's normal to negate, so
        // the 0 here is load-bearing, not padding: it selects mPrimitive1Normal (the B-car side).
        const u16 KU16_CARCAR_USER_TAG_B         = 0;

        // ContactGenList::AddEntry's two per-side volume-instance base offsets (`li r7, 1 ;
        // li r6, 1` @0x8261BDB0). The harvest adds them to each record's primitive index when it
        // rebuilds the contact's VolumeInstanceIds.
        const u8 KU8_CARCAR_VOL_INST_OFFSET      = 1;

        // ⭐ 2026-08-22 (wave T3, cluster C5). DoCarCarContactGeneration's SIMPLE arm padding --
        // flt_82001DA0 a third time (`lfs f1` @0x8261BF10), its own name per the note above.
        const f32 KF_SIMPLE_TRAFFIC_CONTACT_PADDING = 0.5f;

        // DoTrafficCarWorldContactGeneration @0x8261BF28 constants, read off its asm.
        const s32 KI_TRAFFIC_TRIANGLE_CACHE_BASE     = 8;    // `addi r30, r18, 8` -- cars hold 0..7
        const s32 KI_MAX_TRAFFIC_SPHERE_WORLD_TESTS  = 21;   // `cmpwi cr6, r10, 0x15`
        const u16 KU16_TRAFFIC_BOX_MAX_RESULTS       = 100;  // `li r6, 0x64`
        const u32 KU_TRAFFIC_BOX_WORLD_QUEUE         = 10;   // `li r7, 0xA` == maCustomEventQueues[10]
        const u16 KU16_TRAFFIC_BOX_USER_TAG_B        = 0;    // `li r8, 0`
        const f32 KF_TRAFFIC_BOX_CONTACT_PADDING     = 0.0f; // flt_82001CC0

        // DoTrafficWorldContactOrdering @0x825C8F18 owner tags.
        const u32 KU_OWNER_WORLD                     = 0;    // BrnWorld::E_ENTITYTYPE_WORLD
        const u32 KU_OWNER_TRAFFIC_VEHICLE           = 2;    // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE
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

        // ---- [T4-drop] counters. NOT IN THE X360 BINARY. BRN_TRAFFIC_DIAG. DELETE-WHEN-STABLE. ---
        // The decisive negative witness for wave 4: it splits "traffic has no collision volume,
        // so the scene never emits a pair" (all zero) from "pairs arrive but every traffic half
        // is unpromoted, so the 127 fixup eats them" (pairs != 0, dropped == pairs).
        s32 liDiagPairsTotal      = 0;   // every pair the scene published this frame
        s32 liDiagTrafficPairs    = 0;   // car-car pairs with at least one TRAFFIC half
        s32 liDiagTrafficDropped  = 0;   // ...of those, dropped at the global->physical 127 fixup
        s32 liDiagQueue8Pairs     = 0;   // ...of those, race-car-vs-traffic that reached queue [8]

        // ---- (3) the overlap-pair walk ----------------------------------------------------------
        for (s32 liPair = 0; liPair < lpOverlapPairs->GetLength(); ++liPair)
        {
            ++liDiagPairsTotal;   // [T4-drop] DIAG

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

                if (luOwnerA == 2u || luOwnerB == 2u)
                {
                    ++liDiagTrafficPairs;     // [T4-drop] DIAG
                }

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
                        ++liDiagTrafficDropped;   // [T4-drop] DIAG
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
                        ++liDiagTrafficDropped;   // [T4-drop] DIAG
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

                // ⭐ The trap that used to be documented here is GONE (wave T3, 2026-08-22):
                // AddHingedBodyPartPairs @0x82605A98 -- called unconditionally for EVERY
                // overlapping pair -- and AddRaceCarBodyPartPair @0x82605928 are REAL bodies in
                // BrnDeformationManager_ContactBridges.cpp. AddRaceCarWheelPair @0x82605BE8 is
                // still a NAMED GATE there (one missing cylinder-vs-box appender).
                if (lu16QueueIdx == 8)
                {
                    ++liDiagQueue8Pairs;   // [T4-drop] DIAG
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

        // ---- [T4-drop] the wave-4 split line ---------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG). Latched on the traffic tuple
        // AND on a NEW HIGH-WATER MARK of the total pair count -- the raw total changes almost
        // every frame, so latching on it verbatim would spam, but latching only on the traffic
        // fields froze the line at the pre-gameplay frame ("pairs=0" printed once, 60 s before
        // the car drove, reading as "the scene publishes no pairs at all"). pairsMax is monotone,
        // so a reader can always tell an empty scene from a busy one. Reading it:
        //   pairsMax=0                     -> the scene published no overlap pair AT ALL (look
        //                                     upstream of the physics module entirely).
        //   traffic=0                      -> no traffic half in ANY pair: the scene has no
        //                                     traffic collision volume (gate 1, UpdateCollidableVehicles).
        //   traffic>0 && dropped==traffic  -> pairs arrive but nothing was promoted (gate 2,
        //                                     BuildPotentialCollisionList / HandleHalfPotentialContact).
        //   q8>0                           -> a race-car-vs-traffic pair reached custom queue [8];
        //                                     the ram is live from here on.
        // DELETE-WHEN-STABLE.
        {
            static const bool sbT4Diag = (getenv("BRN_TRAFFIC_DIAG") != 0);
            static s32        siLastTraffic = -1;
            static s32        siLastDropped = -1;
            static s32        siLastQueue8  = -1;
            static s32        siMaxPairs    = -1;
            const bool lbNewPairMax = (liDiagPairsTotal > siMaxPairs);
            if (sbT4Diag && CgsDev::Log::gpDebugPrint != 0
                && (lbNewPairMax
                    || liDiagTrafficPairs != siLastTraffic
                    || liDiagTrafficDropped != siLastDropped
                    || liDiagQueue8Pairs != siLastQueue8))
            {
                if (lbNewPairMax) siMaxPairs = liDiagPairsTotal;
                siLastTraffic = liDiagTrafficPairs;
                siLastDropped = liDiagTrafficDropped;
                siLastQueue8  = liDiagQueue8Pairs;
                *CgsDev::Log::gpDebugPrint
                    << "[T4-drop] overlap pairs=" << liDiagPairsTotal
                    << " pairsMax=" << siMaxPairs
                    << " withTrafficHalf=" << liDiagTrafficPairs
                    << " droppedAt127=" << liDiagTrafficDropped
                    << " reachedQueue8=" << liDiagQueue8Pairs
                    << "\n";
            }
        }

        // ---- (4) the two simple-traffic pair lists + the triangle-cache streams -----------------
        if (mTrafficSimpleTrafficPrimPairBuilder.GetNumTests() != 0)                             // +172558
        {
            mpContactGenerator->CollidePrimitivePairList(&mTrafficSimpleTrafficPrimPairBuilder,
                                                         KU16_COLLIDE_MAX_RESULTS, KU_COLLIDE_USER_TAG_A,
                                                         KU16_COLLIDE_USER_TAG_B);
            mpContactGenList->AddEntry(EntityId{ 0x02000000u }, EntityId{ 0x02000000u }, 0, 0);
        }
        if (mRaceCarSimpleTrafficPrimPairBuilder.GetNumTests() != 0)                             // +172570
        {
            mpContactGenerator->CollidePrimitivePairList(&mRaceCarSimpleTrafficPrimPairBuilder,
                                                         KU16_COLLIDE_MAX_RESULTS, KU_COLLIDE_USER_TAG_A,
                                                         KU16_COLLIDE_USER_TAG_B);
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
            // 2026-08-11, create-drain wave: with the first live car it fired per frame, 178 halts
            // in 80 s, because the model table was then permanently -1).
            // ⭐ BANNER CORRECTED 2026-08-23 (wave 4, cluster B): the reason given here -- "Process-
            // AddDeformationModelEvents lives in the UNMOUNTED BrnDeformationManager.cpp" -- IS NO
            // LONGER TRUE. That TU is mounted (build_game_exe.bat) and the drain is bodied and
            // keyed correctly by the HANDLING BODY id (BrnDeformationManager.cpp:310, the wave-3
            // r3 fix). So this line firing now means a genuinely UNREGISTERED car, not an expected
            // bring-up state -- and it matters to the ram: the same table feeds GetSpheresForCar,
            // whose null result trips DoCarCarContactGeneration's :877/:878 (see [T4-spheres]).
            // RESTORE the console assert once a boot shows this line silent.
            if (liModelIndex == -1)
            {
                static bool sbLoggedNoDeformationModel = false;
                if (!sbLoggedNoDeformationModel)
                {
                    sbLoggedNoDeformationModel = true;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] StartVehicleContactGeneration: no deformation "
                               "model for live race car -- car SKIPPED for world contact "
                               "generation, console assert 'liIndex != -1' degraded to this line. "
                               "Reported once, not per frame\n";
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

    // ==============================================================================================
    // ⭐⭐ DoCarCarContactGeneration @0x8261BB38 (251) — REAL as of wave Q7 (2026-08-19).
    // THE LOG-ONCE GATE THAT WAS HERE ("car-car contacts NOT generated") IS DELETED.
    // PS3 DecFIGS 0x75C0C8 (its mangle is the signature authority; the DWARF also names every
    // local this body uses: lpSpheresA/B, liNumSpheresA/B, lfPadding, lVelocityCarA/B,
    // lbAIsSimpleTraffic/lbBIsSimpleTraffic, lpSimplePhysicsA/B).
    //
    // ONE overlapping car pair per call, per frame, already canonicalised by the driver so a race
    // car is side A. Two arms, chosen by whether EITHER car is a SIMPLE traffic vehicle:
    //   * NEITHER simple (race-car-vs-race-car, and race-car-vs-full-traffic) — post ONE
    //     sphere-list-vs-sphere-list command into this frame's sphere-sphere collide stream from
    //     both cars' deformation spheres MINUS THE LAST FOUR, then mark the pair in the
    //     contact-gen list so the harvest (AddContactResultsToQueue) knows an entry exists for it.
    //   * EITHER simple — box-vs-box into one of the two simple-traffic pair builders, which
    //     StartVehicleContactGeneration collides at the end of its overlap walk. NOT gated: the
    //     arm is real since wave T3 (GetSimpleVehicleBox @0x82602A20 has a body,
    //     BrnSimpleVehiclePhysics.cpp:793) and both end-of-walk drains are live. Wave 4 confirmed
    //     it is also UNREACHABLE by construction: the create path never allocates a SIMPLE slot.
    //
    // ⚠️⚠️ ARGUMENT ORDER IS THE CONSOLE'S, AND THE SLOTS LOOK WRONG AT A GLANCE (gotcha 3).
    // The X360 reads the stream producer as `lwz r10, arg_54` and the queue id as
    // `lhz r8, arg_5E` — a 10-byte gap that reads like a skipped parameter. It is not: the
    // Xbox 360 parameter save area uses 8-BYTE doubleword slots with the value RIGHT-JUSTIFIED
    // (big-endian), so a 4-byte producer sits at slot+4 (0x50+4 == 0x54) and a 2-byte queue id at
    // slot+6 (0x58+6 == 0x5E) — CONSECUTIVE slots, u16 BEFORE the f32, exactly as the DWARF
    // declares and as this header already had it. Calibrated twice before trusting it:
    // DoRaceCarWorldContactGeneration's 8th argument is `lwz arg_54` (same slot 0x50), and this
    // body's own caller spills its 4th register parameter to `arg_34` == slot 0x30 + 4. Read as
    // 4-byte slots instead, the u16 would have to be the 10th parameter and the f32 the 9th —
    // the same reading that produces a silently swapped pair. lfTimeStep is the 10th parameter,
    // rides f1, and its reserved slot (0x60) is never written by the caller.
    //
    // The asm, top to bottom (every callee REAL except the one named in the gated arm):
    //   0x8261BB88  assert !(A is TRAFFIC && B is RACECAR)                            (:794)
    //   0x8261BBC0  lbAIsSimpleTraffic = A is traffic && IsTrafficVehicleSimple(A)    (@0x825B4A98)
    //   0x8261BBF0  lbBIsSimpleTraffic = B is traffic && IsTrafficVehicleSimple(B)
    //   0x8261BC20  GetVehiclePhysics(A) / GetVehiclePhysics(B)                       (@0x825B4F50)
    //   0x8261BC34  BOTH frozen (+0x70 == mbFrozen) -> nothing to generate, return
    //   0x8261BC68  GetSpheresForCar x2 into the two zero-seeded out slots (@0x825C2260),
    //               assert lpSpheresA (:877) / lpSpheresB (:878) — the getter returns -1
    //               gracefully WITHOUT writing the slot, so these are the caller's null tests
    //   0x8261BD08  padding = Magnitude(velB - velA) * lfTimeStep, floored at 0.5f by an `fsel`
    //   0x8261BDAC  AddSphereListWithSphereListToStream(A, B, 200, padding, queueId, 0, stream)
    //   0x8261BDC4  ContactGenList::AddEntry(lCarIdA, lCarIdB, 1, 1)                  (@0x825B59B0)
    //   0x8261BDD4  the SIMPLE arm: GetSimpleVehicleBox x2, the self-overlap assert (:857), then
    //               AddPrimitivePair(boxA, boxB, 0.5f, indexA, indexB) into the builder the
    //               owner byte selects (+172564 race-car-vs-simple, +172552 traffic-vs-simple)
    //
    // ⭐ WHAT THIS CHANGES AT RUNTIME, PLAINLY: the sphere-sphere collide stream carried ZERO
    // commands for the whole campaign because this producer was a gate. It now carries one per
    // overlapping car pair per frame, so RunCollideSphereListWithSphereListStream stops
    // early-returning null and the type-8 worker actually runs. Nothing else in the chain moved.
    // ==============================================================================================
    void VehicleManager::DoCarCarContactGeneration(
        CgsSceneManager::EntityId lCarIdA, CgsSceneManager::EntityId lCarIdB,
        CgsSceneManager::EntityId lCarPhysicsIdA, CgsSceneManager::EntityId lCarPhysicsIdB,
        BrnPhysics::Deformation::DeformationManager* lpDefMan,
        BrnPhysics::ContactGenList* lpContactGenList,
        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
        CgsMemory::SimpleDataStreamProducer* lpSphereSphereStream,
        u16 lu16QueueID, f32 lfTimeStep)
    {
        typedef CgsSceneManager::CgsCollision::SphereList SphereList;

        // The driver canonicalises every pair so a race car is side A; this is the console's own
        // tripwire that it did (0x8261BB88).
        CGS_ASSERT(!(lCarPhysicsIdA.GetOwner() == 2u && lCarPhysicsIdB.GetOwner() == 1u),
                   "!( lCarPhysicsIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && "
                   "lCarPhysicsIdB.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR )");          // :794

        // A SIMPLE traffic vehicle carries no deformation spheres -- it collides as one oriented
        // box. The owner test short-circuits the manager call exactly as the asm does.
        const bool lbAIsSimpleTraffic =
            (lCarPhysicsIdA.GetOwner() == 2u)                                     // E_ENTITYTYPE_TRAFFIC_VEHICLE
            && mPhysicalTrafficManager.IsTrafficVehicleSimple(
                   EntityId{ static_cast<u32>(lCarPhysicsIdA) });
        const bool lbBIsSimpleTraffic =
            (lCarPhysicsIdB.GetOwner() == 2u)
            && mPhysicalTrafficManager.IsTrafficVehicleSimple(
                   EntityId{ static_cast<u32>(lCarPhysicsIdB) });

        // GetVehiclePhysics returns the shared SimpleVehiclePhysics base of both branch types
        // (RaceCarPhysics : VehiclePhysics : SimpleVehiclePhysics, and the traffic manager's own
        // SimpleVehiclePhysics) -- which is exactly what the DWARF types it as, and why the body
        // below can read mbFrozen / mLinearVelocity off either without a per-branch test. The
        // in-tree accessor still returns void* (see BrnVehicleManager.h); the cast is the caller's.
        SimpleVehiclePhysics* lpSimplePhysicsA = static_cast<SimpleVehiclePhysics*>(
            GetVehiclePhysi(EntityId{ static_cast<u32>(lCarPhysicsIdA) }));
        SimpleVehiclePhysics* lpSimplePhysicsB = static_cast<SimpleVehiclePhysics*>(
            GetVehiclePhysi(EntityId{ static_cast<u32>(lCarPhysicsIdB) }));

        // 0x8261BC34: two frozen bodies cannot generate a contact worth solving. (The asm nests
        // the whole body under `!(A->mbFrozen && B->mbFrozen)`; the early return is the same.)
        if (lpSimplePhysicsA->IsFrozen() && lpSimplePhysicsB->IsFrozen())
        {
            return;
        }

        if (!lbAIsSimpleTraffic && !lbBIsSimpleTraffic)
        {
            // ---- the SPHERE-SPHERE arm (0x8261BC68..0x8261BDC4) ---------------------------------
            const CgsGeometric::Sphere* lpSpheresA = 0;   // 0x8261BB64 stw 0 -- the callee may not
            const CgsGeometric::Sphere* lpSpheresB = 0;   // 0x8261BB6C stw 0    write these slots
            const s32 liNumSpheresA = lpDefMan->GetSpheresForCar(
                EntityId{ static_cast<u32>(lCarPhysicsIdA) }, &lpSpheresA);
            const s32 liNumSpheresB = lpDefMan->GetSpheresForCar(
                EntityId{ static_cast<u32>(lCarPhysicsIdB) }, &lpSpheresB);
            // ---- [T4-spheres] the assert's cause, named BEFORE it fires -----------------------
            // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG), one-shot. A freshly
            // promoted traffic car whose AddDeformationModel event has not been drained yet has
            // no model, GetSpheresForCar returns -1 and leaves the out pointer null, and :877/:878
            // fire. On the console the model is always registered first. DELETE-WHEN-STABLE.
            if (lpSpheresA == 0 || lpSpheresB == 0)
            {
                static const bool sbT4Diag  = (getenv("BRN_TRAFFIC_DIAG") != 0);
                static bool       sbReported = false;
                if (sbT4Diag && !sbReported && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbReported = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[T4-spheres] no deformation model for a car-car pair: A=" << CgsDev::E_PRINTMODE_HEXONCE
                        << static_cast<u32>(lCarPhysicsIdA) << " nA=" << liNumSpheresA
                        << " B=" << CgsDev::E_PRINTMODE_HEXONCE << static_cast<u32>(lCarPhysicsIdB) << " nB=" << liNumSpheresB
                        << " queue=" << static_cast<s32>(lu16QueueID) << "\n";
                }
            }

            CGS_ASSERT(lpSpheresA != nullptr, "lpSpheresA");                                  // :877
            CGS_ASSERT(lpSpheresB != nullptr, "lpSpheresB");                                  // :878

            SphereList lSphereListA;
            lSphereListA.mpSpheres    = reinterpret_cast<u8*>(
                const_cast<CgsGeometric::Sphere*>(lpSpheresA));
            lSphereListA.miNumSpheres = liNumSpheresA - KI_NUM_APPENDED_WHEEL_SPHERES;

            SphereList lSphereListB;
            lSphereListB.mpSpheres    = reinterpret_cast<u8*>(
                const_cast<CgsGeometric::Sphere*>(lpSpheresB));
            lSphereListB.miNumSpheres = liNumSpheresB - KI_NUM_APPENDED_WHEEL_SPHERES;

            // ---- [T4-normal] the sphere-count witness for the same hop ---------------------
            // [DIAG] NOT IN THE X360 BINARY. Opt-in, value-latched per queue. The worker masks
            // both counts to u16, so a car whose deformation model carries <= 4 spheres would
            // walk 65532+ garbage spheres and hand back garbage normals. DELETE-WHEN-STABLE.
            {
                static const bool sbT4Diag = (getenv("BRN_TRAFFIC_DIAG") != 0);
                static s32        siLastA  = -9999;
                static s32        siLastB  = -9999;
                if (sbT4Diag && CgsDev::Log::gpDebugPrint != 0
                    && (lSphereListA.miNumSpheres != siLastA || lSphereListB.miNumSpheres != siLastB))
                {
                    siLastA = lSphereListA.miNumSpheres;
                    siLastB = lSphereListB.miNumSpheres;
                    *CgsDev::Log::gpDebugPrint
                        << "[T4-normal] car-car sphere lists: queue=" << static_cast<s32>(lu16QueueID)
                        << " rawA=" << liNumSpheresA << " rawB=" << liNumSpheresB
                        << " testedA=" << siLastA << " testedB=" << siLastB << "\n";
                }
            }

            // The contact padding is HOW FAR THE TWO CARS CLOSE ON EACH OTHER THIS STEP: the
            // relative-velocity magnitude times the step, so a fast approach fattens the spheres
            // enough that the pair is not missed between frames. Floored at 0.5f.
            const Vector3 lVelocityCarA = lpSimplePhysicsA->GetLinearVelocity();   // lvx [A+0x50]
            const Vector3 lVelocityCarB = lpSimplePhysicsB->GetLinearVelocity();   // lvx [B+0x50]
            f32 lfPadding =
                rw::math::vpu::Magnitude(lVelocityCarB - lVelocityCarA) * lfTimeStep;

            // 0x8261BDA4  fsubs f12, 0.5f, f13 ; fsel f1, f12, 0.5f, f13.
            // ⚠️ NaN POLARITY (gotcha 4): `fsel` takes the FALSE arm for a NaN condition, so a
            // NaN padding must survive as NaN. Spelled as the subtraction test rather than
            // `lfPadding > 0.5f ? lfPadding : 0.5f`, which would silently return 0.5f on NaN.
            if ((KF_MIN_CARCAR_CONTACT_PADDING - lfPadding) >= 0.0f)
            {
                lfPadding = KF_MIN_CARCAR_CONTACT_PADDING;
            }

            lpContactGenerator->AddSphereListWithSphereListToStream(
                &lSphereListA, &lSphereListB,
                KU16_COLLIDE_MAX_RESULTS,        // li r6, 0xC8
                lfPadding,                       // f1
                lu16QueueID,                     // lhz r8, arg_5E -- the driver's 7 / 8 / 13
                KU16_CARCAR_USER_TAG_B,          // li r9, 0
                lpSphereSphereStream);           // lwz r10, arg_54

            // 0x8261BDC4: the pair's marker entry, carrying the ORIGINAL (global) ids -- entry i
            // belongs to result list i, which is the lockstep AddContactResultsToQueue walks.
            lpContactGenList->AddEntry(EntityId{ static_cast<u32>(lCarIdA) },
                                       EntityId{ static_cast<u32>(lCarIdB) },
                                       KU8_CARCAR_VOL_INST_OFFSET, KU8_CARCAR_VOL_INST_OFFSET);

            // ---- [Q7-carcar] one-shot bring-up witness --------------------------------------
            // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_PROP_DIAG), one-shot, never per frame.
            {
                static const bool sbPropDiag  = (getenv("BRN_PROP_DIAG") != 0);
                static bool       sbFirstPair = true;
                if (sbPropDiag && sbFirstPair && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbFirstPair = false;
                    *CgsDev::Log::gpDebugPrint
                        << "[Q7-carcar] first car-car pair A=" << static_cast<u32>(lCarPhysicsIdA)
                        << " B=" << static_cast<u32>(lCarPhysicsIdB)
                        << " spheres=" << static_cast<s32>(lSphereListA.miNumSpheres
                                                           + lSphereListB.miNumSpheres)
                        << " pairs=0"        // the sphere arm posts a stream command, not a pair
                        << " arm=sphere-sphere queue=" << static_cast<s32>(lu16QueueID)
                        << "\n";
                }
            }
        }
        else
        {
            // ---- the SIMPLE-TRAFFIC arm (0x8261BDD4..0x8261BF14) --------------------------------
            // ⭐⭐ RESTORED 2026-08-22 (wave T3, cluster C5). The log-once gate that stood here is
            // DELETED with the banner it rested on: that banner said SimpleVehiclePhysics::
            // GetSimpleVehicleBox @0x82602A20 was "declared-only ... with NO definition anywhere in
            // the tree". True on 2026-08-19, FALSE since 8ea763bc -- it is bodied and mounted at
            // BrnSimpleVehiclePhysics.cpp:793, signature `void GetSimpleVehicleBox(CgsGeometric::Box&)
            // const`. NOTHING ELSE in this arm was ever missing; no reconstruction was needed.
            //
            // Box-vs-box into the builder the A-side owner selects (both are Prepared every frame by
            // StartVehicleContactGeneration and collided at the end of its walk):
            //   owner A == RACECAR -> +172564 mRaceCarSimpleTrafficPrimPairBuilder (`addi r3,r3,-0x5DEC`)
            //   otherwise          -> +172552 mTrafficSimpleTrafficPrimPairBuilder (`addi r3,r3,-0x5DF8`)
            // Tags are the two 14-bit entity indices (`extrwi ...,14,8`; A -> r7, B -> r8).
            CgsGeometric::Box lSimpleBoxA;
            CgsGeometric::Box lSimpleBoxB;
            lpSimplePhysicsA->GetSimpleVehicleBox(lSimpleBoxA);   // 0x8261BDDC
            lpSimplePhysicsB->GetSimpleVehicleBox(lSimpleBoxB);   // 0x8261BDE8

            const u16 lu16IndexA = lCarPhysicsIdA.GetEntityIndex();
            const u16 lu16IndexB = lCarPhysicsIdB.GetEntityIndex();

            CgsSceneManager::CgsCollision::PrimitivePairListBuilder* lpPairBuilder =
                &mRaceCarSimpleTrafficPrimPairBuilder;
            if (lCarPhysicsIdA.GetOwner() != 1u)   // not E_ENTITYTYPE_RACECAR -> traffic vs traffic
            {
                // The console streams both ids, both indices and both physics ids into the message;
                // lowered to the static prefix per the standing project rule.
                CGS_ASSERT(lu16IndexA != lu16IndexB,
                           "Traffic vehicle overlapped with itself: A) ID ");                 // :857
                lpPairBuilder = &mTrafficSimpleTrafficPrimPairBuilder;
            }

            lpPairBuilder->AddPrimitivePair(&lSimpleBoxA, &lSimpleBoxB,
                                            KF_SIMPLE_TRAFFIC_CONTACT_PADDING,
                                            lu16IndexA, lu16IndexB);                  // 0x8261BF14
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

    // ==============================================================================================
    // DoTrafficCarWorldContactGeneration @0x8261BF28 (226) -- REAL as of wave T3 (2026-08-22).
    // THE CGS_ASSERT(false) TRAP THAT WAS HERE IS DELETED. It was armed: the mounted per-traffic
    // loop above calls this for every bit of mUsedTrafficVehicles, so it fired the frame the first
    // physical traffic body appeared. Body from the export hole closed by the wave-T3 scout
    // (wave3/scout/holes/0x8261BF28.txt: pseudocode + full asm).
    //
    // One live physical traffic car per call, per frame. THREE arms, and only the middle one needs
    // the deformation model:
    //   SIMPLE (mu8PhysicalType == 1)   -> GetSimpleVehicleBox into a 1-slot local pair builder,
    //                                      CollidePrimitiveListAgainstTriangleList (tag 10).
    //   FULL, sphere budget left        -> the deformation spheres into this frame's sphere-triangle
    //                                      collide stream (tag == the caller's queue index, 9).
    //   FULL, budget exhausted (>= 21)  -> the deformed BBox into the same 1-slot builder path, and
    //                                      mbUsingBoxWithWorld is latched TRUE for this frame.
    // Then ONE ContactGenList marker entry: side A owner 0 (WORLD), side B the car's GLOBAL id
    // (not the physical one -- GetGlobalTrafficEntityId @0x8261C288), flags (0, 1).
    //
    // The asm, top to bottom:
    //   0x8261BF64  assert lpMalloc != NULL                                              (:1108)
    //   0x8261BF98  PhysicalTrafficManager::GetPhysicsEntityId(index)      (this+44768 == the mgr)
    //   0x8261BFAC  the traffic-vehicle accessor (IDA GetTrafficInterest_0 == an ICF fold)
    //   0x8261BFEC  the inlined TriangleCacheInterface::GetCache(index + 8) + its
    //               "mpTriangleCacheManager != NULL" tripwire, then GetNumCachedTriangleBatches
    //   0x8261C008  assert lpCachedTriangles                                             (:1128)
    //   0x8261C02C  read mu8PhysicalType (+0x32), CLEAR mbUsingBoxWithWorld (+0x34), then the
    //               type tripwire (BrnPhysicalTrafficVehicle.h:382) -- that store order is the asm's
    //   0x8261C060  the three arms; 0x8261C288 the marker entry
    //
    // EVERY per-frame contact generator appends EXACTLY ONE ContactGenList entry and carves EXACTLY
    // ONE CollisionResultList, and AddContactResultsToQueue walks the two in lockstep. Both early
    // returns below therefore return BEFORE the marker entry, never between the two.
    // ==============================================================================================
    void VehicleManager::DoTrafficCarWorldContactGeneration(
        s32 liTrafficIndex, BrnPhysics::Deformation::DeformationManager* lpDeformationManager,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriangleCacheInterface,
        BrnPhysics::ContactGenList* lpContactGenList,
        CgsSceneManager::CgsCollision::CollisionGenerator* lpContactGenerator,
        CgsMemory::SimpleDataStreamProducer* lpSphereTriangleStream,
        u32 luQueueIndex, CgsMemory::LinearMalloc* lpLinearMalloc)
    {
        typedef CgsSceneManager::CgsCollision::TriangleList TriangleList;
        typedef CgsSceneManager::CgsCollision::SphereList   SphereList;
        typedef CgsSceneManager::CgsCollision::PrimitivePairListBuilder PrimitivePairListBuilder;

        CGS_ASSERT(lpLinearMalloc != nullptr, "lpMalloc != NULL");                            // :1108

        const EntityId lPhysicsEntityId =
            mPhysicalTrafficManager.GetPhysicsEntityId(liTrafficIndex);
        PhysicalTrafficVehicle* lpTrafficVehicle =
            mPhysicalTrafficManager.GetTrafficVehicle(liTrafficIndex);

        // Traffic rides the shared triangle cache at slot 8 + index; race cars hold 0..7. GetCache
        // inlines the "mpTriangleCacheManager != NULL" tripwire the asm carries at 0x8261BFBC.
        const s32 liCacheSlot = liTrafficIndex + KI_TRAFFIC_TRIANGLE_CACHE_BASE;
        const CgsGeometric::Triangle4* lpCachedTriangles = lpTriangleCacheInterface->GetCache(liCacheSlot);
        const s32 liNumTriangleBatches =
            lpTriangleCacheInterface->GetNumCachedTriangleBatches(liCacheSlot);
        CGS_ASSERT(lpCachedTriangles != nullptr, "lpCachedTriangles");                        // :1128

        const u8 lu8PhysicalType = lpTrafficVehicle->mu8PhysicalType;              // lbz 0x32(r29)
        lpTrafficVehicle->mbUsingBoxWithWorld = false;                             // stb 0x34(r29)
        CGS_ASSERT(lu8PhysicalType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");     // BrnPhysicalTrafficVehicle.h:382

        TriangleList lTriangleList;
        lTriangleList.mpTriangles    = const_cast<CgsGeometric::Triangle4*>(lpCachedTriangles);
        lTriangleList.miNumTriangles = liNumTriangleBatches;

        if (lu8PhysicalType == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE)
        {
            // ---- the SIMPLE arm (0x8261C064..0x8261C0CC) -- no deformation model involved ------
            CgsGeometric::Box lBox;
            lpTrafficVehicle->mpVehicleBody->GetSimpleVehicleBox(lBox);            // lwz 0x1C(r29)

            PrimitivePairListBuilder lBuilder;   // Prepare seeds all four base fields (no Construct)
            lBuilder.Prepare(lpLinearMalloc, 1);
            lBuilder.AddPrimitive(&lBox, KF_TRAFFIC_BOX_CONTACT_PADDING, 0);       // sub_82814570
            lTriangleList.CheckAlignment();
            lTriangleList.ValidateTriangles();
            lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
                &lBuilder, &lTriangleList, KU16_TRAFFIC_BOX_MAX_RESULTS,
                KU_TRAFFIC_BOX_WORLD_QUEUE, KU16_TRAFFIC_BOX_USER_TAG_B, true);    // li r9, 1
        }
        else
        {
            // ---- the FULL arms (0x8261C0D4..0x8261C278) -- deformation-model driven ------------
            // [FLAG PC bring-up] HOST BOUNDS GUARD, same shape and reason as the race-car one in
            // StartVehicleContactGeneration above. DeformationManager::IsUsingSweptSpheres /
            // GetSweptSpheresForCar / GetDeformedBBox all index mpaModels[FindModelIndexByEntityID]
            // after a FIRE-AND-CONTINUE assert, so a promoted traffic car with no registered
            // deformation model reads mpaModels[-1] on the host. The console always has the model.
            // RESTORE the unguarded path WHEN the traffic create path (wave T3 C2) is confirmed to
            // register a deformation model for every promoted car. Returns before the marker entry.
            if (lpDeformationManager->FindModelIndexByEntityID(lPhysicsEntityId) == -1)
            {
                static bool sbLoggedNoTrafficDeformationModel = false;
                if (!sbLoggedNoTrafficDeformationModel)
                {
                    sbLoggedNoTrafficDeformationModel = true;
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] DoTrafficCarWorldContactGeneration: no deformation "
                               "model for physical traffic car -- car SKIPPED for world contact "
                               "generation. Reported once, not per frame\n";
                }
                return;
            }

            if (miNumTrafficSphereWorldTests < KI_MAX_TRAFFIC_SPHERE_WORLD_TESTS)   // cmpwi 0x15
            {
                ++miNumTrafficSphereWorldTests;                                     // stw @0x8261C168

                if (!lpDeformationManager->IsUsingSweptSpheres(lPhysicsEntityId))
                {
                    // in-place spheres -> this frame's sphere-triangle collide stream
                    const CgsGeometric::Sphere* lpSpheres = 0;   // 0x8261C174 stw 0 -- the getter
                    const s32 liNumSpheres =                     // may not write the slot
                        lpDeformationManager->GetSpheresForCar(lPhysicsEntityId, &lpSpheres);
                    CGS_ASSERT(lpSpheres != nullptr, "lpSpheres");                        // :1205

                    SphereList lSphereList;
                    lSphereList.mpSpheres    = reinterpret_cast<u8*>(
                        const_cast<CgsGeometric::Sphere*>(lpSpheres));
                    lSphereList.miNumSpheres = liNumSpheres;

                    lTriangleList.CheckAlignment();
                    lTriangleList.ValidateTriangles();
                    lpContactGenerator->AddSphereListWithTriangleListToStream(
                        &lSphereList, &lTriangleList,
                        KU16_COLLIDE_MAX_RESULTS,                     // li r6, 0xC8
                        KF_SPHERE_TRIANGLE_CONTACT_PADDING,           // flt_82001D9C (2.0f)
                        luQueueIndex,                                 // r8 == the baked li 9
                        1,                                            // li r9, 1
                        lpSphereTriangleStream);                      // mr r10, r20
                }
                else
                {
                    // GATE: DoTrafficCarWorldContactGeneration @0x8261BF28 SWEPT arm (0x8261C208).
                    // Blocker: BaseCollisionGenerator::CollideSweptSphereListWithTriangleList
                    // @0x82814080 (this body is its ONLY caller) is neither declared nor bodied;
                    // home CgsCollisionGenerator.h/.cpp, not this cluster's. DELETE-WHEN it lands
                    // (args: swept list, tri list, 200, flt_82001CC0 0.0f, queue index, 1).
                    static bool sbLoggedSweptTrafficGate = false;
                    if (!sbLoggedSweptTrafficGate)
                    {
                        sbLoggedSweptTrafficGate = true;
                        if (CgsDev::Message::gxMessageFilterFlags & 1)
                            *CgsDev::Log::gpDebugPrint
                                << "conductor gate: DoTrafficCarWorldContactGeneration @0x8261BF28 "
                                   "SWEPT arm reached but not landed -- needs BaseCollisionGenerator::"
                                   "CollideSweptSphereListWithTriangleList @0x82814080 [FLAG PC boot "
                                   "gate]. Reported once, not per frame\n";
                    }
                    return;   // before the marker entry -- the lockstep note in the banner
                }
            }
            else
            {
                // sphere budget spent this frame: one deformed BBox against the cache instead
                CgsGeometric::Box lBox;
                lpDeformationManager->GetDeformedBBox(lPhysicsEntityId, &lBox);

                PrimitivePairListBuilder lBuilder;
                lBuilder.Prepare(lpLinearMalloc, 1);
                lBuilder.AddPrimitive(&lBox, KF_TRAFFIC_BOX_CONTACT_PADDING, 0);
                lTriangleList.CheckAlignment();
                lTriangleList.ValidateTriangles();
                lpContactGenerator->CollidePrimitiveListAgainstTriangleList(
                    &lBuilder, &lTriangleList, KU16_TRAFFIC_BOX_MAX_RESULTS,
                    KU_TRAFFIC_BOX_WORLD_QUEUE, KU16_TRAFFIC_BOX_USER_TAG_B, true);
                lpTrafficVehicle->mbUsingBoxWithWorld = true;                     // stb 1, 0x34(r29)
            }
        }

        // 0x8261C288..0x8261C2A4: the WORLD marker entry, carrying the car's GLOBAL entity id.
        lpContactGenList->AddEntry(
            EntityId{ 0u },
            mPhysicalTrafficManager.GetGlobalTrafficEntityId(static_cast<u16>(liTrafficIndex)),
            0, 1);

        // ---- [T3-contact] one-shot bring-up witness ------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG), one-shot. DELETE-WHEN-STABLE.
        {
            static const bool sbTrafficDiag = (getenv("BRN_TRAFFIC_DIAG") != 0);
            static bool       sbFirstTraffic = true;
            if (sbTrafficDiag && sbFirstTraffic && CgsDev::Log::gpDebugPrint != 0)
            {
                sbFirstTraffic = false;
                *CgsDev::Log::gpDebugPrint
                    << "[T3-contact] first traffic world contact gen slot=" << liTrafficIndex
                    << " physType=" << static_cast<s32>(lu8PhysicalType)
                    << " batches=" << liNumTriangleBatches
                    << " queue=" << static_cast<s32>(luQueueIndex)
                    << "\n";
            }
        }
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

                // ---- [T4-normal] the un-normalised-contact hop, NAMED ------------------------
                // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG), one-shot per queue.
                // The bridge asserts |mNormal| == 1 on queues [7]/[8]/[13]
                // (BrnPhysicsModuleBridgeFunctions.cpp:868/:893/:925) and the deformation sensor
                // repeats it (BrnDeformationSensor_ValidateAndAddContact.cpp:107). This is the
                // LAST place the value is ours before it leaves the vehicle manager, so it prints
                // BOTH candidate source normals and the record's own indices: that separates
                // "the worker wrote a bad normal" from "the harvest picked the wrong record".
                // DELETE-WHEN-STABLE.
                {
                    const f32 lfLenSq = lContact.mNormal.x * lContact.mNormal.x
                                      + lContact.mNormal.y * lContact.mNormal.y
                                      + lContact.mNormal.z * lContact.mNormal.z;
                    if (lfLenSq < 0.98f || lfLenSq > 1.02f || lfLenSq != lfLenSq)
                    {
                        static const bool sbT4Diag = (getenv("BRN_TRAFFIC_DIAG") != 0);
                        static u32        suReportedQueues = 0;
                        const u32         luQueueBit =
                            1u << (lResultList.mu32UserTagA & 31u);
                        if (sbT4Diag && (suReportedQueues & luQueueBit) == 0
                            && CgsDev::Log::gpDebugPrint != 0)
                        {
                            suReportedQueues |= luQueueBit;
                            const f32 lfLen0 = lrRecord.mPrimitive0Normal.x * lrRecord.mPrimitive0Normal.x
                                             + lrRecord.mPrimitive0Normal.y * lrRecord.mPrimitive0Normal.y
                                             + lrRecord.mPrimitive0Normal.z * lrRecord.mPrimitive0Normal.z;
                            const f32 lfLen1 = lrRecord.mPrimitive1Normal.x * lrRecord.mPrimitive1Normal.x
                                             + lrRecord.mPrimitive1Normal.y * lrRecord.mPrimitive1Normal.y
                                             + lrRecord.mPrimitive1Normal.z * lrRecord.mPrimitive1Normal.z;
                            *CgsDev::Log::gpDebugPrint
                                << "[T4-normal] non-unit contact normal leaving AddContactResultsToQueue:"
                                << " queue=" << static_cast<s32>(lResultList.mu32UserTagA)
                                << " tagB=" << static_cast<s32>(lResultList.mu16UserTagB)
                                << " type=" << static_cast<s32>(lResultList.meResultType)
                                << " entry=" << liEntry
                                << " result=" << static_cast<s32>(lu16Result)
                                << "/" << static_cast<s32>(lu16NumResults)
                                << " max=" << static_cast<s32>(lResultList.mu16MaxNumResults)
                                << " |n|^2=" << lfLenSq
                                << " n0=(" << lrRecord.mPrimitive0Normal.x << ","
                                << lrRecord.mPrimitive0Normal.y << ","
                                << lrRecord.mPrimitive0Normal.z << ") |n0|^2=" << lfLen0
                                << " n1=(" << lrRecord.mPrimitive1Normal.x << ","
                                << lrRecord.mPrimitive1Normal.y << ","
                                << lrRecord.mPrimitive1Normal.z << ") |n1|^2=" << lfLen1
                                << " primIdx=" << static_cast<s32>(lrRecord.muPrimitive0Index)
                                << "/" << static_cast<s32>(lrRecord.muPrimitive1Index)
                                << " ownerA=" << static_cast<s32>(GetWordOwner(luEntityWordA))
                                << " ownerB=" << static_cast<s32>(GetWordOwner(luEntityWordB))
                                << "\n";
                        }
                    }
                }

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
    //      ⭐ THE GEOMETRY BLOCKER IS GONE (wave Q7, 2026-08-19). This used to read "CgsGeometric::
    //      Box / Box::Set @0x825E6918 / BoxOverlappingTest (PS3 0x7A7B18) are also absent from the
    //      tree; reconstruct them WITH this tail". All three now exist: Box::Set is a real inline
    //      in CgsBox.h and both BoxOverlappingTest overloads (@0x825BEFA0 / @0x825BF090) are real
    //      bodies in CgsBox.cpp. The ONLY remaining blocker on this tail is the network bookkeeping
    //      path itself -- mHiddenRaceCars / mRaceCarsAddedForCollision /
    //      mNetworkCarsAddedForCollisionThisFrame / mNetworkCarsRecievedFirstUpdate /
    //      mauNetworkCarHiddenFramesRemaining / mbPlayerCarInJunkYard.
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
                           "-- car NOT unhidden [FLAG PC boot gate]. The geometry it needs is real "
                           "(Box::Set + both BoxOverlappingTest overloads); what is missing is the "
                           "network bookkeeping path. Reported once, not per frame\n";
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

    // ==============================================================================================
    // DoTrafficWorldContactOrdering @0x825C8F18 (143) -- NEW, wave T3 (2026-08-22, cluster C5).
    // Called EVERY FRAME from PhysicsModule::Update; the conductor gate at
    // BrnPhysicsConductorGates.cpp:281 retires with this body.
    //
    // HOME NOTE: this is BrnVehicleManagerContactGeneration.cpp, not a new slice. The cluster brief
    // said the home was the unmounted BrnVehicleManager.cpp; the console disagrees -- all four of
    // this body's baked assert __FILE__ strings are
    // ".../Physics/VehicleManager/BrnVehicleManagerContactGeneration.cpp" (:1458/:1475/:1476/:1493/
    // :1494), and progress/identity.json gives the same primary_file. No new bat line is needed.
    //
    // WHAT IT DOES: every traffic-vs-world contact is PRODUCED with the WORLD as side A --
    // DoTrafficCarWorldContactGeneration's marker entry is AddEntry(EntityId{0}, trafficGlobalId,
    // 0, 1), so the harvest posts (WORLD, TRAFFIC) records. The solver wants the vehicle first, so
    // this pass walks BOTH traffic-world queues and swaps every record end for end, negating the
    // normal with it. That is exactly PotentialContact::SwapEntityOrder (header-inline,
    // CgsPotentialContact.h:60) -- the X360 inlines it here as the vxor 0x80000000 splat plus the
    // lvx/stvx + ld/std + lwz/stw + lhz/sth field exchange, one for one.
    //
    // THE TWO QUEUES, index-proven off the asm's own base offsets:
    //   a2 + 1474720 == 16 +  9 * 0x28010  -> maCustomEventQueues[9]  GetTrafficWithWorldQueue
    //   a2 + 1638576 == 16 + 10 * 0x28010  -> maCustomEventQueues[10] GetSimpleTrafficWithWorldQueue
    // [10] is the box arm's queue: DoTrafficCarWorldContactGeneration's two
    // CollidePrimitiveListAgainstTriangleList calls pass user tag A == 10 (li r7, 0xA), [9] is the
    // sphere arm's (StartVehicleContactGeneration's baked li 9). The two loops are the console's own
    // two statements, kept separate so each keeps its own assert line pair.
    // ==============================================================================================
    void VehicleManager::DoTrafficWorldContactOrdering(
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContactInterface)
    {
        typedef CgsSceneManager::SceneManagerIO::PotentialContact PotentialContact;
        typedef BrnPhysics::PhysicsModuleIO::PotentialContactInterface::CustomPotentialContactQueue
                CustomPotentialContactQueue;

        CGS_ASSERT(lpContactInterface != nullptr, "lpPotentialContactInterface != NULL");   // :1458

        CustomPotentialContactQueue& lrSphereQueue = lpContactInterface->GetTrafficWithWorldQueue();
        for (s32 liContact = 0; liContact < lrSphereQueue.GetLength(); ++liContact)
        {
            PotentialContact& lrContact = lrSphereQueue.GetEvent(liContact);
            CGS_ASSERT(lrContact.muVolumeInstanceIdA.GetEntityIDOwner() == KU_OWNER_WORLD,
                       "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == "
                       "BrnWorld::E_ENTITYTYPE_WORLD");                                     // :1475
            CGS_ASSERT(lrContact.muVolumeInstanceIdB.GetEntityIDOwner() == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");                           // :1476
            lrContact.SwapEntityOrder();
        }

        CustomPotentialContactQueue& lrBoxQueue = lpContactInterface->GetSimpleTrafficWithWorldQueue();
        for (s32 liContact = 0; liContact < lrBoxQueue.GetLength(); ++liContact)
        {
            PotentialContact& lrContact = lrBoxQueue.GetEvent(liContact);
            CGS_ASSERT(lrContact.muVolumeInstanceIdA.GetEntityIDOwner() == KU_OWNER_WORLD,
                       "lContact.muVolumeInstanceIdA.GetEntityId().GetOwner() == "
                       "BrnWorld::E_ENTITYTYPE_WORLD");                                     // :1493
            CGS_ASSERT(lrContact.muVolumeInstanceIdB.GetEntityIDOwner() == KU_OWNER_TRAFFIC_VEHICLE,
                       "lContact.muVolumeInstanceIdB.GetEntityId().GetOwner() == "
                       "BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");                           // :1494
            lrContact.SwapEntityOrder();
        }

        // ---- [T3-contact] the REAL traffic-contact witness ------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. Opt-in (BRN_TRAFFIC_DIAG). One-shot, then latched on the
        // pair (sphere count, box count) so a changing count reprints and a steady one is silent.
        // This is the first place a HARVESTED traffic contact exists (the sibling witness in
        // DoTrafficCarWorldContactGeneration only proves a command was posted). DELETE-WHEN-STABLE.
        {
            static const bool sbTrafficDiag = (getenv("BRN_TRAFFIC_DIAG") != 0);
            static s32        siLastSphere  = -1;
            static s32        siLastBox     = -1;
            const s32 liSphere = lrSphereQueue.GetLength();
            const s32 liBox    = lrBoxQueue.GetLength();
            if (sbTrafficDiag && (liSphere + liBox) != 0
                && (liSphere != siLastSphere || liBox != siLastBox)
                && CgsDev::Log::gpDebugPrint != 0)
            {
                siLastSphere = liSphere;
                siLastBox    = liBox;
                const PotentialContact& lrFirst =
                    (liSphere != 0) ? lrSphereQueue.GetEvent(0) : lrBoxQueue.GetEvent(0);
                *CgsDev::Log::gpDebugPrint
                    << "[T3-contact] traffic-world contacts ordered sphereQ=" << liSphere
                    << " boxQ=" << liBox
                    // post-swap side A is the traffic vehicle, and the marker entry that produced
                    // it carried the car's GLOBAL entity id (0x8261C288 GetGlobalTrafficEntityId),
                    // so this index is the GLOBAL traffic index, not the physical slot. The
                    // global->physical rewrite happens later, in FixUpVehicleContacts, and only
                    // for the vehicle-vs-vehicle queues.
                    << " firstTrafficGlobalIdx="
                    << static_cast<s32>(GetWordIndex(
                           static_cast<u32>(lrFirst.muVolumeInstanceIdA.muId >> 32)))
                    << " sideAOwner="
                    << static_cast<s32>(lrFirst.muVolumeInstanceIdA.GetEntityIDOwner())
                    << " otherOwner="              // post-swap: side B is the world (0)
                    << static_cast<s32>(lrFirst.muVolumeInstanceIdB.GetEntityIDOwner())
                    << "\n";
            }
        }
    }
}
}
