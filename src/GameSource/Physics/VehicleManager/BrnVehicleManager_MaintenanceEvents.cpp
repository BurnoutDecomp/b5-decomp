// =================================================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_MaintenanceEvents.cpp
//
// VehicleManager::ProcessVehicleMaintenanceEvents @0x8264AB38 (118 insns) and FOUR of the five arms
// it dispatches, as REAL BODIES, plus the two create-path helpers those arms share with the create
// drain. The traffic twin stays a named one-shot gate (it is another wave's function).
//
//   ProcessVehicleMaintenanceEvents        @0x8264AB38  (118)  ⭐ bodied (earlier wave)
//   RecordNetworkRaceCarsAddedForCollision @0x825C7EA8  (321)  ⭐ BODIED THIS WAVE
//   ProcessRemoveEvents                    @0x826160C8  (426)  ⭐ BODIED THIS WAVE
//   ProcessValidationEvents                @0x825E9010  ( 65)  ⭐ BODIED THIS WAVE
//   ProcessCollisionEvents                 @0x825E8F28  ( 57)  ⭐ BODIED THIS WAVE  <- the "export hole"
//   AddRaceCarDeformationModel             @0x825E9118  (153)  ⭐ BODIED THIS WAVE
//   SetAllNetworkRaceCarsHidden            @0x825E9380  (175)  ⭐ BODIED THIS WAVE
//   PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)  ⛔ still a GATE
//   ProcessCreateEvents                    @0x82616770  (1067) -> its OWN slice TU (see below)
//
// Slice TU (home BrnVehicleManager.cpp is still unmounted) -- the same shape as the sibling
// BrnVehicleManager_Prepare.cpp / _ReadUpdatedBodies.cpp / _TractionLineTests.cpp slices.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐⭐ THE "EXPORT HOLE" AT @0x825E8F28 IS RETIRED, AND IT WAS NEVER A HOLE IN THE DATABASE.
//
// Every previous banner in this file recorded ProcessCollisionEvents as "a HOLE in the IDA export
// set -- it has no per-function JSON and is known only by the name IDA prints at this one call
// site ... the insn count is genuinely unknown and is left unstated rather than guessed."
//
// Retired with the targeted headless-idat technique of commit b53e2523 (one `idat.exe -A -S<script>`
// run over a private copy of BURNOUT_X360_ARTIST.XEX.i64). The result:
//     had_function = TRUE.  start 0x825E8F28, end 0x825E900C, 57 instructions,
//     name = BrnPhysics::Vehicle::VehicleManager::ProcessCollisionEvents,
//     Hex-Rays decompiles it clean, xrefs_to = the one expected caller.
// It is a fully analysed, correctly NAMED function that the export RUN simply never wrote a file
// for -- `tools/ida/export_all.py` walks `idautils.Functions()` and writes one JSON per function,
// and this one's write did not happen. ⇒ **"absent from .ida-exports" is a fact about the export,
// not about the database.** Two of this project's standing parks were on that word; anything else
// still parked "export hole" is worth re-checking the same way before it is planned around.
//
// -------------------------------------------------------------------------------------------------
// ⛔⛔ WHY ProcessCreateEvents IS NOT IN THIS FILE ANY MORE.
//
// It is bodied -- completely, all 1,067 instructions -- in the sibling slice
// BrnVehicleManager_ProcessCreateEvents.cpp, which compiles GREEN. It was split out so that the
// MOUNTING decision was separable: the four arms below are inert bookkeeping over queues that are
// empty today, while setting one bit of mUsedRaceCars switches on four already-called per-frame
// loops against a freshly constructed car. Those two things should not have to be mounted in the
// same commit, and a single TU would have forced that.
//
// ⇒ BOTH FILES ARE MOUNTED AS OF THE BOOT-VERIFIED WAVE. This banner used to say "THIS FILE IS
//   MOUNTED. The create TU is NOT (yet)"; that is retired -- tools/build/build_game_exe.bat now
//   echoes BrnVehicleManager_ProcessCreateEvents.cpp too, with the conductor's rationale beside
//   the line. The split stands on its own merits (separable slices, separable blame); it is no
//   longer a mount gate.
//
// -------------------------------------------------------------------------------------------------
// ⭐ CONSOLE-VALUE HAZARD DECLARED ONCE, HERE, BECAUSE THREE BODIES BELOW HIT IT.
//
// The vehicle manager's race-car identity is a 64-bit CgsPhysics::RigidBodyId whose HIGH dword is
// the 32-bit entity word (`ld ; srdi r,r,32` at every read site). The Deformation event structs
// this file posts into (BrnDeformationEvents.h) type their `mHandlingBodyID` with the GLOBAL
// 4-byte `::RigidBodyId { u32 muValue }` stand-in, NOT CgsPhysics::RigidBodyId -- a fork
// BrnDeformationManager.h documents at length and deliberately leaves open.
//
// On the console those two are the same bytes: a 32-bit big-endian load of the first word of the
// 64-bit handle yields the entity word. On x64 they are NOT. So every post below assigns the
// ENTITY WORD into `.muValue`, spelled through CgsPhysics::RigidBodyId::GetEntityId(), never a
// truncating cast of the whole handle. That is what the committed consumers already expect
// (BrnDeformationManager.cpp does `mHandlingBodyID.muValue >> 24` for the owner and
// `(mHandlingBodyID.muValue >> 10) & 0x3FFF` for the index -- entity-word arithmetic).
// ⚠ It is also lossless on this build: ProcessCreateEvents is the only writer of
// maRaceCarHandlingBodyIDs and it stores `((u64)entityWord) << 32`, i.e. the low dword is always
// zero. If that ever stops being true, these three posts lose information silently.
//
// -------------------------------------------------------------------------------------------------
// ⭐⭐ THE HAZARD ABOVE WAS INDEPENDENTLY MEASURED BY THE SIBLING WAVE (2026-08-11), which recovered
// these same two functions, saw the same fork, and PARKED rather than shipped. Its evidence is
// folded in here because it CONFIRMS the reading above from the other direction -- and because its
// conclusion (which this file does not share) is the follow-up flag:
//
//   (a) Every access to maRaceCarHandlingBodyIDs is EIGHT bytes wide, in both directions --
//         ProcessRemoveEvents         0x82616444  ldx r11, r9, r25  ; 0x82616448 std r11, var_D0
//                                     0x826164B0  ldx r11, r11, r25 ; std r11, var_E8
//         AddRaceCarDeformationModel  0x825E9324  ldx r8,  r23, r29 ; std r8,  var_C8
//                                     0x825E9358  ld  r5,  0(r22)   -- AddDeformationModel's arg
//                                     0x825E932C  ldx r4,  r20, r29 -- its ResourceHandle, ALSO 8
//       and the 32-bit forms (lwzx/stw/lwz) appear all over both bodies for the genuinely 32-bit
//       fields, so this is a CONTRAST WITHIN ONE BODY, not one isolated instruction. ⇒ the
//       "handling-body handle is 64-bit" question BrnDeformationManager.h left open is DECIDED.
//
//   (b) It also settles that header's other open contradiction -- why the committed
//       BrnDeformationManager.cpp bodies do `muValue >> 24` / `(muValue >> 10) & 0x3FFF` when
//       CgsRigidBody.h documents the packing as (high dword = EntityId, low dword = index).
//       AddRaceCarDeformationModel does the SAME two extractions, but on the HIGH DWORD FIRST:
//         0x825E9218 srdi r11,r11,32 ; 0x825E9220 clrlwi r28,r11,0
//         0x825E9224 srwi r11,r28,24        <- the owner byte
//         0x825E9240 extrwi r11,r28,14,8    <- the 14-bit index
//       i.e. the ARITHMETIC in those consumers is right and only the WIDTH is short. Which is
//       exactly the "assign the ENTITY WORD, never a truncating cast" rule stated above.
//
//   ⚠️ THE SIBLING WAVE DREW THE OPPOSITE CONCLUSION -- that these two bodies must not land at all,
//   because passing the u64 through the 4-byte stand-in "delivers ZERO with nothing asserting".
//   That failure mode is REAL for a truncating cast (`static_cast<u32>(theWholeHandle)` takes the
//   low dword, which for a race car is identically zero). It is NOT what these bodies do: every
//   post goes through GetEntityId(), which takes the HIGH dword -- the half that carries the value.
//   The bodies stay. What the sibling wave is right about is the DEBT: while `::RigidBodyId` is
//   4 bytes, this file is carrying a fork by hand at every seat.
//
//   ⛔ FOLLOW-UP FLAG, and it is NOT this file's change to make: `::RigidBodyId` in
//   BrnCommonTypes.h:28 should be widened to match CgsPhysics::RigidBodyId, per the fork
//   BrnDeformationManager.h documents. That moves five `mHandlingBodyID` fields, the
//   BrnDeformationManager.cpp bodies that shift them, and those event records' field offsets --
//   all in a MOUNTED subsystem. Its own wave, with its own boot test. When it lands, every
//   `GetEntityId()` seat in this file should be revisited to pass the whole handle instead.
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"  // the five drained queues
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputRequestInterface (InRemoveRigidBody)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h" // the five posted queues
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"  // BrnWorld::ERaceCarType
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                        // gpDebugPrint / gxMessageFilterFlags

