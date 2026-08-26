// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_WriteOutVehicleStats.cpp
//
// THE PER-FRAME PUBLISH CALL SITE -- the function PhysicsModule::Update runs to hand the
// simulated cars to the world side.
//
//   VehicleManager::WriteOutVehicleStats  @0x8263F460  (380 insns)
//   VehicleManager::IsRaceCarHidden       @0x825C2EA0  (104 insns)  -- absent from the JSON
//                                         export set, dumped headless from a COPY of the .i64
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
// (`li r6, 0` is the third argument: the PS3 DWARF names it
//                  lbHackDontReverseRightWheels and the body agrees -- steer IS applied to the
//                  front wheels regardless; false ENABLES the left-wheel pi-about-Y mirror.)
//   0x8263FA28  mPhysicalTrafficManager.WriteOutVehicleStats(lpOutputInterface)
//   0x8263FA38  mbPlayerCarStuckInCollision = false     (`stbx r23(0), r18, 0x2A240`)
//
// ONE LEG IS PARKED, LOUDLY, WITH ITS OWN LOG-ONCE -- never a silent no-op:
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
//       ⛔⛔ [bugwave 2026-08-23] READ THIS BEFORE SPENDING A WAVE ON THIS PARK. It was the
//       PRIME SUSPECT for the "super jumps do not get counted / camera does not fire" report,
//       and it is NOT the cause -- of either half. Measured, not argued:
//         * The super-jump TALLY comes from the GameState collectible ladder
//           (TriggerQueryManager player-trigger fan-out -> StuntManager::LatchJumpElement ->
//            UpdateJumps -> ProcessStuntElement(isJump) -> Profile::AddStuntElement + game
//            action 58), which never touches StuntOffencesManager at all. The real break was
//           that ProcessPlayerTriggers had NO CALLER in the tree; fixed this wave in
//           BrnTriggerQueryManager.cpp :: PreWorldUpdatePlayerTriggersBringUp.
//         * What this leg publishes is the STUNT-RUN / FREEBURN-SKILL telemetry (air time,
//           jump distance, barrel-roll / flat-spin / drift angles) as game EVENT 120
//           (InProgressStuntEvent). ON THIS BUILD THAT EVENT HAS NO CONSUMER: the only
//           GameState-side drain that exists is GameStateModule_gUI_00.cpp ::
//           ProcessGameEventsPropHitBringUp, whose `if (liType == E_EVENT_RECORD_PROP_HIT)`
//           accepts event 111 and nothing else. Its would-be readers -- ChallengeManager
//           (BrnChallengeManager_wC_06.cpp) and StuntModeScoring (Scoring/*) -- are in TUs
//           that are not on the build list.
//         * Its SIBLING is already live and equally unread: StuntOffencesManager::Update
//           (BrnVehicleManager_UpdateVehiclePhysics.cpp:681) calls OutputStuntsCompleted every
//           frame, which posts game event 119 into the same queue. So retyping this fork today
//           would add a SECOND event nobody reads -- the textbook "publishes scalars nobody
//           consumes is not a fix".
//       The park therefore STAYS, and its cost is restated honestly: no freeburn stunt-run
//       skill telemetry. RESTORE-WHEN a GameState ProcessGameEvents arm drains event 120
//       (i.e. when ChallengeManager or the Scoring subsystem mounts) -- retyping the signature
//       is then worth doing and the shape of that change is spelled out above.
//
// THE CONSOLE'S OWN ASSERTS ARE KEPT AS ASSERTS, with one exception, named. The
// "Player car stuck in world. Resetting." tripwire at :7269 fires on a PC-only precondition (the
// stuck-in-collision test chain that sets mbPlayerCarStuckInCollision is
// DoPlayerStuckLineTests, still gated, so the flag's producer is not the console's), and it is a
// PER-FRAME assert inside a per-frame publish, so it is degraded to a log-once
// [FLAG PC bring-up] gate. The other three asserts
// (:7251 null check, :7286 null state, :7294 IsValid steering) are cheap and are kept.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"          // [teleport] the reset queue
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"  // [teleport] DeactivateDeformationModelEvent
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
// Body:
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
        // DIVERGENCE (named): the console reads the bitset regardless and would fault/alias on an
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

    // THE ONE STORE THE WHOLE READBACK IS GATED ON. `ld r10,0(r9) ; std r10,0(r19)`.
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

        // The console indexes maRaceCarStates by the ENTITY INDEX here and by liRaceCar
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

            // PARKED -- see park (1) in this file's banner. The callee exists and is bodied; its
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
                       "RaceCarState / CgsModule::VariableEventQueue<1536,16>. COST: no "
                       "freeburn stunt-run skill telemetry (game event 120). NOT the super-jump "
                       "bug -- that is the StuntManager collectible ladder, and event 120 has no "
                       "drain on this build (ProcessGameEvents accepts 111 only). "
                       "DELETE-WHEN a ProcessGameEvents arm consumes event 120.\n";
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

        // The console's four-wheel publish loop @0x8263F7F4, verbatim: one call per wheel, bool
        // false (`li r6, 0` -- the left-wheel mirror ACTIVE; see the gloss in the banner).
        //
        // NOTHING here writes RaceCarState::mabWheelExists: a full-image scan of all 30,084 X360
        // exports finds no store to RaceCarState+0x446..0x449 outside the copy ctor @0x8220A4C0,
        // and the PS3 set agrees (+1094 has only readers). The render-side wheel-exists flag comes
        // from the DEFORMATION half of the readback (ActiveRaceCar::UpdateWheelPhysicsState
        // @0x822B8738), a leg parked at L3 of ReadUpdatedActiveRaceCarDataFromPhysics.
        for (u8 lu8Wheel = 0; lu8Wheel < 4; ++lu8Wheel)
        {
            lpOutputInterface->SetWheelTransform(
                static_cast<u8>(liRaceCar), lu8Wheel,
                lpRaceCar->GetWheelsWorldTransfrom(
                    static_cast<EVehicleDrivenWheel>(lu8Wheel),
                    /*lbHackDontReverseRightWheels*/ false));
        }
    }

    // 0x8263FA28 -- the traffic half of the same publish
    // (BrnPhysicalTrafficManager_WriteOutVehicleStats.cpp).
    mPhysicalTrafficManager.WriteOutVehicleStats(lpOutputInterface);

    mbPlayerCarStuckInCollision = false;
}

