// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_WriteOutVehicleStats.cpp
//
// ⭐⭐ THE PER-FRAME PUBLISH CALL SITE -- the function PhysicsModule::Update runs to hand the
// simulated cars to the world side.
//
//   VehicleManager::WriteOutVehicleStats  @0x8263F460  (380 insns)  [was a BRN_CONDUCTOR_GATE]
//   VehicleManager::IsRaceCarHidden       @0x825C2EA0  (104 insns)  [was a TRAP STUB]
//
// Both gates are DELETED in the same commit (LNK2005 is the intended tripwire if either returns).
//
// ⚠️ IsRaceCarHidden WAS WRITTEN OFF AS AN ".ida-exports HOLE" (BrnVehicleManagerContactGeneration
// .cpp:21 -- "no PS3 twin surfaced either"). It is not a hole: it is absent from the JSON export
// set but present in the IDB, and a targeted headless IDA 9.3 pull produced its 104 instructions,
// its three assert strings and its three __LINE__s. Same lesson as the joint-queue accessor and
// EndVehicleTractionLineTests before it -- MISSING JSON != MISSING FUNCTION.
//
// -------------------------------------------------------------------------------------------------
// WriteOutVehicleStats, leg by leg (r18 == this, r19 == lpOutputInterface):
//
//   0x8263F480  assert lpOutputInterface != NULL                     (BrnVehicleManager.cpp:7251)
//   0x8263F4A8  ld/std -- lpOutputInterface->mUsedRaceCars = mUsedRaceCars, ONE doubleword. This
//               single store is what un-gates the whole world-side readback: RaceCarEntityModule::
//               ReadUpdatedActiveRaceCarDataFromPhysics loops per slot on exactly this bitset.
//   0x8263F4C8  the inlined BitArray<8u> first/next-set-bit scan (expressed here through the
//               committed GetFirstNonZeroBit()/GetNextNonZeroBit() pair, whose bit math IS that
//               arithmetic -- the same treatment BrnVehicleManager_ReadUpdatedBodies.cpp gives it).
//   per set slot:
//     0x8263F5C0  lpRaceCar = &maRaceCarVehicles[liRaceCar]           (stride 0x1460, base +0x740)
//     0x8263F5D4  if (liRaceCar == mePlayerActiveRaceCarIndex && mbPlayerCarStuckInCollision)
//                     the "Player car stuck in world. Resetting." assert, which streams the car's
//                     four transform rows into the message                                  (:7269)
//     0x8263F6B0  lbForceReset = lpRaceCar->IsBeached()                   (mfBeachedTime > 3.0f)
//                             || (isPlayer && mbPlayerCarStuckInCollision)
//     0x8263F724  VehicleOutputInterface::UpdateRaceCarState(entityIndex, lpRaceCar,
//                                                            &maRaceCarDrivers[liRaceCar],
//                                                            lbForceReset)
//     0x8263F73C  if player: GetRaceCarState(liRaceCar), assert non-null            (:7286)
//                            mStuntOffencesManager.OutputStuntsInProgress(state, gameEventQueue)
//     0x8263F78C  SetEntityID(liRaceCar, maRaceCarEntityIDs[liRaceCar])
//     0x8263F790  SetRaceCarHidden(liRaceCar, IsRaceCarHidden(liRaceCar))
//     0x8263F7A4  the vtable slot-0 GetSteeringAngle call, purely to assert
//                 "RwMath::IsValid( maRaceCarVehicles[liRaceCar].GetSteeringAngle() )"  (:7294)
//     0x8263F7F4  the four-wheel loop: SetWheelTransform(liRaceCar, wheel,
//                     lpRaceCar->GetWheelsWorldTransfrom(wheel, /*lbHackDontReverseRightWheels*/ false))
//                 (`li r6, 0` is the third argument. ⚠️ GLOSS CORRECTED 2026-08-13: this bool
//                  was mis-named lbApplySteer here and in the header; the PS3 DWARF names it
//                  lbHackDontReverseRightWheels and the body agrees -- steer IS applied to the
//                  front wheels regardless; false ENABLES the left-wheel pi-about-Y mirror.)
//   0x8263FA28  mPhysicalTrafficManager.WriteOutVehicleStats(lpOutputInterface)
//   0x8263FA38  mbPlayerCarStuckInCollision = false     (`stbx r23(0), r18, 0x2A240`)
//
// ⛔⛔ TWO LEGS ARE PARKED, LOUDLY, EACH WITH ITS OWN LOG-ONCE -- never a silent no-op:
//
//   (1) StuntOffencesManager::OutputStuntsInProgress @0x8263B278 IS bodied
//       (BrnStuntOffencesManager.cpp:819) but its declared signature takes
//       `BrnPhysics::RaceCarState*` and `BrnGameState::GameStateModuleIO::GameEventQueue*` --
//       TWO FORWARD-DECLARED PLACEHOLDER CLASSES that are NOT the committed
//       `BrnPhysics::Vehicle::RaceCarState` and `CgsModule::VariableEventQueue<1536,16>` this call
//       site holds. That is a REAL TYPE FORK in the tree (BrnStuntOffencesManager.h:37-40 declares
//       both itself), and its body reaches the state through `reinterpret_cast` + memcpy at raw
//       word offsets +0x41C..+0x438. Calling it from here would mean minting a second
//       reinterpret_cast over the fork; the honest move is to name the fork and park the call.
//       ⇒ FIX: retype StuntOffencesManager::OutputStuntsInProgress onto the committed
//       BrnPhysics::Vehicle::RaceCarState + CgsModule::VariableEventQueue<1536,16> (both are
//       includable from that TU), replace its offset memcpys with the named members
//       (muStuntActionInProgress / mfInProgressBarrelRollAngle / mfInProgressAirSpinAngle /
//       mfInProgressHandbreakTurnAngle / mfInProgressDriftTime / mfInProgressDriftDistance --
//       +0x41C..+0x438 are EXACTLY those six, verified against BrnVehicleEvents.h), then delete
//       this park. It publishes the player's in-progress stunt scalars only; it has no bearing on
//       the pose.
//
//   (2) PhysicalTrafficManager::WriteOutVehicleStats @0x825F0308 (481 insns) is the TRAFFIC half
//       of the same publish and is not reconstructed. It is declared and gated in
//       BrnPhysicsConductorGates.cpp so the console's call is reproduced here and the deferral is
//       audible once per boot, rather than being dropped from the call sequence.
//
// ⚠️ THE CONSOLE'S OWN ASSERTS ARE KEPT AS ASSERTS, with one exception, named. The
// "Player car stuck in world. Resetting." tripwire at :7269 fires on a PC-only precondition (the
// stuck-in-collision test chain that sets mbPlayerCarStuckInCollision is
// DoPlayerStuckLineTests, still gated, so the flag's producer is not the console's), and it is a
// PER-FRAME assert inside a per-frame publish. Per this wave's established pattern (three
// precedents) it is degraded to a log-once [FLAG PC bring-up] gate. The other three asserts
// (:7251 null check, :7286 null state, :7294 IsValid steering) are cheap and are kept.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnPhysics
{
namespace Vehicle
{

// -------------------------------------------------------------------------------------------------
// @0x825C2EA0  VehicleManager::IsRaceCarHidden   (DWARF BrnVehicleManager.h -- asserts cite :1941/
// :1948/:1949, which are the X360 header's own __LINE__s)
//
// Body from the headless IDA pull:
//   0x825C2EB8  assert (liRaceCarIndex >= 0 && liRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS)
//   0x825C2EE8  the BitArray<8u> bounds assert (CgsBitArray.h:203) -- the container's own, hoisted
//               to the call site here exactly as ReadUpdatedBodies/CrashingRaceCarInterface do
//               (the committed BitArray header is deliberately assert-free).
//   0x825C2F9C  lbHidden = mHiddenRaceCars.IsBitSet(liRaceCarIndex)
//               (`srwi r11,r28,6 ; addi r11,r11,0x15D4 ; slwi r11,r11,3 ; ldx` == this + 44704)
//   0x825C2FD4  when hidden, two consistency asserts: the car must be a NETWORK car
//               (maeRaceCarTypes[i] == 2 == E_RACE_CAR_TYPE_NETWORK, `4*(i+0x2B28)` == +44192)
//               and must not be the local player (`0x2A0AC` == +172204 ==
//               mePlayerActiveRaceCarIndex).
// -------------------------------------------------------------------------------------------------
bool VehicleManager::IsRaceCarHidden(s32 liRaceCarIndex)
{
    CGS_ASSERT(liRaceCarIndex >= 0 && liRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "liRaceCarIndex >= 0 && liRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < 8u, "invalid index");

    if (liRaceCarIndex < 0 || liRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        // ⚠️ DIVERGENCE (named): the console reads the bitset regardless and would fault/alias on an
        // out-of-range index; on this build the same read is a stack smash. The assert above is the
        // console's; this early-out is the guard.
        return false;
    }

    const bool lbHidden = mHiddenRaceCars.IsBitSet(static_cast<u32>(liRaceCarIndex));

    if (lbHidden)
    {
        CGS_ASSERT(maeRaceCarTypes[liRaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK,
                   "maeRaceCarTypes[liRaceCarIndex] == BrnWorld::E_RACE_CAR_TYPE_NETWORK");
        CGS_ASSERT(liRaceCarIndex != static_cast<s32>(mePlayerActiveRaceCarIndex),
                   "leRaceCarIndex != mePlayerActiveRaceCarIndex");
    }

    return lbHidden;
}

// -------------------------------------------------------------------------------------------------
// @0x8263F460  VehicleManager::WriteOutVehicleStats   (DWARF BrnVehicleManager.cpp:7246)
// -------------------------------------------------------------------------------------------------
void VehicleManager::WriteOutVehicleStats(VehicleOutputInterface* lpOutputInterface)
{
    CGS_ASSERT(lpOutputInterface != 0, "lpOutputInterface != NULL");
    if (lpOutputInterface == 0)
    {
        return;
    }

    // ⭐⭐ THE ONE STORE THE WHOLE READBACK IS GATED ON. `ld r10,0(r9) ; std r10,0(r19)`.
    lpOutputInterface->GetUsedCarsBitArray() = mUsedRaceCars;

    for (s32 liRaceCar = mUsedRaceCars.GetFirstNonZeroBit();
         liRaceCar >= 0 && liRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         liRaceCar = mUsedRaceCars.GetNextNonZeroBit(liRaceCar))
    {
        RaceCarPhysics* lpRaceCar = &maRaceCarVehicles[liRaceCar];

        const bool lbIsLocalPlayer = (liRaceCar == static_cast<s32>(mePlayerActiveRaceCarIndex));
        const bool lbPlayerStuck   = lbIsLocalPlayer && mbPlayerCarStuckInCollision;

        if (lbPlayerStuck)
        {
            // [FLAG PC bring-up] the console fires a full assert here (BrnVehicleManager.cpp:7269),
            // streaming the car's transform into the message. Degraded to a log-once: its
            // precondition (mbPlayerCarStuckInCollision) is produced by DoPlayerStuckLineTests,
            // which is still a conductor gate on this build, so a per-frame dialog here would be
            // reporting the absence of a producer rather than a stuck car.
            // DELETE-WHEN the player-stuck traction-line pair lands.
            static bool sbLoggedPlayerStuck = false;
            if (!sbLoggedPlayerStuck)
            {
                sbLoggedPlayerStuck = true;
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] WriteOutVehicleStats: player car stuck in world "
                       "(BrnVehicleManager.cpp:7269 assert degraded to log-once)\n";
            }
        }

        const bool lbForceReset = lpRaceCar->IsBeached() || lbPlayerStuck;

        // ⚠️ The console indexes maRaceCarStates by the ENTITY INDEX here and by liRaceCar
        // everywhere else in this loop. Reproduced; see the FLAG on the declaration.
        const CgsSceneManager::EntityId lEntityID(maRaceCarEntityIDs[liRaceCar].muValue);
        lpOutputInterface->UpdateRaceCarState(static_cast<s32>(lEntityID.GetEntityIndex()),
                                              lpRaceCar,
                                              &maRaceCarDrivers[liRaceCar],
                                              lbForceReset);

        if (lbIsLocalPlayer)
        {
            RaceCarState* lpPlayerRaceCarState = lpOutputInterface->GetRaceCarState(liRaceCar);
            CGS_ASSERT(lpPlayerRaceCarState != 0, "lpPlayerRaceCarState != NULL");

            // ⛔ PARKED -- see park (1) in this file's banner. The callee exists and is bodied; its
            // declared parameter types are a fork of the committed ones.
            (void)lpPlayerRaceCarState;
            static bool sbLoggedStuntPark = false;
            if (!sbLoggedStuntPark)
            {
                sbLoggedStuntPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] WriteOutVehicleStats: StuntOffencesManager::"
                       "OutputStuntsInProgress @0x8263B278 NOT called -- its declared arg types "
                       "(BrnPhysics::RaceCarState / BrnGameState::GameStateModuleIO::"
                       "GameEventQueue) are a fork of the committed BrnPhysics::Vehicle::"
                       "RaceCarState / CgsModule::VariableEventQueue<1536,16>. Player stunt "
                       "scalars are not published. DELETE-WHEN that signature is retyped.\n";
            }
        }

        lpOutputInterface->SetEntityID(liRaceCar, maRaceCarEntityIDs[liRaceCar]);
        lpOutputInterface->SetRaceCarHidden(liRaceCar, IsRaceCarHidden(liRaceCar));

        // The console's validity test is `vcmpeqfp. v0, v0, v0` over the 16-byte GetSteeringAngle
        // return -- i.e. the self-equality NaN check, which is what RwMath::IsValid is. Spelled as
        // that same self-comparison here because this tree's GetSteeringAngle returns a scalar f32
        // (see the divergence note in BrnVehicleOutputInterface_UpdateRaceCarState.cpp).
        const f32 lfSteeringAngle = lpRaceCar->GetSteeringAngle().x;
        CGS_ASSERT(lfSteeringAngle == lfSteeringAngle,
                   "RwMath::IsValid( maRaceCarVehicles[liRaceCar].GetSteeringAngle() )");

        // ⭐⭐ UNPARKED 2026-08-13 (wheel-transform wave) -- park (3) retired, DELETE-WHEN
        // honoured: SimpleVehiclePhysics::GetWheelsWorldTransfrom @0x825D8878 is BODIED
        // (BrnSimpleVehiclePhysics.cpp, from the operand-level decode bank). The console's
        // four-wheel publish loop @0x8263F7F4, verbatim: one call per wheel, bool false
        // (`li r6, 0` -- the left-wheel mirror ACTIVE; see the corrected gloss in the banner).
        //
        // ⚠️ The park's "third site" flag RESOLVED, and the answer is: THERE IS NO THIRD SITE.
        // A full-image scan of all 30,084 X360 exports (this wave) finds NO store to
        // RaceCarState+0x446..0x449 anywhere except the RaceCarState COPY ctor @0x8220A4C0
        // (and no stbx/addi-computed store either); the PS3 set agrees (+1094 has only
        // readers). The render-side wheel EXISTS on the console comes from the DEFORMATION
        // half of the readback instead -- ActiveRaceCar::UpdateWheelPhysicsState @0x822B8738,
        // fed by the deformation output's per-wheel data, a leg that is parked (L3 of
        // ReadUpdatedActiveRaceCarDataFromPhysics). RaceCarState::mabWheelExists is left
        // untouched here, exactly as the console leaves it; the render module carries the
        // bring-up seam for the exists flag (see BrnRaceCarEntityModule.cpp).
        for (u8 lu8Wheel = 0; lu8Wheel < 4; ++lu8Wheel)
        {
            lpOutputInterface->SetWheelTransform(
                static_cast<u8>(liRaceCar), lu8Wheel,
                lpRaceCar->GetWheelsWorldTransfrom(
                    static_cast<EVehicleDrivenWheel>(lu8Wheel),
                    /*lbHackDontReverseRightWheels*/ false));
        }

        // ---- [move-probe] DELETE-WHEN motion is confirmed on a booted run --------------------
        // A per-boot, once-every-N-publishes readout of the PLAYER's PHYSICS-side position, so
        // that movement can be measured without reading it off a stable-looking screen. It prints
        // the source the publish leg actually reads (the RaceCarPhysics transform), not the
        // rendered pose.
        //
        // ⚠️⚠️ THIS USED TO SAY `liRaceCar == 0`, AND THAT MADE IT A LYING DIAGNOSTIC.
        // Race car 0 is NOT the local player in this build: three cars are created at boot
        // (`HIDE_ONLINE: Created race car 0/1/2`) and the drive runs of 2026-08-16 all report
        // `playerIdx 2`. Cars 0 and 1 sit at the junkyard spawn for the whole session, so the
        // probe faithfully printed `pos 2986.97 -3.20 -2011.40 vel 0 0 0 gas 0.000000` for
        // ~19,000 consecutive frames of a run in which the player's car was driving at 29.7 m/s
        // 165 m away -- and that line was then quoted, in good faith, as evidence that the car
        // does not move. It is the same failure mode as `staticGroups=5`: a number that is
        // correct about the wrong subject. The label now carries the index so the line cannot be
        // read as "the car" ever again.
        if (lbIsLocalPlayer)
        {
            static u32 suProbeCounter = 0;
            if ((suProbeCounter % 300u) == 0u)
            {
                const Matrix44Affine lProbeTransform = lpRaceCar->GetTransform();
                const Vector3        lvProbeVelocity = lpRaceCar->GetLinearVelocity();
                *CgsDev::Log::gpDebugPrint
                    << "[move-probe] player car" << liRaceCar << " physics pos "
                    << lProbeTransform.wAxis.x << " "
                    << lProbeTransform.wAxis.y << " " << lProbeTransform.wAxis.z
                    << " vel " << lvProbeVelocity.x << " " << lvProbeVelocity.y << " "
                    << lvProbeVelocity.z
                    << " speedMPH " << lpRaceCar->GetSpeedMPH().x
                    << " gas " << maRaceCarDrivers[liRaceCar].mControls.mfGas << "\n";
            }
            ++suProbeCounter;
        }
        // ---- end [move-probe] ----------------------------------------------------------------
    }

    // The traffic half of the same publish -- gated (park (2) in this file's banner).
    mPhysicalTrafficManager.WriteOutVehicleStats(lpOutputInterface);

    mbPlayerCarStuckInCollision = false;
}

}
}
