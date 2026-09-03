// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_CrashState.cpp
//
// THE VEHICLE-MANAGER CRASH-STATE LEGS (wave 2026-09-03, lane P2a). Seven per-frame bodies that
// stood as log-once boot gates -- four in BrnPhysicsConductorGates.cpp, three in
// BrnVehicleManagerLinkStubs.cpp -- plus the two callees that had no home in the tree. Every gate
// is DELETED in the same change (LNK2005 is the tripwire if one reappears).
//
//   VehicleManager::CheckState                      @0x825EADA8  (170)   DWARF h:977
//   VehicleManager::UpdateFatalCrashFlags           @0x825EA970  (173)   DWARF h:932
//   VehicleManager::ClearSnappedNetworkCarContacts  @0x8261A8D0  (217)   DWARF h:1088
//   VehicleManager::UpdateVehicleImpacts            @0x82635C00  (322)   DWARF h:953
//   VehicleManager::UpdateAggressiveDriving         @0x82640690  (264)   DWARF h:1236
//   VehicleManager::CrashFatalRaceCars              @0x826361C0  (280)   DWARF h:1287  ** export hole **
//   VehicleManager::ForceRaceCarCrash (6-arg)       @0x82635B00  (28)    DWARF h:1242
//   VehicleManager::ProcessShowtimeShunts           @0x82629F20  (1002)  DWARF h:1305
//   VehicleManager::ApplyShowtimeShunt              @0x82619D28  (129)   DWARF h:1308
//
// CrashFatalRaceCars has NO .ida-exports JSON (the export directory goes InstantTakedown
// @0x82636108 (44 insns, ends 0x826361B8) -> PhysicalTrafficManagerDebugComponent::RenderWorld
// @0x82636628). IDA's xref table names 0x826361C0 as CrashFatalRaceCars (ForceRaceCarCrash
// @0x82635B00's only caller), and the 280 words at 0x826361C0..0x82636620 were decoded from
// image.bin (capstone ppc64be); the register map is in that function's banner below. The two
// assert strings it bakes were read off the image at the addresses the code loads.
//
// The four VMX128 splat constants this file reads are NOT the zeros the image shows at their
// .bss seats: the 0x82C5Bxxx dynamic-init blob writes them at boot (each writer is lfs +
// vspltw + stvx128 of a scalar literal, decoded from image.bin):
//     0x82FB7F30 <- splat(30.0f)     [0x82C5B9D8, lfs flt_82004F5C]  showtime race-car-vs-traffic shunt
//     0x82FB7FB0 <- splat(5.0f)      [0x82C5BA00, lfs flt_8200426C]  showtime traffic-vs-traffic shunt
//     0x82FB9C30 <- splat(100000.0f) [0x82C5BA50, lfs flt_820080E8]  showtime shunt speed-increase-to-quit
//     0x82FB8050 <- splat(10.0f)     [0x82C5B928, lfs flt_82004A20]  impact shunt speed-increase-to-quit
//                                                                     (same seat ApplyShunt already reads)
//
// FLAG (VMX modelling, stated once): the vrsqrtefp/vrefp + Newton-refinement cascades (Normalize,
// the mass-ratio reciprocal) are reproduced through the named vpu:: helpers / plain division, not
// register-for-register; every load-bearing RESULT (dot products, clamps, compares, table reads,
// store targets) is asm-pinned. Same statement as BrnVehicleManager_ImpactHelpers.cpp.
//
// Every member is reached BY NAME. The console offsets are quoted in the per-function banners for
// the record only; none of them reproduce on the x64 host (pointer-carrying records).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                 // EImpactType, gbReadSurfaceProperties, KI_NUM_USED_SURFACES, KAB_SURFACE_IS_WATER
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"           // PhysicalTrafficVehicle, GetTrafficVehicle
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"       // IsCrashing / IsFatallyCrashing / GetPosition / CheckState (ExternalPhysicsBody)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics.h"       // TrafficPhysics (GetFullTrafficPhysics)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"       // AddSlam / AddShunt / IsBeingSlamedOrShunted / SlamEffect / ShuntEffect
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"                // Wheel::RoadContact
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // VehicleOutputInterface / VehicleManagerOutputInterface / RaceCarState / ImpactEvent
#include "GameSource/Physics/VehicleManager/StuntOffences/BrnStuntOffencesManager.h" // SetCurrentRaceCarState
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"           // FindModelIndexByEntityID / GetDeformableObject
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // ClearStoredContacts
#include "GameSource/Physics/ContactSpies/BrnContactSpyData.h"                     // ContactSpyData::GetRaceCarContacts / GetTrafficContacts
#include "GameSource/Physics/ContactSpies/BrnContactSpyEvents.h"                   // RaceCarContact / TrafficContact
#include "GameSource/World/BrnEntityTypes.h"                                       // BrnWorld::E_ENTITYTYPE_RACECAR / _TRAFFIC_VEHICLE
#include "SharedClasses/World/BrnCollisionTag.h"                                 // BrnWorld::KU_COLLISION_MASK_SURFACE_ID / KU_COLLISION_FLAG_FATAL / _DRIVEABLE
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // BrnWorld::E_RACE_CAR_TYPE_NETWORK
#include "GameSource/GameState/BrnTakedownType.h"                                  // BrnGameState::E_TAKEDOWN_GRINDING / E_TAKEDOWN_NONE
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                         // BitArray<8> (mUsedRaceCars, mPlayerWonImpact)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                   // CgsModule::Event / VariableEventQueue<1536,16>::AddEvent
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                       // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                         // vpu::Subtract / Normalize