namespace
{
    inline void MaintenanceGateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

#define BRN_MAINTENANCE_GATE(TAG)                                                          \
    do { static bool s_bLogged = false;                                                    \
         MaintenanceGateLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{
namespace Vehicle
{
    // ---------------------------------------------------------------------------------------------
    // ProcessVehicleMaintenanceEvents  @0x8264AB38  (118 insns; DWARF/asserts BrnVehicleManager.cpp
    // :1141..:1147)
    //
    // Read straight off the pseudocode AND the asm: seven null asserts in parameter order, then six
    // unconditional calls, no branch and no local state between them. The argument routing below is
    // the console's exactly -- note that the arms do NOT all take the same list:
    //     RecordNetworkRaceCarsAddedForCollision(this, in)
    //     ProcessRemoveEvents (this, in, out, mgrOut,        deform)
    //     ProcessCreateEvents (this, in, out, mgrOut,        deform)
    //     ProcessValidationEvents(this, in,                  deform)
    //     ProcessCollisionEvents (this, in,                  deform)
    //     PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(this+44768, <all seven>)
    // The traffic call's r3 is `this + 44768`, which is &mPhysicalTrafficManager -- reached BY NAME
    // here, not by that offset.
    //
    // ⚠ The console returns the traffic call's result (r3 falls through). Nothing reads it: the one
    // caller, PhysicsModule::PostSceneUpdate, ignores the return. Declared void here, matching the
    // shape the caller actually uses; the discarded value is noted rather than fabricated into a
    // return type nobody consumes.
    // ---------------------------------------------------------------------------------------------
    void VehicleManager::ProcessVehicleMaintenanceEvents(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const VehicleInputInterface* lpInputInterface,
        VehicleOutputRequestInterface* lpOutputInterface,
        VehicleManagerOutputInterface* lpManagerOutputInterface,
        VehicleOutputInterface* lpVehicleOutputInterface,
        Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        CGS_ASSERT(lpInputBufferStack        != 0, "lpInputBufferStack != NULL");        // :1141
        CGS_ASSERT(lpOutputBufferStack       != 0, "lpOutputBufferStack != NULL");       // :1142
        CGS_ASSERT(lpInputInterface          != 0, "lpInputInterface != NULL");          // :1143
        CGS_ASSERT(lpOutputInterface         != 0, "lpOutputInterface != NULL");         // :1144
        CGS_ASSERT(lpManagerOutputInterface  != 0, "lpManagerOutputInterface != NULL");  // :1145
        CGS_ASSERT(lpVehicleOutputInterface  != 0, "lpVehicleOutputInterface != NULL");  // :1146
        CGS_ASSERT(lpDeformationInterface    != 0, "lpDeformationInterface != NULL");    // :1147

        RecordNetworkRaceCarsAddedForCollision(lpInputInterface);

        ProcessRemoveEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessCreateEvents(lpInputInterface, lpOutputInterface,
                            lpManagerOutputInterface, lpDeformationInterface);

        ProcessValidationEvents(lpInputInterface, lpDeformationInterface);

        ProcessCollisionEvents(lpInputInterface, lpDeformationInterface);

        mPhysicalTrafficManager.ProcessTrafficMaintenanceEvents(
            lpInputBufferStack, lpOutputBufferStack, lpInputInterface, lpOutputInterface,
            lpManagerOutputInterface, lpVehicleOutputInterface, lpDeformationInterface);
    }

    // =============================================================================================
    // ARM 1 -- RecordNetworkRaceCarsAddedForCollision @0x825C7EA8 (321 insns)
    //          asserts BrnVehicleManager.cpp:9677 / :9699, CgsBitArray.h:203 / :222
    //
    // Two per-frame bitsets, one queue drain. Read off the asm register for register:
    //
    //   0x825C7EF0  addis r11,r30,1 ; addi r11,r11,-0x5150 ; std r20(0),0(r11)
    //               -> this + 44720 == mNetworkCarsAddedForCollisionThisFrame, cleared (one std,
    //                  i.e. one 8-byte BitArray<8> word).
    //   0x825C7F00  lis/ori r10 = 0x22B50 (142160) ; ldx r11,r31,r10
    //   0x825C7F10  lis/ori r9  = 0xAEA8  (44712)  ; stdx r11,r30,r9
    //               -> mRaceCarsAddedForCollision = *lpInputInterface->GetRaceCarsAddedForCollision()
    //                  (142160 is that member's console seat -- the very literal
    //                  BrnVehicleInputInterface.h's accessor banner already quotes).
    //   0x825C7F08  addis r3,r31,2 ; addi r3,r3,0x358 -> input + 131928 ==
    //               mNetworkCarsAddedRemovedForCollisionQueue; length re-read at +8 every iteration.
    //
    // ⚠️ THE LOOP BOUND IS RE-READ, NOT CACHED (`lwz r10,8(r10)` at 0x825C8394, inside the loop),
    // so it is written as a live `GetLength()` test rather than hoisted.
    // ⚠️ The two CgsBitArray.h asserts (:203 IsBitSet bounds, :222 SetBit bounds) are the
    // container's own tripwires and fire from inside the named calls; the console emits them
    // inline because it inlines the container. Not duplicated here.
    // ⚠️ The two "invalid index : %u < %u" / "Index: %u, Number of bits: %u" streams are console
    // gpcMessageBuffer formatting inside those same container asserts -- see the container header.
    // =============================================================================================
    void VehicleManager::RecordNetworkRaceCarsAddedForCollision(
        const VehicleInputInterface* lpInputInterface)
    {
        CGS_ASSERT(lpInputInterface != 0, "lpInputInterface != NULL");   // :9677

        mNetworkCarsAddedForCollisionThisFrame.UnSetAll();
        mRaceCarsAddedForCollision = *lpInputInterface->GetRaceCarsAddedForCollision();

        const VehicleInputInterface::NetworkCarsAddRemoveForCollisionQueue* lpQueue =
            lpInputInterface->GetNetworkCarsAddRemoveForCollisionQueue();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const VehicleAddedForCollisionEvent& lrEvent = lpQueue->GetEvent(liEvent);

            // `ld r11,0(evt) ; srdi r11,r11,32 ; extrwi r29,r11,14,8` -- the 14-bit entity index
            // out of the embedded entity word, i.e. the active-race-car slot.
            const u32 luRaceCar = lrEvent.mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();

            CGS_ASSERT(mUsedRaceCars.IsBitSet(luRaceCar),
                       "mUsedRaceCars.IsBitSet( liRaceCar )");           // :9699

            // `lbz r11,8(evt)` -- VehicleAddedForCollisionEvent::mbAdded.
            if (lrEvent.mbAdded)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Network race car "
                        << static_cast<s32>(luRaceCar) << ", type "
                        << static_cast<s32>(maeRaceCarTypes[luRaceCar])
                        << " was added for collision\n";
                }
                mNetworkCarsAddedForCollisionThisFrame.SetBit(luRaceCar);
            }
        }
    }