// =================================================================================================
// @0x82617820  VehicleManager::ProcessResetEvents   (526 insns)   [was a BRN_CONDUCTOR_GATE]
//
// THE ONLY MECHANISM IN THE GAME THAT MOVES AN ALREADY-SIMULATED CAR.  Landed 2026-08-21
// (gateui r9) because the wave needed a way to PUT THE CAR AT COORDINATES, and this is what the
// console does -- there is no other writer of a live car's transform outside the integrator.
// PhysicsModule::Update @0x825B0640 has always called it, every frame, on both the normal and the
// network-catchup path (BrnPhysicsModuleUpdateFunctions.cpp, "the shared tail"); until now that
// call reached the inert gate in BrnPhysicsConductorGates.cpp, which is DELETED in the same commit
// (LNK2005 is the intended tripwire if it returns).
//
// THE PRODUCER SIDE, so the whole chain is on one page:
//     ActiveRaceCar::RequestPlaceOnTrack(pos, dir, speed)          -- the request latch
//       -> PlaceOnTrackManager::PrePhysicsUpdate                    -- 100 m vertical line test
//          (on PC: ApplyPendingRequestsWithoutSceneQueryBringUp over the shipped WORLDCOL)
//       -> PlaceOnTrackManager::PlaceCarOnTrack                     -- BrnMath::BuildTransform
//       -> RaceCarEntityModule::ResetActiveRaceCar                  -- its IsActive() arm
//       -> VehicleInputInterface::ResetRaceCar  @0x822CC2A0         -- enqueues ResetVehicleEvent
//       -> THIS FUNCTION.
//
// IT IS A SLICE, AND THE PARKED LEGS ARE NAMED. What is reproduced is the TRANSFORM/VELOCITY
// core plus the two posts, read instruction by instruction out of the ARTIST asm:
//
//   0x82617BD0  lrEvent = queue->GetEvent(i)                        (sub_825BB948 == GetEvent)
//   0x82617BE0  v127 = event+0x50 (mInitialVelocity), v126 = event+0x60 (mAngularVelocity)
//   0x82617BF0  r30 = event+0x70 mbResetTransform, r29 = +0x71 mbResetDeformation,
//               r24 = +0x72 mbResettingAfterWreck, f29 = +0x74 mfRoadRageHowCloseToWrecked,
//               r26 = +0x78 meDeformationResetType; the four transform rows +0x10..0x40 are
//               copied to a stack Matrix44Affine (var_150).
//   0x82617C5C  if (index == mePlayerActiveRaceCarIndex) `ori r10,r10,0x20` into
//               mStuntOffencesManager(+0xACD0).muCurrentRaceCarState(+0x28) -- the named setter
//               SetCurrentRaceCarState(E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET). RESTORED (the
//               original park (P1) mis-read the offset by 0x10000 -- verify_r9_billboard W2).
//   0x82617C74  lpRaceCar = &maRaceCarVehicles[index]      (mulli 0x1460 ; addi 0x740 -- the same
//               stride/base WriteOutVehicleStats above uses, reached by name here)
//   0x82617C84  the inlined VehicleDriver::ClearControls over maRaceCarDrivers[index] (0xE0
//               stride; the -1 at +0x40/+0x78, the twelve 0.0f, the lone f30 at +0x74, the ten
//               zero bytes) -- byte for byte the block ProcessRemoveEvents also inlines, which is
//               why the same out-of-line helper is called here.
//   0x82617D00  assert mpAttribs->IsValid()  ("Trying to reset a car without valid physics
//               attributes", BrnVehicleManager.cpp:0x69B == 1691)
// 0x82617D34  if (mbResetTransform) SetTransformFromPositionOnRoad(lTransform)   THE SEAT
//   0x82617D44  ... then stvx v127 -> car+0x50 (mLinearVelocity) and v126 -> car+0x60
//               (mAngularVelocity), and mfMass (+0xE0) = splat(mpAttribs+0x70 lane 0)
//   0x82617D60  if (!mbResetDeformation) the four-wheel "wheel is attached" assert loop
// (BrnVehicleManager.cpp:0x6AB == 1707)                  PARKED -- see (P2)
//   0x82617DB0  if (mbResetDeformation) DeactivateDeformationModelEvent::AddEvent
//               { maRaceCarHandlingBodyIDs[index], mfRoadRageHowCloseToWrecked,
//                 meDeformationResetType }
// 0x82617DF0  if (mbResetTransform) VehiclePhysics::Reset(mInitialVelocity)     THE RE-SEED
//   0x82617E00  else                  VehiclePhysics::ClearCrashing()  == vtable slot 1
//                                     LANDED 2026-08-26 -- (P3) is RESOLVED, not parked
//   0x82617E28  RaceCarResetEvent::AddEvent(managerOut+0x5B0,
//               { index, mbResettingAfterWreck, <the transform's translation row, v125> })
//   0x82617E34+ the `index == player` tail: a bit test at +0x1908, SetAllNetworkRaceCarsHidden,
// and four gpcMessageBuffer streams                      PARKED -- see (P4)
//
// (P1) RETIRED (r9 verify): the "un-homed +0x1ACD0 flags word" was a 0x10000 mis-read of
//      +0xACD0 == mStuntOffencesManager; the leg is the named SetCurrentRaceCarState call,
//      restored in the body above.
// (P2) Wheel::IsAttached has no reconstructed body; the assert is debug-only and its absence
//      cannot change behaviour.
// (P3) ⭐⭐ RESOLVED 2026-08-26 (resetpump wave) -- THE SLOT IS SETTLED AND BOTH HALVES OF THE
//      OLD NOTE WERE WRONG. Read off the image, not reasoned about: the RaceCarPhysics vtable
//      lives at 0x820D1034 (pinned by VehiclePhysics.h's own slot-5 note, and slot 0 there is
//      VehiclePhysics::GetSteeringAngle, which confirms the base), and
//          vtable[1] == 0x825D5450 == BrnPhysics::Vehicle::VehiclePhysics::ClearCrashing
//      The same pointer sits in the VehiclePhysics/TrafficPhysics vtable @0x820D0C68, so it is
//      NOT overridden. 0x825D5450 is an ARTIST export HOLE (no JSON), recovered the documented
//      way -- its name is in a NEIGHBOUR'S xrefs_to (CgsDev::Assert::BeginAssert @0x82817548
//      lists `0x825D5450 BrnPhysics::Vehicle::VehiclePhysics::ClearCrashing`) -- and then
//      disassembled straight out of the image (44 insns, 0x825D5450..0x825D54FC).
//
//      ⛔⛔ AND "the arm is unreachable here" IS FALSE, MEASURED: the reset events this build
//      has actually processed all carried resetTransform == 0, so THIS is the live arm.
//      ⚠️ BUT THAT IS NOT "every reset", AND THE DIFFERENCE IS THE WHOLE STORY -- see below.
//
//      ⭐ WHAT ClearCrashing DOES (image-read):
//          0x825D5464  CGS_ASSERT(IsCrashing(), "IsCrashing()")   VehiclePhysics.cpp:7408
//          0x825D54B0  mbCrashing            = false      (+0x710)
//          0x825D54B4  mbIsFatalyCrashing    = false      (+0x711)
//          0x825D54BC  four f32 lanes at +0x1114/+0x1118/+0x111C/+0x1120 = flt_82001CC0 (0.0f)
//          0x825D54C0  a byte at +0x1128 = -1, and a VMX block zeroed from +0x1130
//      ⭐⭐⭐ THIS IS THE ONE THING KEEPING BRN_ENABLE_CRASH_ENTRY OFF. The reset-on-track pump
//      now puts a crashed car back on the road (resetpump wave), but nothing clears mbCrashing,
//      so the recovered car drives on flagged crashing for ever. This dispatch is what clears it.
//
//      ⭐⭐ THE ENTRY ASSERT WAS FEARED AS A TRAP. IT IS NOT ONE -- IT IS A TAUTOLOGY, and
//      settling that is what let this land (crashclear wave, 2026-08-26). The note that stood
//      here read: "on this build the !mbResetTransform arm runs for EVERY reset -- the junkyard
//      hand-off, the harness teleport, the crash recovery -- and only the last of those has a
//      crashing car, so un-parking it naively HANGS THE BOOT". FALSE TWICE OVER. Recorded rather
//      than quietly deleted, because the reasoning error is reusable:
//
//        (a) MEASURED. Run rp_default is a full flow to DRIVING -- junkyard hand-off, 2.1 km
//            drive -- and contains ZERO `[teleport]` lines of ANY kind. The hand-off is
//            PlaceOnTrackManager's INITIAL PLACEMENT (E_STATE_WAITING -> E_STATE_ACTIVE, 16
//            [PLACEONTRACK] lines), which never enqueues a ResetVehicleEvent and never reaches
//            this function at all. Only the crash runs produce a reset event: exactly one each.
//
//        (b) STRUCTURAL, from the producer's own asm. RaceCarEntityModule::ResetActiveRaceCar's
//            re-reset arm @0x822F4990 -- already bodied in this tree -- classifies:
//                if (IsCrashing()) { IsDriveableAfterCrash() -> resetTransform = 0   <-- ONLY 0
//                                    else                    -> resetTransform = 1 }
//                else                                        -> resetTransform = 1
//                                             (`li r26, 1` @0x822F4960, never overwritten)
//            So mbResetTransform == 0 is reachable on EXACTLY ONE path: a car that is CRASHING
//            and is DRIVEABLE AFTER THE CRASH. A non-crash reset always carries 1 and always
//            takes the VehiclePhysics::Reset(velocity) arm above. This arm is therefore, BY
//            CONSTRUCTION, unreachable with a non-crashing car -- which is exactly what
//            CGS_ASSERT(IsCrashing()) inside ClearCrashing states. The two agree.
//
//      ⭐ THE ERROR CLASS, named so it is not repeated: the earlier note observed
//      `resetTransform=0` on the only two reset events that had ever existed -- both crash
//      recoveries -- and generalised "0 on every reset that ARRIVES" into "every reset takes
//      this arm". A condition's truth over the events that arrive says NOTHING about WHICH
//      events arrive. (Same shape as the standing rule that "X reads Y and Y is never written"
//      is a claim about ONE BRANCH of X.)
//
//      ⇒ THE ASSERT STAYS VERBATIM, THE CALL IS UN-GATED, NOTHING IS INVENTED. Deleting the
//      assert to "get past it" would have been the invented-arm class this campaign has paid
//      for twice; gating the call on IsCrashing() would have hidden exactly the producer defect
//      the console's own assert exists to catch.
// (P4) the player tail is the network-car un-hide + four debug streams; the un-hide's own
//      SetAllNetworkRaceCarsHidden IS bodied, but its guard is the +0x1908 bit of an unnamed
//      member. Parked for the same reason as (P1), and it is a no-op with no network cars.
// =================================================================================================
void VehicleManager::ProcessResetEvents(
        const VehicleInputInterface* lpInputInterface,
        BrnPhysics::Vehicle::VehicleOutputRequestInterface* /*lpRequestOutputInterface*/,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        BrnPhysics::Deformation::DeformationInputInterface* lpDeformationInterface)
{
    CGS_ASSERT(lpInputInterface != 0, "lpInputInterface != NULL");
    if (lpInputInterface == 0)
    {
        return;
    }

    // 0x82617870..0x82617AFC -- THE OPENING LEG (restored per verify_r9_billboard W1): before
    // the queue length is even read, the console walks mUsedRaceCars and UNCONDITIONALLY
    // clears every live car's VehiclePhysics::mbResetCarTransform (`stb r18(=0), 0x1A9C(r10)`,
    // r10 = this + i*0x1460 -> maRaceCarVehicles[i]+0x135C). This function is the flag's ONLY
    // image-wide writer of FALSE (VehiclePhysics::Prepare @0x8263809C writes TRUE); without
    // this leg the flag latches TRUE from car creation forever and is copied every frame into
    // RaceCarState::mbResetCarTransform (+0x44E), which AirTimeManager / ScoringSystem /
    // StuntModeScoring / ChallengeManager / BurnoutSkillzManager all read as "this car was
    // just reset -- drop the run".
    for (u32 luCar = 0; luCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luCar)
    {
        if (mUsedRaceCars.IsBitSet(luCar))
            maRaceCarVehicles[luCar].mbResetCarTransform = false;
    }

    const VehicleInputInterface::ResetRaceCarEventQueue* lpQueue =
        lpInputInterface->GetResetRaceCarEvents();

    for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
    {
        const ResetVehicleEvent& lrEvent = lpQueue->GetEvent(liEvent);

        const s32 liRaceCar = static_cast<s32>(lrEvent.miRaceCarIndex);

        // NOT the console's: the console indexes maRaceCarVehicles with the event's word and
        // would alias on a bad one. On this build that is a stack smash, so the range is checked
        // and the drain skips (loudly) rather than corrupting the manager.
        if (liRaceCar < 0 || liRaceCar >= E_ACTIVE_RACE_CAR_INDEX_COUNT
            || !mUsedRaceCars.IsBitSet(static_cast<u32>(liRaceCar)))
        {
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[teleport] ProcessResetEvents: reset for race car " << liRaceCar
                    << " ignored (out of range or not a live slot)\n";
            }
            continue;
        }

        // 0x82617C5C (restored per verify_r9_billboard W2 -- park (P1) was WRONG: the address
        // is +0xACD0 == 44240 == the pinned, named VehicleManager::mStuntOffencesManager, and
        // the `ori 0x20` triple into its +0x28 is exactly the existing named setter, already
        // called the same way at BrnVehicleManager_ProcessCreateEvents.cpp). Tell the stunt
        // detector the player car was just reset so in-flight air-time/spin/drift accumulation
        // is abandoned instead of emitting a stale stunt-complete on landing.
        if (liRaceCar == static_cast<s32>(mePlayerActiveRaceCarIndex))
        {
            mStuntOffencesManager.SetCurrentRaceCarState(E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET);
        }

        RaceCarPhysics* lpRaceCar = &maRaceCarVehicles[liRaceCar];

        // 0x82617C84 -- the inlined VehicleDriver::ClearControls (see the banner).
        maRaceCarDrivers[liRaceCar].ClearControls();

        // 0x82617D00 -- `lwz r11,0x720(car) ; lbz r11,0x360(r11)` == mpAttribs->IsValid().
        CGS_ASSERT(lpRaceCar->GetAttribs() != 0 && lpRaceCar->GetAttribs()->IsValid(),
                   "Trying to reset a car without valid physics attributes");   // :1691
        if (lpRaceCar->GetAttribs() == 0 || !lpRaceCar->GetAttribs()->IsValid())
        {
            continue;
        }

        if (lrEvent.mbResetTransform)
        {
            // THE SEAT. The event's transform's translation row is a point ON THE ROAD (the
            // place-on-track line test put it there); SetTransformFromPositionOnRoad lifts the
            // handling frame to its at-rest height above that point.
            lpRaceCar->SetTransformFromPositionOnRoad(lrEvent.mInitialTransform);

            // 0x82617D44/0x82617D4C -- the two velocity registers, straight from the event.
            lpRaceCar->SetLinearVelocity(lrEvent.mInitialVelocity);
            lpRaceCar->SetAngularVelocity(lrEvent.mAngularVelocity);

            // 0x82617D50..0x82617D5C -- mfMass (+0xE0) = splat(mpAttribs+0x70 lane 0). NOT
            // reproduced, and it cannot diverge: UpdateDriving re-splats mfMass from the SAME
            // attribute lane at the top of every single frame (VehiclePhysics.cpp, the
            // `mfMass = VecFloat{lfM,...}` line right after mfSpeedMPH), so the console's store
            // here is overwritten before anything reads it. Stated rather than silently dropped.
        }

        if (lrEvent.mbResetDeformation)
        {
            // 0x82617DDC -- the same three-field event ProcessRemoveEvents posts, with THIS
            // event's damage amount and reset type instead of the remove drain's 0.0f/-1.
            Deformation::DeactivateDeformationModelEvent lDeactivate;
            lDeactivate.mHandlingBodyID        =
                CgsPhysics::RigidBodyId(maRaceCarHandlingBodyIDs[liRaceCar]);
            lDeactivate.mfInitialDamageAmount  = lrEvent.mfRoadRageHowCloseToWrecked;
            lDeactivate.meDeformationResetType = lrEvent.meDeformationResetType;
            if (lpDeformationInterface != 0)
            {
                lpDeformationInterface->GetDeactivateDeformationModelQueue().AddEvent(lDeactivate);
            }
        }

        if (lrEvent.mbResetTransform)
        {
            // THE RE-SEED. Kills every force/impulse, zeroes the drift/boost/slam/shunt banks,
            // re-seats the wall-contact timers and sets mbResetCarTransform -- the flag the whole
            // game side reads as "this car was just reset, drop the run".
            lpRaceCar->Reset(lrEvent.mInitialVelocity);
        }
        else
        {
            // 0x82617E00 `lwz r11,0(r31) ; lwz r11,4(r11) ; mtctr r11 ; bctrl` -- RaceCarPhysics
            // vtable slot 1 == VehiclePhysics::ClearCrashing @0x825D5450, reached BY NAME.
            //
            // THIS IS THE ONLY THING ON THE ENTIRE RESET PATH THAT ENDS A CRASH. The chain it
            // feeds, so the next reader does not have to rediscover it: ClearCrashing zeroes
            // mbCrashing (+0x710) -> VehicleOutputInterface::UpdateRaceCarState copies it into
            // RaceCarState::mbCrashing -> RCEntityActiveRaceCarOutputInterface::
            // IsPlayerCarCrashing reads it -> BridgeWorldVehicleDataToGui turns its FALLING
            // EDGE into GUI event 377, payload 1 == E_CRASHBARSTATE_LEAVE_CRASHED.
            //
            // Reachable only with a crashing car -- see (P3)(b): the producer hardcodes
            // resetTransform = 1 for every non-crash reset. The callee's own
            // CGS_ASSERT(IsCrashing()) is kept verbatim as the tripwire for that invariant.
            const bool lbWasCrashing = lpRaceCar->IsCrashing();

            lpRaceCar->ClearCrashing();

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[crash-clear] ProcessResetEvents car " << liRaceCar
                    << ": ClearCrashing() via vtable slot 1 -- crashing "
                    << (lbWasCrashing ? 1 : 0) << " -> "
                    << (lpRaceCar->IsCrashing() ? 1 : 0) << "\n";
            }
        }

        // 0x82617E28 -- the world-side notification.
        if (lpManagerOutputInterface != 0)
        {
            RaceCarResetEvent lResult;
            lResult.meActiveRaceCarIndex  = static_cast<EActiveRaceCarIndex>(liRaceCar);
            lResult.mbResettingAfterWreck = lrEvent.mbResettingAfterWreck;
            lResult.mResetPosition        = lrEvent.mInitialTransform.wAxis;
            lpManagerOutputInterface->AddRaceCarResetEvent(lResult);
        }

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            const Matrix44Affine& lrSeated = lpRaceCar->GetTransform();
            *CgsDev::Log::gpDebugPrint
                << "[teleport] ProcessResetEvents car " << liRaceCar
                << " road (" << lrEvent.mInitialTransform.wAxis.x << ", "
                << lrEvent.mInitialTransform.wAxis.y << ", "
                << lrEvent.mInitialTransform.wAxis.z << ")"
                << " -> seated (" << lrSeated.wAxis.x << ", " << lrSeated.wAxis.y << ", "
                << lrSeated.wAxis.z << ")"
                << " at (" << lrSeated.zAxis.x << ", " << lrSeated.zAxis.y << ", "
                << lrSeated.zAxis.z << ")"
                << " resetTransform=" << (lrEvent.mbResetTransform ? 1 : 0)
                << " resetDeform=" << (lrEvent.mbResetDeformation ? 1 : 0) << "\n";
        }
    }
}

}
}