#include <cmath>

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // ---- image literals ------------------------------------------------------------------
        // FLAG: every constant NAME below is proposed (no DWARF symbol for a .cpp file-scope literal
        // or an anonymous rodata float); the VALUE and its use are asm-literal, addresses quoted.

        // byte_8208F9F0 (rodata, 8 bytes): the impact-type PRIORITY table UpdateVehicleImpacts
        // indexes by EImpactType (0..7): {NONE 0, TRADING_PAINT 1, NUDGE 1, SLAM 2, SHUNT 3,
        // BOOST_SLAM 2, BOOST_SHUNT 3, GRINDING 0}. Read as signed bytes (extsb).
        const signed char KAI8_IMPACT_TYPE_PRIORITY[8] = { 0, 1, 1, 2, 3, 2, 3, 0 };

        // 0x82FB8050 <- splat(10.0f): the speed-increase-to-quit every impact-driven AddShunt
        // carries (this file's UpdateVehicleImpacts and ImpactHelpers' ApplyShunt read the same seat).
        const f32 KF_IMPACT_SHUNT_SPEED_INCREASE_TO_QUIT = 10.0f;

        // flt_82F2A208 (.data, initialised): the vulnerability factor a car is given while its
        // vulnerable-time counts down, indexed by BrnWorld::ERaceCarType (PLAYER 0, AI 1, NETWORK 2,
        // INACTIVE 3). The four floats that follow it at 0x82F2A218 are a different table.
        const f32 KAF_VULNERABILITY_FACTOR_BY_RACE_CAR_TYPE[4] = { 3.0f, 6.0f, 3.0f, 0.0f };

        // byte_82F2A1A4 == 30 / byte_82F2A1A5 == 3 (.data, initialised). The grinding frame windows:
        // a per-car grind duration keeps accumulating only while fewer than 30 frames have passed
        // since the last grinding contact, and the grinding TAKEDOWN fires only while fewer than 3
        // have (i.e. the contact is live this frame or the two before).
        const unsigned char KU8_GRINDING_FRAMES_TIMEOUT       = 30;   // byte_82F2A1A4
        const unsigned char KU8_GRINDING_FRAMES_TAKEDOWN_LIVE = 3;    // byte_82F2A1A5

        const f32 KF_GRINDING_DURATION_FOR_GUI_SECONDS       = 1.0f;   // flt_82001C98 (both grind arms)
        const f32 KF_OTHER_GRINDING_TAKEDOWN_SECONDS         = 2.0f;   // flt_82001D9C (rival-grinds-player arm)
        const f32 KF_RUBBING_DURATION_FOR_GUI_SECONDS        = 0.8f;   // flt_8208F9C8
        const f32 KF_GRINDING_TAKEDOWN_NORMAL_STRESS_SQ      = 0.01f;  // flt_82002138 (f1 into InstantTakedown)

        // 0x82FB7F30 / 0x82FB7FB0 / 0x82FB9C30 (dynamic-init splats, see the file banner).
        const f32 KF_SHOWTIME_RACECAR_TRAFFIC_SHUNT_MAGNITUDE  = 30.0f;
        const f32 KF_SHOWTIME_TRAFFIC_TRAFFIC_SHUNT_MAGNITUDE  = 5.0f;
        const f32 KF_SHOWTIME_SHUNT_SPEED_INCREASE_TO_QUIT     = 100000.0f;

        // stru_8208F620.x == FLT_EPSILON (the same seat ReadSurfaceProperties compares against).
        const f32 KF_SHOWTIME_SHUNT_MIN_HORIZONTAL_OFFSET = 1.1920928955078125e-07f;

        // ⚠️ CORRECTED 2026-09-03 before gating: the wheel RoadContact::mCollisionTag low halfword
        // (the material tag) needs NO proposed constants -- the DWARF names all three bits
        // CrashFatalRaceCars reads, in SharedClasses/World/BrnCollisionTag.h:
        //   KU_COLLISION_MASK_SURFACE_ID  0x03F0  (bits 4..9)
        //   KU_COLLISION_FLAG_FATAL       0x4000  (bit 14)
        //   KU_COLLISION_FLAG_DRIVEABLE   0x2000  (bit 13)
        // The bit POSITIONS are read straight off the two rlwinm's at 0x826363D4 / 0x826363E0:
        // `rlwinm rX, tag, 18, 31, 31` puts original bit 14 in the LSB (ROTL32 by 18 maps bit 14
        // -> bit 0), and `rlwinm rX, tag, 19, 31, 31` puts bit 13 there -- 14 and 13, NOT 13 and
        // 12. Two independent bodies already in the tree spell the same two bits with the same
        // named constants: ActiveRaceCar::IsWrecked (`lwz 0x1E4 ; >>14 &1` == FLAG_FATAL) and
        // DoPlayerStuckLineTests (`extrwi 3,16 ; clrlwi 31` == bit 13 == FLAG_DRIVEABLE).
        // So the test is the one the function's NAME states: crash the car on a FATAL,
        // NON-DRIVEABLE triangle.

        // The 12-byte game event UpdateVehicleImpacts posts (AddEvent(..., 31, 12) @0x826360DC):
        // three stw's of the impact event's type/aggressor/victim words. Same id/size as the
        // takedown-scored record HandleRaceCarRaceCarContact posts (BrnVehicleManager.cpp names it
        // TakedownIoEventRecord and marks its third word "provenance untraced" -- this site proves
        // the third word is the VICTIM index: `lwz r10, 0x18(r30) ; stw r10, var_B8` @0x82636098).
        struct ImpactIoEventRecord : public CgsModule::Event
        {
            s32 miImpactType;       // +0  <- ImpactEvent::meImpactType                  (lwz 0x10(ev))
            s32 miAggressorIndex;   // +4  <- ImpactEvent::meAggressorActiveRaceCarIndex (lwz 0x14(ev))
            s32 miVictimIndex;      // +8  <- ImpactEvent::meVictimActiveRaceCarIndex    (lwz 0x18(ev))
        };
        static_assert(sizeof(ImpactIoEventRecord) == 12, "AddEvent(..., 31, 12): the console posts 12 bytes");
        const s32 KI_GAME_EVENT_IMPACT      = 31;
        const s32 KI_GAME_EVENT_IMPACT_SIZE = 12;

        // The packed-id decode the whole takedown family spells on BrnCommonTypes' EntityId word.
        inline u32 OwnerOf(u32 luEntityId)  { return luEntityId >> 24; }
        inline u32 IndexOf(u32 luEntityId)  { return (luEntityId >> 10) & 0x3FFFu; }
    }

    // =============================================================================================
    // CheckState  @0x825EADA8  (170 insns; DWARF h:977)
    //
    // The per-stage validation sweep PhysicsModule::Update brackets its stages with (15 call
    // sites). 159 of the 170 instructions are the inlined BitArray<8> first/next-bit walk plus its
    // "invalid index" assert builder (CgsBitArray.h:203). The body proper:
    //   0x825EADBC  r19 = this + 0x10000 - 0x5340 == this + 44224        -> mUsedRaceCars
    //   per set bit r31:
    //   0x825EAE60  r3 = this + 0x1460*r31 + 0x750 == &maRaceCarVehicles[r31] + 16
    //               (the ExternalPhysicsBody base sits 16 bytes into the vehicle: vfptr + pad)
    //   0x825EAE68  li r4, 0 ; 0x825EAE70 bl ExternalPhysicsBody::CheckState(this, NULL)
    // The base-class call is spelled through the derived object; MSVC places the base at the
    // same +16 on x64 for the same reason (vfptr first, base aligned to 16). CheckState(NULL) is
    // what the console passes; the host DebugPrint sink null-guards the context string.
    // =============================================================================================
    void VehicleManager::CheckState()
    {
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();                     // 0x825EADC0..0x825EAE1C
             liCar >= 0;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))                     // 0x825EAE3C..0x825EAFE8
        {
            maRaceCarVehicles[liCar].CheckState(0);                              // 0x825EAE70 bl @0x825A24B0
        }
    }

    // =============================================================================================
    // UpdateFatalCrashFlags  @0x825EA970  (173 insns; DWARF h:932)
    //
    // Same inlined BitArray<8> walk (this+44224). Per live car r31:
    //   0x825EAA3C  lbz r30, 0xE51(this + 0x1460*r31 + 0x740)   -> maRaceCarVehicles[r31] +0x711
    //                                                              == mbStartedFatallyCrashing
    //   0x825EAA40  bl VehicleOutputInterface::GetRaceCarState(lpVehicleOutputInterface, r31)
    //   0x825EAA48  stb r30, 0x44B(r3)                           -> RaceCarState +1099
    //                                                              == mbIsFatalyCrashing
    // Publishes the physics-side fatal-crash latch into the frame's RaceCarState snapshot -- the
    // word the game side (RaceCarEntityModule / camera / scoring) reads as "fatally crashing".
    // =============================================================================================
    void VehicleManager::UpdateFatalCrashFlags(VehicleOutputInterface* lpVehicleOutputInterface)
    {
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar >= 0;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            const bool lbFatallyCrashing = maRaceCarVehicles[liCar].IsFatallyCrashing();   // 0x825EAA3C lbz +0x711
            lpVehicleOutputInterface->GetRaceCarState(liCar)->mbIsFatalyCrashing = lbFatallyCrashing;   // 0x825EAA48 stb +1099
        }
    }

    // =============================================================================================
    // ClearSnappedNetworkCarContacts  @0x8261A8D0  (217 insns; DWARF h:1088)
    //
    // Per live car r30 (BitArray<8> walk over this+44224):
    //   0x8261A928  lbz (this + 224*r30 + 0x115)     -> maRaceCarDrivers[r30] +0xD5 == mbSnappedThisFrame
    //   0x8261A934  lwz (this + 4*(r30+11048))       -> maeRaceCarTypes[r30] != 2 -> assert
    //               "maeRaceCarTypes[liRaceCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK" (BrnVehicleManager.cpp:9942)
    //   0x8261A960  bl DeformationManager::FindModelIndexByEntityID(lpDeformationManager,
    //                   this + 4*(r30+10896))        -> maRaceCarEntityIDs[r30]
    //   0x8261A96C  == -1 -> assert "liModelIndex != -1" (BrnDeformationManager.h:878)
    //   0x8261A990  bl DeformableObject::ClearStoredContacts(mgr->mpaModels[idx])   (26496 stride, +76032)
    // Tail (0x8261AB00): PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts(this+44768, mgr)
    //   @0x825F37F0 (220) -- the same walk over the traffic drivers' mbSnappedThisFrame.
    //
    // A driver's mbSnappedThisFrame is raised only by the network catch-up snap, so offline this
    // is a no-op walk. The race-car arm is real; the traffic arm is a NAMED PARK (its callee has
    // no host body).
    // =============================================================================================
    void VehicleManager::ClearSnappedNetworkCarContacts(Deformation::DeformationManager* lpDeformationManager)
    {
        for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
             liRaceCar >= 0;
             liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
        {
            if (!maRaceCarDrivers[liRaceCar].SnappedThisFrame())                     // 0x8261A928 lbz +0xD5
                continue;

            CGS_ASSERT(maeRaceCarTypes[liRaceCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK,
                       "maeRaceCarTypes[liRaceCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK");   // :9942

            const s32 liModelIndex =
                lpDeformationManager->FindModelIndexByEntityID(maRaceCarEntityIDs[liRaceCar]);   // 0x8261A960
            CGS_ASSERT(liModelIndex != -1, "liModelIndex != -1");                    // BrnDeformationManager.h:878

            // [GUARD] the console indexes mpaModels[-1] on a failed lookup; on the host that is a
            // wild pointer, so the (asserted) miss is skipped. DELETE-WHEN the assert is fatal.
            if (liModelIndex != -1)
                lpDeformationManager->GetDeformableObject(liModelIndex)->ClearStoredContacts();   // 0x8261A990
        }

        // [FLAG PC bring-up] PhysicalTrafficManager::ClearSnappedNetworkTrafficContacts @0x825F37F0
        // (220 insns) -- not declared or bodied on the host. It is the traffic twin of the loop above
        // (walk the used-traffic bitset, skip drivers without mbSnappedThisFrame, GetPhysicsEntityId
        // -> FindModelIndexByEntityID -> ClearStoredContacts). Offline no traffic driver is ever
        // snapped, so nothing is lost until network play. DELETE-WHEN that body lands in
        // BrnPhysicalTrafficManager (declaration text in this wave's report).
    }

    // =============================================================================================
    // UpdateVehicleImpacts  @0x82635C00  (322 insns; DWARF h:953)
    //
    // Drains the frame's ImpactEvent queue (the classifier output of HandleRaceCarRaceCarContact,
    // 48-byte events, BaseEventQueue<ImpactEvent>::GetEvent @0x8254D8B8) and, for every event the
    // LOCAL PLAYER is a party to, updates the per-car impact bookkeeping, applies the physical
    // slam/shunt to the player car when the player is the VICTIM, and posts the 12-byte game event.
    //
    // Register map (r28 == this, r31 == queue, r30 == event, r26 == the OTHER car, r24 == player-is-aggressor):
    //   0x82635C20  lwz r11, 8(queue)                       -> GetLength()
    //   0x82635CCC  r19 = this + 0x2A0AC                    -> mePlayerActiveRaceCarIndex
    //   0x82635CD0  lwz 0x14(ev) / 0x18(ev)                 -> meAggressorActiveRaceCarIndex / meVictimActiveRaceCarIndex
    //   0x82635CD4..CE4  skip unless aggressor == player || victim == player
    //   0x82635CE8..D08  r24 = (aggressor == player) ; r26 = r24 ? victim : aggressor
    //   0x82635D0C  lwz 0x10(ev) -> meImpactType ; r22 = type in {SLAM 3, BOOST_SLAM 5}
    //   0x82635D30  r31 = 4*(r26 + 0x1_0000 - 0x5861) == 171644 + 4*r26 -> maeImpactType[other]
    //   0x82635D40  r29 = maeImpactType[other] in {3, 5}
    //   0x82635D5C  r25 = type in {SHUNT 4, BOOST_SHUNT 6}
    //   0x82635D80  bl HasRaceCarHadRecentImpact(other) -> r27 = 1 if true, else:
    //   0x82635D90..DB0  lbzx byte_8208F9F0[type], byte_8208F9F0[maeImpactType[other]] ; extsb both ;
    //               cmpw old, new ; bgt -> r27 = 1 ; bne -> r27 stays 0 ; equal ->
    //   0x82635DB8..DE8  r27 = 1 iff r22 && r29 && lbz 0x2C(ev) (muScore) > lbz (this + r26 + 0x29E9C) (mauImpactScore[other])
    //   0x82635DF0  if (!r27) next event
    //   0x82635DFC  if (!r24 && HasRaceCarHadRecentImpact(other)) {          // the player is the victim of a car still in cooldown
    //   0x82635E1C     r11 = &maRaceCarVehicles[player] ;
    //                  stfs 0.0 -> +0x1114/+0x1118/+0x1120/+0x111C (mSlamEffect.mfSteering/mfOriginalSteering/mfTotalSlamTime/mfSlamLife)
    //                  stb -1 -> +0x1128 (mSlamEffect.mi8SlamNumber)
    //                  stvx128 0 -> +0x1130 (mShuntEffect.mDirectionPlusDesiredSpeed)
    //                  +0x1140: lane y <- 0.0, lane x <- -1.0 (mShuntEffect.mv4_Life_SpeedIncreaseToQuit), z/w kept
    //               }
    //   0x82635E8C  stwx type -> maeImpactType[other]
    //   0x82635EA4  lfsx this+0x29E14 (mfMinSecondsBetweenImpacts) -> stfsx this + 4*(other+0x1_0000-0x5857) == mafNoImpactTimeSeconds[other]
    //   0x82635EB4  stbx muScore -> mauImpactScore[other]
    //   0x82635EB8..FA0  r24 ? mPlayerWonImpact.SetBit(other) (CgsBitArray.h:222 "Index: ..Number of bits: ")
    //                       : mPlayerWonImpact.UnSetBit(other) (CgsBitArray.h:241 "luIndex < NUMBITS")
    //   0x82635FBC  stbx r23 (== r24) -> this+0x29ED4 == mbPlayerWonDisplayImpact
    //   0x82635FC0  stwx type -> this+0x29ED0 == meDisplayImpactType
    //   0x82635FC4  if (!r24) {                                              // player is the VICTIM: apply the physics
    //   0x82635FD0     assert victim != aggressor "leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex" (:9840)
    //   0x8263601C     VehiclePhysics::AddSlam(&maRaceCarVehicles[player], r4 = lbz this+0x2A11B (mbIsOnlineGameMode),
    //                                          f1 = 0x20(ev) mfDuration, f2 = 0x24(ev) mfSteeringDirection,
    //                                          f3 = 0x28(ev) mfRecoveryTime, r8 = extsb(aggressor))
    //   0x82636020     if (r25) AddShunt(player car, r4 = extsb(aggressor), v1 = splat(lfs 0x1C(ev) mfMagnitude),
    //                                    v2 = lvx128 0(ev) mDirection, v3 = unk_82FB8050 (splat 10.0))
    //   0x8263605C     if (r22 || r25) mafVulnerableTimeSeconds[other] (4*(other+0x1_0000-0x5848)) = this+0x29DEC (mfAIVulnerabilityDurationSeconds)
    //               }
    //   0x8263608C  r31 = lpVehicleOutputInterface + 0x65F0 (GetGameEventQueue) ; the 12-byte record
    //               {0x10(ev), 0x14(ev), 0x18(ev)} ; assert aggressor != victim
    //               "lEvent.meAggressorActiveRaceCarIndex != lEvent.meVictimActiveRaceCarIndex" (:9107)
    //   0x826360DC  bl VariableEventQueue<1536,16>::AddEvent(queue, &rec, 31, 12)
    //
    // WHERE THE PSEUDOCODE LIED: it rendered the priority compare as `v24 > v23` with both operands
    // unnamed and dropped the three AddSlam floats and both AddShunt vectors (VMX args). The asm
    // above is the source of every operand.
    //
    // Only lpImpactEventQueue (r4) and lpVehicleOutputInterface (r5, the game-event sink) are read;
    // r6..r8 are stored to the home area and never reloaded.
    // =============================================================================================
    void VehicleManager::UpdateVehicleImpacts(
        const CgsModule::EventQueue<ImpactEvent, 16>* lpImpactEventQueue,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        (void)lpRequestOutputInterface;          // r6: stored to the home area, never read
        (void)lpVehicleManagerOutputInterface;   // r7: idem
        (void)lpDeformationInterface;            // r8: idem

        const s32 liNumEvents = lpImpactEventQueue->GetLength();                                  // 0x82635C20
        for (s32 liEvent = 0; liEvent < liNumEvents; ++liEvent)
        {
            const ImpactEvent& lEvent = lpImpactEventQueue->GetEvent(liEvent);                     // 0x82635CC4 @0x8254D8B8

            const s32 liPlayer    = static_cast<s32>(mePlayerActiveRaceCarIndex);                 // lwz 0(r19)
            const s32 liAggressor = static_cast<s32>(lEvent.meAggressorActiveRaceCarIndex);       // lwz 0x14(ev)
            const s32 liVictim    = static_cast<s32>(lEvent.meVictimActiveRaceCarIndex);          // lwz 0x18(ev)
            if (liAggressor != liPlayer && liVictim != liPlayer)                                   // 0x82635CD4..CE4
                continue;

            const bool lbPlayerIsAggressor = (liAggressor == liPlayer);                            // 0x82635CE8..CF4 (r24)
            const s32  liOther             = lbPlayerIsAggressor ? liVictim : liAggressor;         // 0x82635D00..D08 (r26)

            const EImpactType leType = lEvent.meImpactType;                                        // lwz 0x10(ev)
            const bool lbTypeIsSlam      = (leType == E_IMPACT_SLAM  || leType == E_IMPACT_BOOST_SLAM);           // r22
            const bool lbPrevTypeIsSlam  = (maeImpactType[liOther] == E_IMPACT_SLAM ||
                                            maeImpactType[liOther] == E_IMPACT_BOOST_SLAM);                       // r29
            const bool lbTypeIsShunt     = (leType == E_IMPACT_SHUNT || leType == E_IMPACT_BOOST_SHUNT);          // r25

            // Does this impact REPLACE the one already recorded against the other car? Yes while the
            // other car is inside its impact cooldown; else only when the recorded type's priority
            // beats the new one, or ties it as a slam-vs-slam decided by the classifier's score.
            bool lbApply = false;                                                                  // r27
            if (HasRaceCarHadRecentImpact(liOther))                                                // 0x82635D80
            {
                lbApply = true;
            }
            else
            {
                const signed char liNewPriority = KAI8_IMPACT_TYPE_PRIORITY[static_cast<s32>(leType)];                 // lbzx (type)
                const signed char liOldPriority = KAI8_IMPACT_TYPE_PRIORITY[static_cast<s32>(maeImpactType[liOther])]; // lbzx (old)
                if (liOldPriority > liNewPriority)                                                 // 0x82635DAC cmpw old,new ; bgt
                {
                    lbApply = true;
                }
                else if (liOldPriority == liNewPriority                                            // bne -> skip
                         && lbTypeIsSlam && lbPrevTypeIsSlam
                         && lEvent.muScore > mauImpactScore[liOther])                              // 0x82635DD4..DE4 (unsigned compare)
                {
                    lbApply = true;
                }
            }
            if (!lbApply)                                                                          // 0x82635DF0
                continue;

            RaceCarPhysics& lrPlayerCar = maRaceCarVehicles[liPlayer];

            // 0x82635DFC..0x82636070: the player is the victim of a car still in cooldown -> the
            // player's current slam and shunt are cancelled before the new one is applied.
            if (!lbPlayerIsAggressor && HasRaceCarHadRecentImpact(liOther))
            {
                lrPlayerCar.mSlamEffect.mfSteering         = 0.0f;   // stfs +0x1114
                lrPlayerCar.mSlamEffect.mfOriginalSteering = 0.0f;   // stfs +0x1118
                lrPlayerCar.mSlamEffect.mfSlamLife         = 0.0f;   // stfs +0x111C
                lrPlayerCar.mSlamEffect.mfTotalSlamTime    = 0.0f;   // stfs +0x1120
                lrPlayerCar.mSlamEffect.mi8SlamNumber      = -1;     // stb  +0x1128
                lrPlayerCar.mShuntEffect.mDirectionPlusDesiredSpeed = Vector3Plus{ 0.0f, 0.0f, 0.0f, 0.0f };   // stvx128 v0(0) +0x1130
                lrPlayerCar.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.y = 0.0f;    // vrlimi128 mask 4 (lane y) <- 0.0
                lrPlayerCar.mShuntEffect.mv4_Life_SpeedIncreaseToQuit.x = -1.0f;   // vrlimi128 mask 8 (lane x) <- -1.0 ; z/w kept
            }

            maeImpactType[liOther]          = leType;                                              // 0x82635E8C
            mafNoImpactTimeSeconds[liOther] = mfMinSecondsBetweenImpacts;                          // 0x82635EA4 (arms the cooldown)
            mauImpactScore[liOther]         = lEvent.muScore;                                      // 0x82635EB4

            if (lbPlayerIsAggressor)                                                               // 0x82635EB8
                mPlayerWonImpact.SetBit(static_cast<u32>(liOther));                                // 0x82635F4C..F60 (or)
            else
                mPlayerWonImpact.UnSetBit(static_cast<u32>(liOther));                              // 0x82635F88..F9C (andc)

            mbPlayerWonDisplayImpact = lbPlayerIsAggressor;                                        // 0x82635FBC stbx +0x29ED4
            meDisplayImpactType      = leType;                                                     // 0x82635FC0 stwx +0x29ED0

            if (!lbPlayerIsAggressor)                                                              // 0x82635FC4: the player was hit
            {
                CGS_ASSERT(liVictim != liAggressor,
                           "leVictimActiveRaceCarIndex != leAggressorActiveRaceCarIndex");         // :9840

                lrPlayerCar.AddSlam(mbIsOnlineGameMode,                                            // r4  lbzx +0x2A11B
                                    lEvent.mfDuration,                                             // f1  lfs 0x20(ev)
                                    lEvent.mfSteeringDirection,                                    // f2  lfs 0x24(ev)
                                    lEvent.mfRecoveryTime,                                         // f3  lfs 0x28(ev)
                                    static_cast<s8>(liAggressor));                                 // r8  extsb
                if (lbTypeIsShunt)                                                                 // 0x82636020..28
                {
                    const f32 lfMagnitude = lEvent.mfMagnitude;                                    // lvlx 0x1C(ev) ; vspltw
                    lrPlayerCar.AddShunt(VecFloat{ lfMagnitude, lfMagnitude, lfMagnitude, lfMagnitude },   // v1
                                         lEvent.mDirection,                                               // v2 lvx128 0(ev)
                                         VecFloat{ KF_IMPACT_SHUNT_SPEED_INCREASE_TO_QUIT,                 // v3 unk_82FB8050
                                                   KF_IMPACT_SHUNT_SPEED_INCREASE_TO_QUIT,
                                                   KF_IMPACT_SHUNT_SPEED_INCREASE_TO_QUIT,
                                                   KF_IMPACT_SHUNT_SPEED_INCREASE_TO_QUIT },
                                         static_cast<s8>(liAggressor));                                   // r4 extsb
                }
                if (lbTypeIsSlam || lbTypeIsShunt)                                                 // 0x8263605C..6C
                    mafVulnerableTimeSeconds[liOther] = mfAIVulnerabilityDurationSeconds;          // 0x82636084/88
            }

            // 0x8263608C..0x826360DC: the 12-byte game event, whichever side the player was on.
            ImpactIoEventRecord lRecord;
            lRecord.miImpactType     = static_cast<s32>(leType);                                   // stw var_C0
            lRecord.miAggressorIndex = liAggressor;                                                // stw var_BC
            lRecord.miVictimIndex    = liVictim;                                                   // stw var_B8
            CGS_ASSERT(liAggressor != liVictim,
                       "lEvent.meAggressorActiveRaceCarIndex != lEvent.meVictimActiveRaceCarIndex");   // :9107
            lpVehicleOutputInterface->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_GAME_EVENT_IMPACT, KI_GAME_EVENT_IMPACT_SIZE);   // 0x826360DC
        }
    }

    // =============================================================================================
    // UpdateAggressiveDriving  @0x82640690  (264 insns; DWARF h:1236)
    //
    // The per-frame GRINDING / RUBBING ticker over ALL EIGHT slots (a counted loop, not the used
    // bitset). PPC arg map with the float consuming a GPR slot: this=r3, f1=timestep, r5=Request
    // (r18), r6=Manager (r28), r7=VehicleOutput (r27), r8=Deformation (r17).
    //   r22 = this+171936+i  mau8FramesSincePlayerGrindingOther[i]  (+8: ..OtherGrindingPlayer, +0x10: mabRubbingThisUpdate)
    //   r21 = this+171744+4i mafVulnerableTimeSeconds[i]  (-0x3C: mafNoImpactTimeSeconds, +0x20: mafVulnerabilityFactor,
    //                        +0x40: mafTotalVulnerableTime, +0x60: mafPlayerGrindingOtherDurationSeconds,
    //                        +0x80: mafOtherGrindingPlayerDurationSeconds, +0xA0: mafRubbingDurationSeconds)
    //   r26 = this+43744+8i  maRaceCarHandlingBodyIDs[i]  (the EntityId is the HIGH word: ld ; srdi 32)
    //   r25 = this+0x780+0x1460*i  maRaceCarVehicles[i].mTransform row 3 == GetPosition()
    //   r19 = 0x2A0AC        mePlayerActiveRaceCarIndex
    //
    // Per slot i:
    //   0x82640760  if (HasRaceCarHadRecentImpact(i)) mafNoImpactTimeSeconds[i] -= dt
    //   0x8264077C  if (mafVulnerableTimeSeconds[i] > 0) { it -= dt ;
    //                  > 0 ? mafVulnerabilityFactor[i] = flt_82F2A208[maeRaceCarTypes[i]]   (0x826407AC..C4; r21-0x20000+0xDC0 == +44192)
    //                      : VulnerableTime = 0, mafTotalVulnerableTime[i] = 0, factor = 1.0 }
    //   0x826407C8  PLAYER-GRINDS-OTHER arm: if (frames[i] >= 30) duration = 0 ; else {
    //                  prev = duration ; duration += dt ;
    //                  if (duration >= 1.0 && prev < 1.0) { VehicleOutput+0x6C02 = 1 (mAggressiveDrivingFlags.mbPlayerWonGrindingThisFrame)
    //                                                       Manager+0x79C = 1 (mVehicleGuiOutputMessages.mbPlayerGrindingOther) }
    //                  if (duration >= 1.0 && maeRaceCarTypes[i] != NETWORK && frames[i] < 3)
    //                      InstantTakedown(victim = handlingId[i].hi, aggressor = handlingId[player].hi,
    //                                      v1 = normalise(pos[i] - pos[player]), v2 = pos[i] - pos[player] (raw),
    //                                      f1 = 0.01, r7..r10 = the four interfaces, stack = 1 == E_TAKEDOWN_GRINDING)
    //                  ++frames[i] }
    //   0x826408D0  OTHER-GRINDS-PLAYER arm: if (frames2[i] >= 30) duration2 = 0 ; else {
    //                  assert i != mePlayerActiveRaceCarIndex "leActiveRaceCarIndex != mePlayerActiveRaceCarIndex" (:8417)
    //                  prev = duration2 ; duration2 += dt ;
    //                  if (duration2 >= 1.0 && prev < 1.0) { VehicleOutput+0x6C03 = 1 (mbPlayerLostGrindingThisFrame)
    //                                                         Manager+0x79D = 1 (mbOtherGrindingPlayer) }
    //                  if (duration2 >= 2.0 (flt_82001D9C) && frames2[i] < 3)
    //                      InstantTakedown(victim = handlingId[player].hi, aggressor = handlingId[i].hi,
    //                                      v1 = normalise(pos[player] - pos[i]), v2 = raw diff, 0.01, ..., E_TAKEDOWN_GRINDING)
    //                  ++frames2[i] }
    //   0x82640A00  RUBBING arm: if (mabRubbingThisUpdate[i]) { if (frames2[i] >= 30 && frames[i] >= 30) {
    //                  rubbing += dt ; if (rubbing >= 0.8) { VehicleOutput+0x6C04 = 1 (mbRubbingThisFrame),
    //                  Manager+0x79E = 1 (mbRubbing), rubbing = 0 } } mabRubbingThisUpdate[i] = 0 }
    //               else rubbing = 0
    //   0x82640A78  loop-end assert i+1 <= 8 "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" (BurnoutConstants.h:39)
    //
    // WHERE THE PSEUDOCODE LIED: it showed InstantTakedown with `a7` (a phantom 7th parameter)
    // and lost both vector arguments; the real operands are the VMX block above (vsubfp ; vmsum3fp ;
    // vrsqrtefp + 2 Newton steps ; vmulfp == Normalize) and the two `ld ; srdi 32` handling ids.
    // =============================================================================================
    void VehicleManager::UpdateAggressiveDriving(
        f32 lfTimeStep,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        AggressiveDrivingFlags&   lrFlags = lpVehicleOutputInterface->GetAggressiveDrivingFlags();          // +0x6C00
        VehicleGuiOutputMessages& lrGui   = lpVehicleManagerOutputInterface->GetVehicleGuiOutputMessages(); // +0x79C

        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            const s32 liPlayer = static_cast<s32>(mePlayerActiveRaceCarIndex);

            if (HasRaceCarHadRecentImpact(liCar))                                                  // 0x82640760
                mafNoImpactTimeSeconds[liCar] -= lfTimeStep;                                       // 0x82640770..78

            if (mafVulnerableTimeSeconds[liCar] > 0.0f)                                            // 0x82640784
            {
                mafVulnerableTimeSeconds[liCar] -= lfTimeStep;                                     // 0x8264078C..90
                if (mafVulnerableTimeSeconds[liCar] > 0.0f)                                        // 0x82640794
                {
                    mafVulnerabilityFactor[liCar] =
                        KAF_VULNERABILITY_FACTOR_BY_RACE_CAR_TYPE[static_cast<s32>(maeRaceCarTypes[liCar])];   // 0x826407AC..C4
                }
                else
                {
                    mafVulnerableTimeSeconds[liCar] = 0.0f;                                        // 0x8264079C
                    mafTotalVulnerableTime[liCar]   = 0.0f;                                        // 0x826407A0 (+0x40)
                    mafVulnerabilityFactor[liCar]   = 1.0f;                                        // 0x826407A4 (+0x20)
                }
            }

            // ---- the player is grinding car liCar --------------------------------------------
            if (mau8FramesSincePlayerGrindingOther[liCar] >= KU8_GRINDING_FRAMES_TIMEOUT)          // 0x826407C8..D4
            {
                mafPlayerGrindingOtherDurationSeconds[liCar] = 0.0f;                               // 0x826408CC
            }
            else
            {
                const f32 lfPrevious = mafPlayerGrindingOtherDurationSeconds[liCar];               // 0x826407D8
                mafPlayerGrindingOtherDurationSeconds[liCar] = lfPrevious + lfTimeStep;            // 0x826407DC..E0
                if (mafPlayerGrindingOtherDurationSeconds[liCar] >= KF_GRINDING_DURATION_FOR_GUI_SECONDS)   // 0x826407E4
                {
                    if (lfPrevious < KF_GRINDING_DURATION_FOR_GUI_SECONDS)                         // 0x826407EC (the rising edge)
                    {
                        lrFlags.mbPlayerWonGrindingThisFrame = true;                               // stb 0x6C02(VehicleOutput)
                        lrGui.mbPlayerGrindingOther          = true;                               // stb 0x79C(Manager)
                    }
                    if (maeRaceCarTypes[liCar] != BrnWorld::E_RACE_CAR_TYPE_NETWORK                // 0x826407FC..0C
                        && mau8FramesSincePlayerGrindingOther[liCar] < KU8_GRINDING_FRAMES_TAKEDOWN_LIVE)   // 0x82640810..1C
                    {
                        EntityId lVictimId;    lVictimId.muValue    = static_cast<u32>(maRaceCarHandlingBodyIDs[liCar]    >> 32);   // ld 0(r26) ; srdi 32
                        EntityId lAggressorId; lAggressorId.muValue = static_cast<u32>(maRaceCarHandlingBodyIDs[liPlayer] >> 32);   // 0x82640854..7C
                        const Vector3 lvOffset = vpu::Subtract(maRaceCarVehicles[liCar].GetPosition(),          // v11 lvx128 r25
                                                               maRaceCarVehicles[liPlayer].GetPosition());      // v0  lvx128 r6+0x780
                        InstantTakedown(lVictimId, lAggressorId,
                                        vpu::Normalize(lvOffset),                                  // v1 (vmsum3fp/vrsqrtefp cascade)
                                        lvOffset,                                                  // v2 (the raw difference)
                                        KF_GRINDING_TAKEDOWN_NORMAL_STRESS_SQ,                     // f1 flt_82002138
                                        lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                                        lpVehicleOutputInterface, lpDeformationInterface,
                                        BrnGameState::E_TAKEDOWN_GRINDING);                        // stw 1, var_C4
                    }
                }
                ++mau8FramesSincePlayerGrindingOther[liCar];                                       // 0x826408BC..C4
            }

            // ---- car liCar is grinding the player --------------------------------------------
            if (mau8FramesSinceOtherGrindingPlayer[liCar] >= KU8_GRINDING_FRAMES_TIMEOUT)          // 0x826408D0..DC
            {
                mafOtherGrindingPlayerDurationSeconds[liCar] = 0.0f;                               // 0x826409FC
            }
            else
            {
                CGS_ASSERT(liCar != liPlayer, "leActiveRaceCarIndex != mePlayerActiveRaceCarIndex");   // :8417 (not a guard)

                const f32 lfPrevious = mafOtherGrindingPlayerDurationSeconds[liCar];               // 0x8264090C
                mafOtherGrindingPlayerDurationSeconds[liCar] = lfPrevious + lfTimeStep;            // 0x82640910..14
                if (mafOtherGrindingPlayerDurationSeconds[liCar] >= KF_GRINDING_DURATION_FOR_GUI_SECONDS
                    && lfPrevious < KF_GRINDING_DURATION_FOR_GUI_SECONDS)                          // 0x82640918..24
                {
                    lrFlags.mbPlayerLostGrindingThisFrame = true;                                  // stb 0x6C03(VehicleOutput)
                    lrGui.mbOtherGrindingPlayer           = true;                                  // stb 0x79D(Manager)
                }
                if (mafOtherGrindingPlayerDurationSeconds[liCar] >= KF_OTHER_GRINDING_TAKEDOWN_SECONDS   // 0x82640934..40 (flt_82001D9C)
                    && mau8FramesSinceOtherGrindingPlayer[liCar] < KU8_GRINDING_FRAMES_TAKEDOWN_LIVE)    // 0x82640944..50
                {
                    EntityId lVictimId;    lVictimId.muValue    = static_cast<u32>(maRaceCarHandlingBodyIDs[liPlayer] >> 32);   // 0x8264097C..B0
                    EntityId lAggressorId; lAggressorId.muValue = static_cast<u32>(maRaceCarHandlingBodyIDs[liCar]    >> 32);   // ld 0(r26) ; srdi 32
                    const Vector3 lvOffset = vpu::Subtract(maRaceCarVehicles[liPlayer].GetPosition(),       // v0 lvx128 r6+0x780
                                                           maRaceCarVehicles[liCar].GetPosition());         // v11 lvx128 r25
                    InstantTakedown(lVictimId, lAggressorId,
                                    vpu::Normalize(lvOffset), lvOffset,
                                    KF_GRINDING_TAKEDOWN_NORMAL_STRESS_SQ,
                                    lpRequestOutputInterface, lpVehicleManagerOutputInterface,
                                    lpVehicleOutputInterface, lpDeformationInterface,
                                    BrnGameState::E_TAKEDOWN_GRINDING);                            // stw 1, var_C4
                }
                ++mau8FramesSinceOtherGrindingPlayer[liCar];                                       // 0x826409EC..F4
            }

            // ---- rubbing -----------------------------------------------------------------------
            if (mabRubbingThisUpdate[liCar])                                                       // 0x82640A00..08
            {
                if (mau8FramesSinceOtherGrindingPlayer[liCar] >= KU8_GRINDING_FRAMES_TIMEOUT       // 0x82640A0C..18
                    && mau8FramesSincePlayerGrindingOther[liCar] >= KU8_GRINDING_FRAMES_TIMEOUT)   // 0x82640A1C..24
                {
                    mafRubbingDurationSeconds[liCar] += lfTimeStep;                                // 0x82640A28..34
                    if (mafRubbingDurationSeconds[liCar] >= KF_RUBBING_DURATION_FOR_GUI_SECONDS)   // 0x82640A3C (flt_8208F9C8)
                    {
                        lrFlags.mbRubbingThisFrame = true;                                         // stb 0x6C04(VehicleOutput)
                        lrGui.mbRubbing            = true;                                         // stb 0x79E(Manager)
                        mafRubbingDurationSeconds[liCar] = 0.0f;                                   // 0x82640A4C
                    }
                }
                mabRubbingThisUpdate[liCar] = false;                                               // 0x82640A50..54
            }
            else
            {
                mafRubbingDurationSeconds[liCar] = 0.0f;                                           // 0x82640A5C
            }

            CGS_ASSERT(liCar + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");   // BurnoutConstants.h:39
        }
    }

    // =============================================================================================
    // ForceRaceCarCrash (6-arg overload)  @0x82635B00  (28 insns; DWARF h:1242)
    //
    //   0x82635B10..B28  lbz 0xE50(this + 0x1460*r8) -> maRaceCarVehicles[idx].mbCrashing ; bne -> return
    //   0x82635B2C..B58  ld (this + 8*(idx + 0x155C)) ; srdi 32 -> r4 = maRaceCarHandlingBodyIDs[idx] high word (the victim)
    //                    r5 = r9 (the EntityId argument -- the WORLD id, as the aggressor)
    //                    v1 = v2 = 0 ; r6..r9 = the four interfaces ; r10 = -1 (E_TAKEDOWN_NONE)
    //   0x82635B5C       bl SetRaceCarCrashing
    // The 5-arg overload (BrnVehicleManager_UpdateVehiclePhysics.cpp) is the same commit with the
    // car as its own aggressor plus the two invulnerability clears; this one has neither.
    // =============================================================================================
    void VehicleManager::ForceRaceCarCrash(
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        EActiveRaceCarIndex leRaceCarIndex,
        EntityId lAggressorEntityId)
    {
        if (maRaceCarVehicles[leRaceCarIndex].IsCrashing())                                        // lbz 0xE50 gate
            return;

        EntityId lVictimId;
        lVictimId.muValue = static_cast<u32>(maRaceCarHandlingBodyIDs[leRaceCarIndex] >> 32);      // ld ; srdi 32
        SetRaceCarCrashing(lVictimId,
                           lAggressorEntityId,                                                     // mr r5, r9
                           Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                                      // v1 (vspltisw 0)
                           Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                                      // v2 (vmr)
                           lpRequestOutputInterface,
                           lpVehicleManagerOutputInterface,
                           lpVehicleOutputInterface,
                           lpDeformationInterface,
                           BrnGameState::E_TAKEDOWN_NONE);                                         // li r10, -1
    }

    // =============================================================================================
    // CrashFatalRaceCars  @0x826361C0  (280 insns; DWARF h:1287)  -- EXPORT HOLE, decoded from image.bin
    //
    // Called every frame by UpdateVehiclePhysics behind mbCrashRaceCarWhenFatal (Construct-seeded
    // TRUE). THE WORLD-SURFACE FATAL-CRASH DETECTOR: any live, non-crashing, non-network race car
    // not being reset whose wheel stands on a triangle tagged "crash on contact" is crashed by the
    // world; a wheel on a WATER surface instead raises the stunt-offence car-reset state.
    //
    // Register map (r18 == this; r4..r8 spilled to 0x16C..0x18C(r1) and reloaded for the call):
    //   0x826361DC/EC  r9 = this + 0x10000 - 0x5340 == this + 44224   -> mUsedRaceCars (BitArray<8> walk,
    //                  first bit 0x826361FC..0x82636250, next bit 0x8263644C..0x82636614, both with the
    //                  CgsBitArray.h:203 "invalid index : %u < 8" builder at 0x82636474..0x8263656C)
    //   per live car r29:
    //   0x826362B8  r27 = this + 0x1460*r29 + 0x740                 -> &maRaceCarVehicles[r29]
    //   0x826362C4  lbz 0x710(r27) != 0 -> skip                     -> IsCrashing()
    //   0x826362D0  lwz (this + 4*(r29 + 0x2B28)) == 2 -> skip      -> maeRaceCarTypes[r29] == E_RACE_CAR_TYPE_NETWORK
    //   0x826362E4  lbz 0x79(this + 0xE0*r29) != 0 -> skip          -> maRaceCarDrivers[r29] (+64) + 0x39 == mControls.mbReset
    //   per wheel r28 = 0..3:
    //   0x826362FC  ld/std x6 from r27 + 0xE0*r28 + 0x130            -> the 48-byte maWheels[r28].mRoadContact copy (stack 0x80)
    //   0x82636328  lbz byte_82FB7DF0 == 0 -> assert (:9368)          -> gbReadSurfaceProperties,
    //               "Trying to read surface 'is water' property before surface properties have been loaded"
    //   0x8263638C  lhz 0xA6(r1) -> copy +0x26 == mCollisionTag low halfword ; srwi 4 ; clrlwi 26 -> surface id (bits 4..9)
    //   0x8263639C  lwz dword_82F2A10C -> KI_NUM_USED_SURFACES ; id >= -> assert (:9374)
    //               "static_cast<int32_t>( luSurfaceId ) < KI_NUM_USED_SURFACES"
    //   0x826363C0  lbz 0xA8(r1) -> copy +0x28 == mbIsOnGround
    //   0x826363C4  lbzx byte_82FB7DF4[id]                            -> KAB_SURFACE_IS_WATER[id]
    //   0x826363C8..F4  if (onGround && (tag16 & 0x4000) && !(tag16 & 0x2000) && !isWater)
    //                   == onGround && FATAL && !DRIVEABLE && !water   (see the constants note)
    //   0x826363F8..14     ForceRaceCarCrash(r4..r7 = the four interfaces, r8 = r29, r9 = the EntityId arg)  @0x82635B00
    //   0x82636418..34  if (isWater) { r11 = this + 0x10000 - 0x5330 == this + 44240 (mStuntOffencesManager) ;
    //                                  lwz 0x28 ; ori 0x20 ; stw 0x28 }   -> SetCurrentRaceCarState(CAR_HAS_BEEN_RESET)
    //   0x82636438  ++r28 < 4
    // No pseudocode exists for this body; the whole thing is the disassembly above.
    // =============================================================================================
    void VehicleManager::CrashFatalRaceCars(
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* lpRequestOutputInterface,
        VehicleManagerOutputInterface* lpVehicleManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface,
        CgsSceneManager::EntityId lWorldEntityId)
    {
        EntityId lWorldId;
        lWorldId.muValue = static_cast<u32>(lWorldEntityId);   // r9 passes through untouched

        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar >= 0;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            RaceCarPhysics& lrCar = maRaceCarVehicles[liCar];                                       // 0x826362B8
            if (lrCar.IsCrashing())                                                                // 0x826362C4 lbz +0x710
                continue;
            if (maeRaceCarTypes[liCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)                       // 0x826362D0..E0
                continue;
            if (maRaceCarDrivers[liCar].GetControls()->mbReset)                                    // 0x826362E4..F4 lbz +0x39
                continue;

            for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)                           // 0x826362F8 / 0x82636438..44
            {
                // The console copies the 48-byte record to the stack and reads two of its fields.
                const Wheel::RoadContact& lRoadContact =
                    lrCar.GetWheel(static_cast<EVehicleDrivenWheel>(liWheel)).mRoadContact;        // 0x826362FC..24

                CGS_ASSERT(gbReadSurfaceProperties,
                           "Trying to read surface 'is water' property before surface properties have been loaded");   // :9368

                const u16 lu16Tag      = static_cast<u16>(lRoadContact.mCollisionTag.muValue & 0xFFFFu);   // lhz +0x26 (low halfword)
                const s32 liSurfaceId  = static_cast<s32>((lu16Tag & BrnWorld::KU_COLLISION_MASK_SURFACE_ID) >> 4);   // srwi 4 ; clrlwi 26
                CGS_ASSERT(liSurfaceId < KI_NUM_USED_SURFACES,
                           "static_cast<int32_t>( luSurfaceId ) < KI_NUM_USED_SURFACES");          // :9374

                // [GUARD] the console indexes KAB_SURFACE_IS_WATER past its end after the assert; a
                // stale SURFACELIST.BIN can make the id >= 20 on the host, so the read is bounded.
                // DELETE-WHEN the assert is fatal.
                const bool lbIsWater = (liSurfaceId < KI_MAX_NUM_SURFACES) && KAB_SURFACE_IS_WATER[liSurfaceId];   // lbzx byte_82FB7DF4

                if (lRoadContact.mbIsOnGround                                                      // 0x826363C8 lbz +0x28
                    && (lu16Tag & BrnWorld::KU_COLLISION_FLAG_FATAL) != 0                          // 0x826363D4 rlwinm 18,31,31 == bit 14 SET
                    && (lu16Tag & BrnWorld::KU_COLLISION_FLAG_DRIVEABLE) == 0                      // 0x826363E0 rlwinm 19,31,31 == bit 13 CLEAR
                    && !lbIsWater)                                                                 // 0x826363EC
                {
                    ForceRaceCarCrash(lpRequestOutputInterface, lpVehicleManagerOutputInterface,   // 0x82636414 @0x82635B00
                                      lpVehicleOutputInterface, lpDeformationInterface,
                                      static_cast<EActiveRaceCarIndex>(liCar), lWorldId);
                }

                if (lbIsWater)                                                                     // 0x82636418
                    mStuntOffencesManager.SetCurrentRaceCarState(E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET);   // lwz/ori 0x20/stw +0x28
            }
        }
    }

    // =============================================================================================
    // ApplyShowtimeShunt  @0x82619D28  (129 insns; DWARF h:1308)
    //   (this=r3, r4=lpTrafficVehicle, r5=lpShunter (const VehiclePhysics*), v1=lvfMagnitude -> v126)
    //
    //   0x82619D54  lbz 0x32(r4) -> PhysicalTrafficVehicle +50 == mu8PhysicalType ; >= 2 -> assert
    //               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT" (BrnPhysicalTrafficVehicle.h:382) ; != 0 -> return
    //   0x82619D8C  bl PhysicalTrafficVehicle::GetFullTrafficPhysics ; bl VehiclePhysics::IsBeingSlamedOrShunted ; true -> return
    //   0x82619DA4  r31 = GetFullTrafficPhysics() (called again)
    //   0x82619DB4  GetGraphicsVehicleTransform(shunter) row 3 (+0x30) -> v127 ; same for r31 -> v12
    //   0x82619DDC  v11 = v12 - v127 == trafficPos - shunterPos
    //   0x82619E20  vrlimi128 v11, splat(0.0), 4, 3 -> lane y = 0 (horizontal only)
    //   0x82619E24..40  abs(v11) ; w lane <- x ; vcmpgtfp. vs splat(stru_8208F620.x == FLT_EPSILON) ;
    //               "all lanes false" (bit 26) -> return  ==> proceed iff |dx| > eps || |dz| > eps
    //   0x82619E54  lvx128 0xE0(r31) -> traffic mfMass (VecFloat) ; lvx128 0xE0(r29) -> shunter mfMass
    //   0x82619E64..78  vrefp + 2 Newton steps ; vmul -> ratio = shunterMass / trafficMass
    //   0x82619E7C  vcmpgefp. splat(1.0) >= ratio ; all true (bit 24) ->
    //   0x82619E90..9C     k = min(1.0, max(0.0, ratio*ratio))
    //               else 0x82619EA4..B4  k = min(1.5, max(0.25, ratio))       (vcfsx ..,1 == 0.5 ; 0.5*0.5 ; 1.0+0.5)
    //   0x82619EB8  dot = v11.v11 ; v1 = k * lvfMagnitude
    //   0x82619ECC..E0  r4 = extsb(lwzx this+0x2A0AC) == (s8)mePlayerActiveRaceCarIndex
    //   0x82619ED4  v3 = unk_82FB9C30 (splat 100000.0)
    //   0x82619EEC..10  v2 = v11 * rsqrt(dot) (2 Newton steps) == Normalize(v11)
    //   0x82619F14  bl VehiclePhysics::AddShunt(r31, r4, v1, v2, v3)
    // =============================================================================================
    void VehicleManager::ApplyShowtimeShunt(PhysicalTrafficVehicle* lpTrafficVehicle,
                                            const VehiclePhysics* lpShunter,
                                            VecFloat lvfMagnitude)
    {
        const u32 luType = lpTrafficVehicle->mu8PhysicalType;                                      // lbz +0x32
        CGS_ASSERT(luType < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                                      // BrnPhysicalTrafficVehicle.h:382
        if (luType != PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL)                        // 0x82619D80
            return;

        if (lpTrafficVehicle->GetFullTrafficPhysics()->IsBeingSlamedOrShunted())                   // 0x82619D8C..9C
            return;

        TrafficPhysics* lpTraffic = lpTrafficVehicle->GetFullTrafficPhysics();                     // 0x82619DA4 (r31)

        const Vector3 lvShunterPosition = lpShunter->GetGraphicsVehicleTransform().wAxis;          // 0x82619DB4/DC8 (v127)
        const Vector3 lvTrafficPosition = lpTraffic->GetGraphicsVehicleTransform().wAxis;          // 0x82619DCC/DD4 (v12)
        Vector3 lvOffset = vpu::Subtract(lvTrafficPosition, lvShunterPosition);                    // 0x82619DDC vsubfp
        lvOffset.y = 0.0f;                                                                         // 0x82619E20 vrlimi128 lane y

        if (!(std::fabs(lvOffset.x) > KF_SHOWTIME_SHUNT_MIN_HORIZONTAL_OFFSET
              || std::fabs(lvOffset.z) > KF_SHOWTIME_SHUNT_MIN_HORIZONTAL_OFFSET))                 // 0x82619E24..40
            return;

        const f32 lfRatio = lpShunter->GetMass().x / lpTraffic->GetMass().x;                       // 0x82619E54..78 (vrefp cascade)
        f32 lfScale;
        if (1.0f >= lfRatio)                                                                       // 0x82619E7C vcmpgefp.
        {
            lfScale = lfRatio * lfRatio;                                                           // 0x82619E90
            if (lfScale < 0.0f) lfScale = 0.0f;                                                    // vmaxfp 0
            if (lfScale > 1.0f) lfScale = 1.0f;                                                    // vminfp 1.0
        }
        else
        {
            lfScale = lfRatio;
            if (lfScale < 0.25f) lfScale = 0.25f;                                                  // vmaxfp 0.25
            if (lfScale > 1.5f)  lfScale = 1.5f;                                                   // vminfp 1.5
        }

        const VecFloat lvfScaledMagnitude{ lfScale * lvfMagnitude.x, lfScale * lvfMagnitude.y,     // 0x82619EBC vmulfp128 v1, v10, v126
                                           lfScale * lvfMagnitude.z, lfScale * lvfMagnitude.w };
        const VecFloat lvfSpeedIncreaseToQuit{ KF_SHOWTIME_SHUNT_SPEED_INCREASE_TO_QUIT, KF_SHOWTIME_SHUNT_SPEED_INCREASE_TO_QUIT,
                                               KF_SHOWTIME_SHUNT_SPEED_INCREASE_TO_QUIT, KF_SHOWTIME_SHUNT_SPEED_INCREASE_TO_QUIT };   // unk_82FB9C30

        lpTraffic->AddShunt(lvfScaledMagnitude,                                                    // v1
                            vpu::Normalize(lvOffset),                                              // v2
                            lvfSpeedIncreaseToQuit,                                                // v3
                            static_cast<s8>(mePlayerActiveRaceCarIndex));                          // r4 extsb
    }

    // =============================================================================================
    // ProcessShowtimeShunts  @0x82629F20  (1002 insns; DWARF h:1305 / BrnVehicleManager.cpp:4507)
    //
    // ProcessContactSpies' second call. In the two SHOWTIME game modes, while the player car is in
    // showtime, every traffic vehicle the player touches this frame is shunted away from the player
    // (magnitude 30), and every traffic vehicle a FULLY-PHYSICAL traffic vehicle touches is shunted
    // away from it (magnitude 5) -- the bounce chain of crash mode. ~800 of the 1002 instructions
    // are four inlined copies of GetPhysicsEntityIDFromGlobalEntityID (each with its own BitArray<8>
    // walk and assert builders) -- the host header inline reproduces them.
    //
    //   0x82629F44  lwzx this+0x2A15C -> meCurrentGameModeType ; != 2 && != 16 -> return
    //   0x82629F74..8C  r3 = &maRaceCarVehicles[mePlayerActiveRaceCarIndex] ; vtable +0x10 -> IsPlayerVehicleInShowtime() ; false -> return
    //   RACE-CAR SPIES (r22 == lpContactSpies == &mRaceCarContactQueue; 0x82629FA4 lwz 8(r22) == GetLength()):
    //   0x8262A080  bl BaseEventQueue<RaceCarContact>::GetEvent (96-byte stride) -> r31
    //   0x8262A088  lbz 0(spy) != 1 -> assert "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR" (:4528)
    //   0x8262A0AC..B8  extrwi 14,8 of mEntityIdA == mePlayerActiveRaceCarIndex ?
    //   0x8262A0C0..C8  r26 = lwz 4(spy) mEntityIdB ; owner byte == 2 (TRAFFIC) ?
    //   0x8262A0D4..E0  r24 = &maRaceCarVehicles[player]
    //   0x8262A0E4..A4D8  inlined GetPhysicsEntityIDFromGlobalEntityID(mEntityIdB) -> r29
    //   0x8262A510..18  GetTrafficInterest_0(&mPhysicalTrafficManager (this+44768), extrwi 14,8 of r29) == GetTrafficVehicle(index)
    //   0x8262A524  lvx128 v1, unk_82FB7F30 (splat 30.0) ; bl ApplyShowtimeShunt(this, traffic, r24, v1)
    //   TRAFFIC SPIES (0x8262A53C r19 = r22 + 0x70A0 == &mTrafficContactQueue; lwz 8(r19) == GetLength()):
    //   0x8262A57C  bl BaseEventQueue<TrafficContact>::GetEvent (sub_82368330, 96-byte stride) -> r23
    //   0x8262A584..98  owner(mEntityIdA) == 2 && lbz 4(spy) (owner(mEntityIdB)) == 2 ?
    //   0x8262A5A0..A9E4  inlined conversion of mEntityIdA -> GetTrafficVehicle(index) -> r23 (vehicle A)
    //   0x8262A9FC..AE4C  r26 = lwz 4(spy) (mEntityIdB, read BEFORE r23 is re-purposed) ; inlined conversion -> GetTrafficVehicle -> r3 (vehicle B)
    //   0x8262AE54..7C  lbz 0x32(A) >= 2 -> assert "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT" (:382) ; != 0 -> assert "IsFullyPhysical()" (:391)
    //   0x8262AEA0..AC  lvx128 v1, unk_82FB7FB0 (splat 5.0) ; r5 = lwz 0x1C(A) == A->mpVehicleBody ; bl ApplyShowtimeShunt(this, B, A body, v1)
    // Nothing else is touched: no crash, no scoring, no event -- the physics shunt is the whole effect.
    // =============================================================================================
    void VehicleManager::ProcessShowtimeShunts(const ContactSpy::ContactSpyData* lpContactSpies)
    {
        // 0x82629F48 cmpwi 2 / 0x82629F50 cmpwi 0x10 -- BrnGameState::E_MODE_OFFLINE_SHOWTIME (2)
        // and E_MODE_ONLINE_SHOWTIME (16), BrnGameStateSharedIO.h:64/:80. Compared as raw s32 here
        // because VehicleManager stores the field as s32 (BrnVehicleManager.h:2672) and the physics
        // tree does not pull the game-state header -- the same spelling the sibling note at
        // BrnVehicleManager.h:731 already uses.
        if (meCurrentGameModeType != 2 && meCurrentGameModeType != 16)                             // 0x82629F44..68
            return;

        RaceCarPhysics& lrPlayerCar = maRaceCarVehicles[mePlayerActiveRaceCarIndex];              // 0x82629F74..80
        if (!lrPlayerCar.IsPlayerVehicleInShowtime())                                              // 0x82629F84..9C (vtable +0x10)
            return;

        const VecFloat lvfRaceCarTrafficMagnitude{ KF_SHOWTIME_RACECAR_TRAFFIC_SHUNT_MAGNITUDE, KF_SHOWTIME_RACECAR_TRAFFIC_SHUNT_MAGNITUDE,
                                                   KF_SHOWTIME_RACECAR_TRAFFIC_SHUNT_MAGNITUDE, KF_SHOWTIME_RACECAR_TRAFFIC_SHUNT_MAGNITUDE };   // unk_82FB7F30
        const VecFloat lvfTrafficTrafficMagnitude{ KF_SHOWTIME_TRAFFIC_TRAFFIC_SHUNT_MAGNITUDE, KF_SHOWTIME_TRAFFIC_TRAFFIC_SHUNT_MAGNITUDE,
                                                   KF_SHOWTIME_TRAFFIC_TRAFFIC_SHUNT_MAGNITUDE, KF_SHOWTIME_TRAFFIC_TRAFFIC_SHUNT_MAGNITUDE };   // unk_82FB7FB0

        // ---- race car vs traffic ----------------------------------------------------------------
        const ContactSpy::ContactSpyData::RaceCarContactQueue* lpRaceCarSpies = lpContactSpies->GetRaceCarContacts();
        const s32 liNumRaceCarSpies = lpRaceCarSpies->GetLength();                                 // 0x82629FA4
        for (s32 i = 0; i < liNumRaceCarSpies; ++i)
        {
            const ContactSpy::RaceCarContact& lSpy = lpRaceCarSpies->GetEvent(i);                 // 0x8262A080

            CGS_ASSERT(OwnerOf(lSpy.mEntityIdA.muValue) == BrnWorld::E_ENTITYTYPE_RACECAR,
                       "lSpy.mEntityIdA.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR");            // :4528

            if (IndexOf(lSpy.mEntityIdA.muValue) != static_cast<u32>(mePlayerActiveRaceCarIndex))   // 0x8262A0AC..B8
                continue;
            if (OwnerOf(lSpy.mEntityIdB.muValue) != BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)         // 0x8262A0C4..C8
                continue;

            const u32 luPhysicsIdB = static_cast<u32>(
                GetPhysicsEntityIDFromGlobalEntityID(CgsSceneManager::EntityId(lSpy.mEntityIdB.muValue)));   // 0x8262A0E4..A4D8 (inlined)
            PhysicalTrafficVehicle* lpTraffic =
                mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(IndexOf(luPhysicsIdB)));   // 0x8262A518 @0x825B4880

            ApplyShowtimeShunt(lpTraffic, &lrPlayerCar, lvfRaceCarTrafficMagnitude);               // 0x8262A52C
        }

        // ---- traffic vs traffic -----------------------------------------------------------------
        const ContactSpy::ContactSpyData::TrafficContactQueue* lpTrafficSpies = lpContactSpies->GetTrafficContacts();   // r22 + 0x70A0
        const s32 liNumTrafficSpies = lpTrafficSpies->GetLength();                                 // 0x8262A544
        for (s32 i = 0; i < liNumTrafficSpies; ++i)
        {
            const ContactSpy::TrafficContact& lSpy = lpTrafficSpies->GetEvent(i);                 // 0x8262A57C (sub_82368330)

            if (OwnerOf(lSpy.mEntityIdA.muValue) != BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE         // 0x8262A588..8C
                || OwnerOf(lSpy.mEntityIdB.muValue) != BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)     // 0x8262A594..98 lbz 4(spy)
                continue;

            const u32 luPhysicsIdA = static_cast<u32>(
                GetPhysicsEntityIDFromGlobalEntityID(CgsSceneManager::EntityId(lSpy.mEntityIdA.muValue)));   // 0x8262A5A0..A9E4
            PhysicalTrafficVehicle* lpTrafficA =
                mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(IndexOf(luPhysicsIdA)));   // 0x8262A9F8

            const u32 luPhysicsIdB = static_cast<u32>(
                GetPhysicsEntityIDFromGlobalEntityID(CgsSceneManager::EntityId(lSpy.mEntityIdB.muValue)));   // 0x8262A9FC..AE4C
            PhysicalTrafficVehicle* lpTrafficB =
                mPhysicalTrafficManager.GetTrafficVehicle(static_cast<s32>(IndexOf(luPhysicsIdB)));   // 0x8262AE50

            // The console's inlined GetFullTrafficPhysics() on A: both asserts, then the raw body pointer.
            const u32 luTypeA = lpTrafficA->mu8PhysicalType;                                       // 0x8262AE54 lbz +0x32
            CGS_ASSERT(luTypeA < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                       "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");                                  // :382
            CGS_ASSERT(luTypeA == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_FULL, "IsFullyPhysical()");   // :391

            ApplyShowtimeShunt(lpTrafficB,
                               static_cast<const VehiclePhysics*>(lpTrafficA->mpVehicleBody),      // lwz 0x1C(A)
                               lvfTrafficTrafficMagnitude);                                        // 0x8262AEAC
        }
    }

} // namespace Vehicle
} // namespace BrnPhysics