    // =============================================================================================
    // ARM 2 -- ProcessRemoveEvents @0x826160C8 (426 insns)
    //          asserts BrnVehicleManager.cpp:1202 / :1203 / :1639 / :1640, CgsBitArray.h:203 / :241
    //
    // ⭐⭐ THIS IS PART OF THE CREATE WAVE, NOT A LATER ONE. A read-only probe at the drain point
    // (2026-08-11) reported three create events on one boot naming race-car slots **0, then 1, then
    // 2** -- three different slots, not one car three times. ProcessCreateEvents' own assert list
    // contains "Race Car Index Already Used", i.e. the console expects the slot to be FREE when it
    // is claimed, and this function is the only thing that frees one.
    //
    // The body, in the console's order (`bl sub_825BB8A0` is BaseEventQueue<RemoveRaceCarEvent>::
    // GetEvent; queue seat `addis r3,r4,2 ; addi r3,r3,-0x6D0` == input + 129328):
    //   1. owner + slot out of the event's VolumeInstanceId (`srwi r10,r27,24`, `extrwi r30,r27,14,8`)
    //   2. two asserts, then the driver-record clear
    //   3. two deformation posts (deactivate, remove) keyed on maRaceCarHandlingBodyIDs[slot]
    //   4. maRaceCarEntityIDs[slot] = the invalid sentinel (dword_82F2A3A4 == 0xFFFFFFFF, read out
    //      of the image), and an InRemoveRigidBody post whose id re-owners the entity word to 0x0B
    //   5. three per-car seeds inside maRaceCarVehicles[slot]
    //   6. mUsedRaceCars.UnSetBit(slot), the log, the network-car un-hide, and the type reset
    //
    // ⚠️ STEP 2 IS AN INLINED VehicleDriver::ClearControls(). The stores are, relative to
    // &maRaceCarDrivers[slot] (`mulli r11,r30,0xE0 ; add r11,r11,r24`, then +0x40..+0x8E):
    //     +0x00 = -1 (word)     +0x04..+0x30 = 0.0f (twelve floats)     +0x34 = 1.0f
    //     +0x38 = -1 (byte)     +0x39,+0x3B..+0x42 = 0                  +0x48 = 0.0f
    //     +0x4C,+0x4D,+0x4E = 0
    // -- which is BrnAIDriverControls in full, EXCEPT +0x3A (mbToggle) and +0x44 (meDriverType).
    // That is the identical store set (identical omissions included) as the out-of-line
    // BrnNetworkDriverControls::Clear @0x82581200 this tree already documents, so it is the same
    // "clear the controls, keep the driver type" routine inlined here. `VehicleDriver::ClearControls`
    // IS declared (BrnVehicleDriver.h:66) and, as of 2026-08-11, HAS a body -- BrnVehicleDriver.cpp,
    // recovered from these same two inline sites by the sibling wave. The block is therefore no
    // longer written out here: this drain CALLS it, at the console's own call position.
    // ⚠️ 0.0f is flt_82001CC0 and 1.0f is flt_82001C98, both read out of the image, not assumed.
    // =============================================================================================
    void VehicleManager::ProcessRemoveEvents(const VehicleInputInterface* lpInputInterface,
                                             VehicleOutputRequestInterface* lpOutputInterface,
                                             VehicleManagerOutputInterface*,
                                             Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::RemoveRaceCarEventQueue* lpQueue =
            lpInputInterface->GetRemoveRaceCarEvents();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const RemoveRaceCarEvent& lrEvent = lpQueue->GetEvent(liEvent);

            const u32 luEntityWord = static_cast<u32>(lrEvent.mVolumeInstanceID.muId >> 32);
            const u32 luOwner      = luEntityWord >> 24;
            const u32 luRaceCar    = lrEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            CGS_ASSERT(luOwner == 1u,
                "lRaceCarEvent.mVolumeInstanceID.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :1202
            CGS_ASSERT(mUsedRaceCars.IsBitSet(luRaceCar),
                       "Trying to remove an unused race car");                                          // :1203

            // ---- the inlined VehicleDriver::ClearControls (see the banner) --------------------
            // ⭐ DELETE-WHEN HONOURED 2026-08-11 (merge of the two create-drain waves). The 28-store
            // block that stood open here is now VehicleDriver::ClearControls, an out-of-line body
            // in BrnVehicleDriver.cpp recovered from BOTH console inline sites at once. Its store
            // set was checked field-for-field against this copy before the swap: identical, same
            // two omissions (mbToggle, meDriverType), same lone 1.0f. The call goes exactly where
            // the console's inlined run sat.
            maRaceCarDrivers[luRaceCar].ClearControls();

            // ---- the two deformation posts ---------------------------------------------------
            // Both take maRaceCarHandlingBodyIDs[slot]; see this file's console-value banner for
            // why the ENTITY WORD is what lands in the 4-byte event field.
            const CgsPhysics::RigidBodyId lHandlingBodyId(maRaceCarHandlingBodyIDs[luRaceCar]);

            {
                // `ldx r11,r9,r25 ; std r11,var_D0 ; bl DeactivateDeformationModelEvent::AddEvent`
                //
                // ⚠️⚠️ CORRECTED 2026-08-11 AT THE MERGE, and this one was a real bug in the
                // committed body. It used to write ONLY mHandlingBodyID, with a note claiming "the
                // console leaves mfInitialDamageAmount and meDeformationResetType at whatever the
                // stack held". THAT IS WRONG, and the sibling wave caught it: the other two fields
                // are LOOP-INVARIANT CONSTANTS, so the compiler hoisted their stores OUT of the
                // drain loop, into the prologue -- which is why a read that only scanned the loop
                // body could not see them. Re-read from the ARTIST asm, addresses and values:
                //     0x82616118  lfs  f31, flt_82001CC0   <- 0.0f, read out of the image
                //     0x8261611C  li   r11, -1
                //     0x82616128  stfs f31, var_C8(r1)     <- event +8  mfInitialDamageAmount = 0.0f
                //     0x82616138  stw  r11, var_C4(r1)     <- event +12 meDeformationResetType = -1
                //     ...loop...
                //     0x82616444  ldx  r11, r9, r25        <- maRaceCarHandlingBodyIDs[slot]
                //     0x82616448  std  r11, var_D0(r1)     <- event +0  mHandlingBodyID
                // var_D0 is the 16-byte event base and var_C8/var_C4 are its other two fields, so
                // the console posts a FULLY initialised event every iteration. All three are
                // written here now. (-1 is not a named DeformationResetType enumerator; it is the
                // literal the image stores, so it is spelled as a cast of that literal and not
                // rounded to the nearest named value.)
                Deformation::DeactivateDeformationModelEvent lDeactivate;
                lDeactivate.mHandlingBodyID.muValue =
                    static_cast<u32>(lHandlingBodyId.GetEntityId());
                lDeactivate.mfInitialDamageAmount  = 0.0f;                                  // flt_82001CC0
                lDeactivate.meDeformationResetType =
                    static_cast<Deformation::DeformationResetType>(-1);                     // li r11, -1
                lpDeformationInterface->GetDeactivateDeformationModelQueue().AddEvent(lDeactivate);
            }

            // The console's inlined VehicleManager::RemoveRaceCarDeformationModel -- its two
            // asserts are the give-away, and they sit AFTER the deactivate post, exactly here.
            CGS_ASSERT(lpDeformationInterface != 0, "NULL != lpDeformationInterface");             // :1639
            CGS_ASSERT(static_cast<s32>(luRaceCar) >= 0 && static_cast<s32>(luRaceCar) < 8,
                       "(liVehicleIndex >= 0) && (liVehicleIndex < ku8MaxNumRaceCars)");           // :1640
            {
                Deformation::RemoveDeformationModelEvent lRemove;
                lRemove.mHandlingBodyID.muValue = static_cast<u32>(lHandlingBodyId.GetEntityId());
                lpDeformationInterface->GetRemoveDeformationModelQueue().AddEvent(lRemove);
            }

            // ---- forget the entity, and ask the simulation to drop the body ------------------
            // `lwz r11, dword_82F2A3A4 ; stwx r11,r9,r25` -- the sentinel is 0xFFFFFFFF, read out
            // of the image (not inferred from the name).
            maRaceCarEntityIDs[luRaceCar].muValue = 0xFFFFFFFFu;

            {
                // `clrlwi r10,r27,8 ; oris r10,r10,0xB00 ; sldi r11,r10,32`
                // -- the entity word with its OWNER byte replaced by 0x0B
                // (BrnWorld::E_ENTITYTYPE_PROP_COLLISION_RACECAR), promoted into the handle's high
                // dword. That is the id the simulation knows this body by.
                CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody lRemoveBody;
                lRemoveBody.mID = static_cast<u64>((luEntityWord & 0x00FFFFFFu) | 0x0B000000u) << 32;
                lRemoveBody.mbFailIfRigidBodyNotFound = false;   // `stb r28(0), var_B8`
                lpOutputInterface->GetRemoveRigidBodyQueue()->AddEvent(lRemoveBody);
            }

            // ---- the three per-car seeds inside the race-car record --------------------------
            // `mulli r11,r30,0x1460 ; add r11,r11,r25 ; addi r11,r11,0x740` == &maRaceCarVehicles[slot],
            // then `std 0,0x1158`, `std 0,0x1220`, `stb 0,0x1359` -- i.e. drop every queued air-ram
            // and spin slot and clear the deformation-model-is-active latch. All three reached BY
            // NAME (VehiclePhysics.h pins +0x1158 / +0x1220 / +0x1359 to these members).
            maRaceCarVehicles[luRaceCar].mUsedAirRams.UnSetAll();
            maRaceCarVehicles[luRaceCar].mUsedSpins.UnSetAll();
            maRaceCarVehicles[luRaceCar].mbDeformationModelIsActive = false;

            mUsedRaceCars.UnSetBit(luRaceCar);   // CgsBitArray.h:241 bounds tripwire

            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Removing race car "
                    << static_cast<s32>(luRaceCar) << ", type "
                    << static_cast<s32>(maeRaceCarTypes[luRaceCar]) << "\n";
            }

            if (maeRaceCarTypes[luRaceCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint << "HIDE_ONLINE: " << "Making network car "
                        << static_cast<s32>(luRaceCar)
                        << " not hidden because it was removed\n";
                }
                mHiddenRaceCars.UnSetBit(luRaceCar);   // CgsBitArray.h:241 bounds tripwire
            }

            maeRaceCarTypes[luRaceCar] = BrnWorld::E_RACE_CAR_TYPE_INACTIVE;   // `li r10,3 ; stwx`
        }
    }

    // =============================================================================================
    // ARM 4 -- ProcessValidationEvents @0x825E9010 (65 insns)
    //
    // The smallest of the five and completely unambiguous. Drain
    // mValidateRaceCarEventQueue (input + 131472) and, per event, either LATCH the two resource
    // handles into the manager's own tables or CLEAR them, then forward a ValidateRaceCarEvent onto
    // the deformation interface's own validate queue (deform + 3856).
    //
    //   `ld r10,8(evt) ; srdi ; extrwi r10,r10,14,8`   -- the slot, out of mVolumeInstanceID
    //   `lbz r9,0(evt)`                                -- mbValidate selects the arm
    //   validate arm  (0x825E9078..0x825E90C4): maRaceCarModelHandles[slot] = evt.mModelHandle,
    //       maRaceCarGraphicsModelHandles[slot] = evt.mGraphicsHandle, then the forwarded event is
    //       {mbValidate = 1, mVolumeInstanceID, mModelHandle}.
    //   invalidate arm(0x825E90C8..0x825E90F4): both table entries cleared to {0,0}, and the
    //       forwarded event is {mbValidate = 0, mVolumeInstanceID}.
    //
    // ⚠️ NEITHER ARM WRITES THE FORWARDED EVENT'S mGraphicsHandle, and the invalidate arm does not
    // write its mModelHandle either. Reproduced exactly -- the console posts a partly-uninitialised
    // stack event, and filling those fields in would be inventing input for the deformation drain.
    // ⚠️ 8 * (slot + 0x154C) and 8 * (slot + 0x1554) are the two CONSOLE strides into the tables;
    // they are reached by NAME here because the host stride is 16, not 8 (see BrnVehicleManager.h).
    // =============================================================================================
    void VehicleManager::ProcessValidationEvents(const VehicleInputInterface* lpInputInterface,
                                                 Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::ValidateRaceCarEventQueue* lpQueue =
            lpInputInterface->GetValidateRaceCarEvents();

        for (s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent)
        {
            const ValidateRaceCarEvent& lrEvent = lpQueue->GetEvent(liEvent);
            const u32 luRaceCar = lrEvent.mVolumeInstanceID.GetEntityIDEntityIndex();

            ValidateRaceCarEvent lForwarded;
            if (lrEvent.mbValidate)
            {
                maRaceCarModelHandles[luRaceCar]         = lrEvent.mModelHandle;
                maRaceCarGraphicsModelHandles[luRaceCar] = lrEvent.mGraphicsHandle;

                lForwarded.mbValidate        = true;
                lForwarded.mVolumeInstanceID = lrEvent.mVolumeInstanceID;
                lForwarded.mModelHandle      = lrEvent.mModelHandle;
                // mGraphicsHandle deliberately NOT written -- see the banner.
            }
            else
            {
                maRaceCarModelHandles[luRaceCar].Clear();
                maRaceCarGraphicsModelHandles[luRaceCar].Clear();

                lForwarded.mbValidate        = false;
                lForwarded.mVolumeInstanceID = lrEvent.mVolumeInstanceID;
                // mModelHandle / mGraphicsHandle deliberately NOT written -- see the banner.
            }

            lpDeformationInterface->GetValidateDeformationModelEvents().AddEvent(lForwarded);
        }
    }

    // =============================================================================================
    // ARM 5 -- ProcessCollisionEvents @0x825E8F28 (57 insns)   ⭐ the retired "export hole"
    //
    // Two independent drains, both pure forwarders into the deformation interface:
    //   loop 1  input + 131744 (mSetRaceCarCollisionEventQueue)   -> deform + 4128
    //   loop 2  input + 131836 (mSetRaceCarCullingGroupEventQueue)-> deform + 4592
    //
    // Each event's 32-bit EntityId is promoted into the HIGH dword of a 64-bit rigid-body handle --
    // `lwz r11,<scratch> ; extldi r11,r11,64,32` is a rotate-left-32 of the entity word, i.e.
    // `(u64)entityWord << 32`. The payload byte / word is copied across unchanged
    // (`lbz var_50+4 ; stb var_38` and `lwz var_50+4 ; stw var_38`).
    //
    // ⚠️ Both loops re-read the queue length every iteration (`lwz r11,8(r30)` at 0x825E8F90 /
    // 0x825E8FF4); written as live GetLength() tests, not hoisted.
    // ⚠️ `this` IS UNUSED in the console body -- r3 is never read after the prologue. Kept a
    // non-static member because that is what the console's `bl` with r3 = this is.
    // =============================================================================================
    void VehicleManager::ProcessCollisionEvents(const VehicleInputInterface* lpInputInterface,
                                                Deformation::DeformationInputInterface* lpDeformationInterface)
    {
        const VehicleInputInterface::SetRaceCarCollisionEventQueue* lpCollisionQueue =
            lpInputInterface->GetSetRaceCarCollisionEvents();

        for (s32 liEvent = 0; liEvent < lpCollisionQueue->GetLength(); ++liEvent)
        {
            const SetRaceCarCollisionEvent& lrEvent = lpCollisionQueue->GetEvent(liEvent);

            Deformation::SetModelCollisionEvent lPost;
            lPost.mHandlingBodyID.muValue = lrEvent.mBodyId.muValue;   // the entity word (see banner)
            lPost.mbCollide               = lrEvent.mbCollide;
            lpDeformationInterface->GetSetModelCollisionEvents().AddEvent(lPost);
        }

        const VehicleInputInterface::SetRaceCarCullingGroupEventQueue* lpCullingQueue =
            lpInputInterface->GetSetRaceCarCullingGroupEvents();

        for (s32 liEvent = 0; liEvent < lpCullingQueue->GetLength(); ++liEvent)
        {
            const SetRaceCarCullingGroupEvent& lrEvent = lpCullingQueue->GetEvent(liEvent);

            Deformation::SetModelCullingGroupEvent lPost;
            lPost.mHandlingBodyID.muValue = lrEvent.mBodyId.muValue;   // the entity word (see banner)
            lPost.mCullGroup              = lrEvent.mCullingGroup;
            lpDeformationInterface->GetSetModelCullingGroupEvents().AddEvent(lPost);
        }
    }

    // =============================================================================================
    // AddRaceCarDeformationModel @0x825E9118 (153 insns)
    //   asserts BrnVehicleManager.cpp:1606 / :1607 / :1611, BrnVehicleManager.h:1985
    //
    // ⭐⭐ THIS FUNCTION WAS RECORDED AS "ALREADY BODIED AND MOUNTED" BY AN EARLIER BANNER IN THIS
    // FILE. It was ABSENT: before this wave the name appeared in exactly two places in the whole
    // tree, both of them comment lines in that banner. Its `xrefs_to` is a one-element set
    // (ProcessCreateEvents), which is why nothing ever failed to link.
    //
    // The body is a marshaller into DeformationInputInterface::AddDeformationModel. Eleven
    // arguments, every one of them read off the call-site asm at 0x825E9320..0x8261936C:
    //     r4 = maRaceCarModelHandles[index]      (`ldx r4,r20,r29`  -- the whole 8-byte handle)
    //     r5 = maRaceCarHandlingBodyIDs[index]   (`ldx r8,r23,r29 ; std ; ld r5`)
    //     r6 = GetEntityId(that handle)          (the inlined helper -- see BrnVehicleManager.h)
    //     r7 = &maRaceCarVehicles[index]         (`mulli r9,r31,0x1460 ; add r24 ; addi r7,r24,0x740`)
    //     v1 = mpAttribs->mBaseAttribs.mCOMOffset(`lwz r21,0xE60(r24)` == the record's +0x720
    //                                             pointer, then `lvx128 v1,r21,0x20`)
    //     r8 = the transform, passed straight through
    //     v2 = ZERO   (`stfs 0.0f x3 ; lvx128 v2,r0,&var_A0`)
    //     v3 = ZERO   (`stfs 0.0f x4 ; lvx128 v3,r0,&var_B0`)
    //     f1 = the damage amount, passed straight through
    //     r10 = the reset type, passed straight through
    //     +stack byte = 1  (`li r31,1 ; stb r31,var_D9`) -- lbUseSweptSphereTests
    //
    // ⚠️ PPC FLOAT-ARG ABI, AND IT IS LOAD-BEARING ON BOTH SIDES. This function's own prologue
    // consumes r4,r5,r6 then f1 then r8 -- **r7 is skipped**, because the float argument takes the
    // slot. Reading the prologue as r4,r5,r6,r7,r8 would have shifted the reset type onto the
    // transform and the transform onto the index.
    // ⚠️ The three zeroed stack vectors are 0.0f from flt_82001CC0, read out of the image.
    // =============================================================================================
    void VehicleManager::AddRaceCarDeformationModel(
        Deformation::DeformationInputInterface* lpDeformationInterface,
        s32 liVehicleIndex,
        Matrix44Affine lInitialWorldSpaceTransform,
        f32 lfInitialDamageAmount,
        Deformation::DeformationResetType leBaseDeformationType)
    {
        CGS_ASSERT(lpDeformationInterface != 0, "NULL != lpDeformationInterface");           // :1606
        CGS_ASSERT(liVehicleIndex >= 0 && liVehicleIndex < 8,
                   "(liVehicleIndex >= 0) && (liVehicleIndex < ku8MaxNumRaceCars)");         // :1607
        CGS_ASSERT(maRaceCarModelHandles[liVehicleIndex].mpResourceMemory != 0,
            "maRaceCarModelHandles[liVehicleIndex].GetResource()->GetMemoryResource() != NULL"); // :1611

        const CgsPhysics::RigidBodyId lHandlingBodyId(maRaceCarHandlingBodyIDs[liVehicleIndex]);
        const EntityId lGlobalEntityId = GetEntityId(lHandlingBodyId);   // BrnVehicleManager.h:1985 lives inside

        Vector3 lvZero;
        lvZero.x = 0.0f; lvZero.y = 0.0f; lvZero.z = 0.0f; lvZero.w = 0.0f;

        // ⛔⛔ THE MODEL-HANDLE ARGUMENT IS A LOUD PARK, AND IT IS A TYPE FORK, NOT A GAP IN THE
        // DECODE. The console passes the WHOLE 8-byte resource handle here
        // (`ldx r4, r20, r29` -- one doubleword straight out of maRaceCarModelHandles[index]), and
        // on the host that value is a 16-byte `CgsResource::ResourceHandle {void*, Entry*}`.
        // `DeformationInputInterface::AddDeformationModel`'s first parameter is spelled
        // `BrnPhysics::Deformation::ResourceHandle`, which BrnDeformationEvents.h:22 defines as a
        // FOUR-BYTE stand-in `{ u32 muValue }`. There is no honest projection of a pair of 64-bit
        // pointers onto one u32, and reinterpret_cast'ing across would hand the deformation add
        // the low half of a pointer -- exactly the console-value-on-the-host defect this wave
        // exists to avoid.
        //
        // ⚠️ It is INERT TODAY on both ends: this function's only caller is ProcessCreateEvents
        // (its own TU, unmounted), and the only consumer of the posted event's mModelHandle is
        // `DeformationManager::ResolveDeformationSpec`, which BrnDeformationManager.cpp:90 marks
        // DECLARE-ONLY. So nothing reads it -- but a zero handle is still a wrong value, so it is
        // announced once rather than passed silently.
        //
        // DELETE-WHEN the Deformation group de-forks its `ResourceHandle`. That is their change,
        // not this wave's: retyping it onto CgsResource::ResourceHandle grows
        // AddDeformationModelEvent past the X360-attested `sizeof == 160` its own mounted
        // BaseEventQueue TU static_asserts, so it needs the queue stride re-derived with it.
        {
            static bool s_bModelHandleForkLogged = false;
            if (!s_bModelHandleForkLogged)
            {
                s_bModelHandleForkLogged = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "conductor park: AddRaceCarDeformationModel @0x825E9118 passes a NULL "
                           "model handle -- BrnPhysics::Deformation::ResourceHandle is a 4-byte "
                           "stand-in for the 16-byte CgsResource::ResourceHandle the console hands "
                           "it [FLAG PC boot gate]\n";
                }
            }
        }
        Deformation::ResourceHandle lParkedModelHandle;
        lParkedModelHandle.muValue = 0u;

        // ⚠️ [FLAG PC bring-up] NULL-mpAttribs TRIPWIRE (verifier catch, 2026-08-11): the console
        // reads mpAttribs unconditionally here, but VehiclePhysics::Construct seeds it NULL and
        // only Prepare fills it -- and the create drain SKIPS Prepare when the event carries
        // mbDisablePhysicsStateReset (true for a player-car physics-state reset). On the console
        // that read survives; on the host it is a frame-one AV at +0x20 that would be misread as
        // a physics-loop failure. The assert makes the precondition loud; the guard below keeps
        // the boot alive by skipping only this deformation post (already parked NULL-handle inert).
        CGS_ASSERT(maRaceCarVehicles[liVehicleIndex].mpAttribs != 0,
                   "AddRaceCarDeformationModel: mpAttribs != NULL (create skipped Prepare?)");
        if (maRaceCarVehicles[liVehicleIndex].mpAttribs == 0)
        {
            return;
        }

        // The 4-byte `::RigidBodyId` stand-in carries the ENTITY WORD (see this file's banner).
        RigidBodyId lHandlingBodyIDField;
        lHandlingBodyIDField.muValue = static_cast<u32>(lHandlingBodyId.GetEntityId());

        lpDeformationInterface->AddDeformationModel(
            lParkedModelHandle,
            lHandlingBodyIDField,
            lGlobalEntityId,
            &maRaceCarVehicles[liVehicleIndex],
            maRaceCarVehicles[liVehicleIndex].mpAttribs->mBaseAttribs.mCOMOffset,
            lInitialWorldSpaceTransform,
            lvZero,
            lvZero,
            lfInitialDamageAmount,
            leBaseDeformationType,
            true);
    }

    // =============================================================================================
    // SetAllNetworkRaceCarsHidden @0x825E9380 (175 insns)
    //
    // 175 instructions of which almost all are the INLINED CgsContainers::BitArray<8> cursor walk
    // (`GetFirstNonZeroBit` / `GetNextNonZeroBit`, the `addi r11,r11,-1 ; and ; subf ; cntlzd ;
    // subf ; addi 0x3F` lowest-set-bit idiom) plus its two bounds asserts. The actual work is three
    // lines. The bitset seat is `addis r19,r15,1 ; addi r19,r19,-0x5340` == this + 44224 ==
    // mUsedRaceCars, and the per-car test is `addi r11,r31,0x2B28 ; slwi r11,r11,2 ; lwzx` ==
    // maeRaceCarTypes[i], compared against 2 == E_RACE_CAR_TYPE_NETWORK.
    //
    // ⚠️ The only argument is r4, forwarded verbatim as SetNetworkRaceCarHidden's r5 -- the frame
    // count, not a bool. Its one call site (ProcessCreateEvents @0x826177CC) passes `li r4, 1`.
    // =============================================================================================
    void VehicleManager::SetAllNetworkRaceCarsHidden(s32 liFrames)
    {
        for (s32 liCar = mUsedRaceCars.GetFirstNonZeroBit();
             liCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liCar = mUsedRaceCars.GetNextNonZeroBit(liCar))
        {
            if (maeRaceCarTypes[liCar] == BrnWorld::E_RACE_CAR_TYPE_NETWORK)
            {
                SetNetworkRaceCarHidden(static_cast<EActiveRaceCarIndex>(liCar), liFrames);
            }
        }
    }

    // ---- the traffic twin, still a LOUD one-shot gate --------------------------------------------
    // ⛔ NEVER silently no-op this: the log-once IS the loudness. @0x82649768 (246) carries its own
    // create/remove/crash arms over the twenty traffic slots and is its own wave.
    void PhysicalTrafficManager::ProcessTrafficMaintenanceEvents(
        CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
        const VehicleInputInterface*, VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, VehicleOutputInterface*,
        BrnPhysics::Deformation::DeformationInputInterface*)
    {
        BRN_MAINTENANCE_GATE("PhysicalTrafficManager::ProcessTrafficMaintenanceEvents @0x82649768 (246)");
    }
}
}
