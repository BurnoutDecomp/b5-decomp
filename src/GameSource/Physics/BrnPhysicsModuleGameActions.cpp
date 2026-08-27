// =================================================================================================
// BrnPhysicsModuleGameActions.cpp
//
// PhysicsModule::HandleGameActions @0x825A72F0 (185 insns; PS3 0x69CD60) -- THE PHYSICS SIDE OF THE
// GAME-ACTION PIPE. Landed 2026-08-27 (showtime S3 wave); its conductor gate in
// BrnPhysicsConductorGates.cpp is deleted in the same commit.
//
// WHY THIS ONE FUNCTION MATTERED
// ------------------------------
// `VehicleManager::SetPlayerCarToShowtimeMode @0x8259C108` has EXACTLY ONE console caller, and it is
// this function's case 23. Everything upstream of here was already complete and mounted --
//     GameStateModule -> GameStateModuleIO::OutputBuffer::GameActionQueue
//       -> BridgeGameStateToWorld -> WorldModule::HandleGameActions
//       -> WorldModule::BridgeActionsToPhysicsModule   (allowlist {7,11,23,34,37,39,42,43,65,97,98,
//                                                        99,116,135,138,146,176,198})
//       -> PhysicsModuleIO::InputBuffer::mGameActionQueue -> HERE
// -- so the physics module was being handed every showtime action every frame and dropping all of
// them on the floor. The 2026-08-27 pre-change control confirmed the *call* arrives: the gate's
// one-shot printed in a live run (scratch/flow_run/shipDefault/BrnGame.log:1077). It could not
// confirm anything about the *events*, which is what the [s3-action] witness below is for.
//
// ⚠️⚠️ HEX-RAYS IS WRONG ABOUT THIS FUNCTION IN THREE PLACES. Every one of them was caught by
// reading the asm, and each is documented at its arm:
//   * case 42 StartImpactTime -- an entire FLOAT argument dropped and the surviving one mistyped.
//   * case 37/39 OnGameModeStop -- an argument INVENTED (the call site's `lwz r4` is a dead store).
//   * case 176 -- rendered as a call to `BaseCollisionGenerator::Destruct`. That address (0x8284CB38)
//     is a single `blr`: IDA folded an empty body onto the name (ICF). The arm IS A NO-OP.
// [[reconstruction-gotchas]] -- use --asm; Hex-Rays drops arguments AND invents them.
//
// ⭐ THE BIT DECODE AND THE MEMBER LAYOUT ARE AN INDEPENDENT CROSS-CHECK OF EACH OTHER.
// The case-23 flag word is read as `ld r11, 0x860(r30)` (r30 == event + 0x30, so the doubleword at
// event+2192) and masked with rlwinm on its LOW word == event+2196. Decoding the rlwinm MASK FIELDS
// rather than trusting the pseudocode's literals gives:
//     rlwinm 0,28,28 -> 1<<3  = 0x8       KU_FLAG_EASY_CRASHING
//     rlwinm 0,22,22 -> 1<<9  = 0x200     KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR   (BrnGameModeParams.h:147)
//     rlwinm 0,27,27 -> 1<<4  = 0x10      KU_FLAG_PLAYER_MUST_BE_CRASHING
//     rlwinm 0,4,4   -> 1<<27 = 0x8000000 KU_FLAG_DONUT_START
//     rlwinm 0,12,12 -> 1<<19 = 0x80000   KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED
// and each of the five store targets resolves, through the already-committed layouts, to a member
// whose NAME says the same thing the flag does:
//     this+191409 = mVehicleManager+172305 = mbEasyCrashingEnabled      <- 0x8
//     this+191413 = mVehicleManager+172309 = mbCrashPlayerNextUpdate    <- 0x10
//     this+191417 = mVehicleManager+172313 = mbTrafficCheckingAllowed   <- !(0x80000)
//     this+190568 = mVehicleManager+171464 = mbSlamsAndShuntsOn
//     this+191552 = mVehicleManager+172448 = meStationaryPlayerWheelAngle
// The flag names come from BrnGameModeParams.h and the member names from the DWARF-anchored
// VehicleManager layout; neither wave knew about the other. That agreement is the calibration
// control for the whole decode. [[diagnostics-that-lie]] -- and this one does not.
//
// ⭐ THE TRAFFIC-CHECKING ARM IS INVERTED, ON PURPOSE.
//     0x825A77D4  cntlzw r11, r11        ; r11 is 1 when the flag is set, else 0
//     0x825A77D8  extrwi r11, r11, 1,26  ; take bit 26 == the 0x20 place of the count
// cntlzw(1) == 31 (0b011111, 0x20 place CLEAR); cntlzw(0) == 32 (0b100000, 0x20 place SET). So the
// stored byte is 1 exactly when the flag is CLEAR: `mbTrafficCheckingAllowed = !(flags &
// KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED)`. Written as the negation, which is what it is -- the
// cntlzw/extrwi pair is the compiler's branchless `!x`, not a computation.
// [[check-what-a-zeroed-field-means]] -- the member and the flag have OPPOSITE polarity.
//
// ⚠️ FOUR ARMS ARE NOT LANDED HERE, AND THEY ARE NAMED, NOT SILENT. Ids 11 / 116 / 198 each call a
// body that does not exist anywhere in the tree; each gets its own one-shot deferral line so a
// future run says which one was hit rather than producing a plausible nothing.
// [[silent-drop-stubs]]. None of the three is on the showtime path.
// =================================================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // SetShowtimeAimDirection
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationInputInterface.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // GameStateModuleIO::GameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                               // PrepareForModeAction + GameModeParams::KU_FLAG_*
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                     // gpDebugPrint ([s3-action] witness)

namespace BrnPhysics
{
    namespace
    {
        // -----------------------------------------------------------------------------------------
        // THE X360 GAME-ACTION IDS THIS SWITCH ANSWERS.
        // The console's switch is `addi r11, r3, -7 ; cmplwi r11, 0xBF` over a 192-entry jump table,
        // so jump-table case K is action id K + 7. Spelled as named constants because every one of
        // them is a wire id shared with the GameState side.
        // -----------------------------------------------------------------------------------------
        const s32 KI_ACTION_SET_STATIONARY_WHEEL_ANGLE   = 7;    // jt case 0
        const s32 KI_ACTION_NETWORK_CAR_DISCONNECT       = 11;   // jt case 4
        const s32 KI_ACTION_PREPARE_GAME_MODE            = 23;   // jt case 16
        const s32 KI_ACTION_START_PLAYING_MODE           = 34;   // jt case 27
        const s32 KI_ACTION_STOP_GAME_MODE_A             = 37;   // jt case 30
        const s32 KI_ACTION_STOP_GAME_MODE_B             = 39;   // jt case 32
        const s32 KI_ACTION_START_IMPACT_TIME            = 42;   // jt case 35
        const s32 KI_ACTION_END_IMPACT_TIME              = 43;   // jt case 36
        const s32 KI_ACTION_CLEAR_STATIONARY_WHEEL_ANGLE = 65;   // jt case 58
        const s32 KI_ACTION_RESET_PLAYER_SCRATCHES       = 98;   // jt case 91
        const s32 KI_ACTION_FORWARD_TO_OUTPUT            = 116;  // jt case 109
        const s32 KI_ACTION_SET_SHOWTIME_BEHAVIOUR       = 138;  // jt case 131
        const s32 KI_ACTION_SET_SHOWTIME_AIM_DIRECTION   = 146;  // jt case 139
        const s32 KI_ACTION_COLLISION_GENERATOR_NOOP     = 176;  // jt case 169
        const s32 KI_ACTION_PLAYER_STATS_UPDATE          = 198;  // jt case 191

        // -----------------------------------------------------------------------------------------
        // ⛔⛔ THE CASE-23 PAYLOAD IS READ BY NAME, NOT BY X360 DISPLACEMENT -- AND THAT WAS
        // MEASURED, NOT ASSUMED.
        //
        // The first cut of this file transcribed the console's displacements literally:
        //     r30 = event + 0x30 ; lbz 0x94(r30) ; lwz 0x148(r30) ; ld 0x860(r30)
        // i.e. event + 196 / + 376 / + 2196. The FIRST run that actually produced an action 23
        // printed the witness line
        //     [s3-action] case 23 PREPARE_GAME_MODE: size 1792 substate 1 modeType -992477571
        //                 flags 0x00000000 SHOWTIME=0
        // -- three tells at once. The console record is 0x8E0 == 2272 bytes; THIS one is 1792, so
        // +2196 was an OUT-OF-BOUNDS READ (which is why the flags came back a clean zero, the most
        // plausible-looking wrong answer there is), and +376 landed mid-member and produced visible
        // garbage. The host `PrepareForModeAction` is a fully typed struct whose embedded
        // GameModeParams has real members, real enums and host-width padding; it does not reproduce
        // the console's byte map and BrnGameModeParams.h's own banner says so outright ("NOTHING
        // below indexes by offset; the host layout is by named member").
        //
        // Every read is therefore routed through the accessors the rest of the tree already uses --
        // the same three RaceCarEntityModule::HandlePrepareForModeAction reaches for:
        //     lpAction->IsFirstPrepareForMode()      == the console's `ev[0]==0 || ev[0]==1` guard
        //     lpParams->GetGameModeType()            == `lwz 0x148(r30)`   (that accessor's own
        //                                               banner pins it to 0x148)
        //     lpParams->GetFlag(KU_FLAG_...)         == `ld 0x860(r30)` + rlwinm (GetFlag @0x821F2C88
        //                                               IS that load)
        //     lpParams->mbIsOnline                   == `lbz 0x94(r30)`   (pinned at +0x94 by an
        //                                               unrelated wave, GameBridgeGameStateToX_
        //                                               EventFlowGuiEvents.cpp:174)
        //
        // ⭐ AND THE FLAG CONSTANTS CROSS-CHECK THE ASM DECODE EXACTLY. The five bits recovered here
        // from the rlwinm MASK FIELDS -- 0x8 / 0x10 / 0x200 / 0x80000 / 0x8000000 -- are, by name in
        // BrnGameModeParams.h:141/142/147/157/165, ENABLE_EASY_CRASHING / PLAYER_MUST_BE_CRASHING /
        // USE_SHOWTIME_VEHICLE_BEHAVIOUR / TRAFFIC_CHECKING_NOT_ALLOWED / DONUT_START. Five for
        // five, against a table this wave did not write. The named constants are used below; the
        // literals are not repeated.
        // [[diagnostics-that-lie]] · [[serialized-slots-stay-32-bit]] -- host layout is NOT console
        // layout, and no gate can see the difference.
        //
        // The remaining arms (7 / 42 / 65 / 138 / 146) read only the payload's leading scalar(s),
        // for which no typed host record exists at all; those are byte-read, and each one's witness
        // prints the value it read so a future layout divergence shows up the same way this one did.
        // -----------------------------------------------------------------------------------------
        const u32 KU_EV_LEADING_WORD    = 0;      // s32/u32 -- `lwz r11, 0(r29)`
        const u32 KU_EV_IMPACT_DURATION = 0;      // f32 -- case 42 `lfs f1, 0(r29)`
        const u32 KU_EV_IMPACT_ADDITIVE = 4;      // u8  -- case 42 `lbz r5, 4(r29)`

        // -----------------------------------------------------------------------------------------
        // [s3-action] -- THE PROOF WITNESS. NOT X360. One line per DISTINCT action id, ever.
        //
        // ⭐⭐ IT HAS TWO PARTS ON PURPOSE, BECAUSE ONE OF THEM CANNOT SEE THE OTHER'S FAILURE.
        // A per-id witness that prints nothing is ambiguous: it means either "no action arrived" or
        // "the witness is broken / the drain never ran". So the FIRST call also prints the queue's
        // length unconditionally. Between them, silence has exactly one meaning left -- the function
        // was never called at all, which the conductor gate already disproved.
        // [[diagnostics-that-lie]] -- ask what the probe cannot see.
        //
        // The id table covers 0..255. An id outside it would otherwise be invisible, so it gets a
        // line of its own (repeating, but such an id is rare) instead of vanishing.
        // DELETE-WHEN: the showtime chain is proven end to end and no longer under investigation.
        // -----------------------------------------------------------------------------------------
        bool gabActionIdSeen[256] = { false };

        void WitnessAction(s32 liAction, s32 liSize)
        {
            if (CgsDev::Log::gpDebugPrint == 0)
                return;

            if (liAction < 0 || liAction > 255)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[s3-action] id " << liAction << " is OUTSIDE the 0..255 witness table"
                    << " (size " << liSize << ")\n";
                return;
            }

            if (!gabActionIdSeen[liAction])
            {
                gabActionIdSeen[liAction] = true;
                *CgsDev::Log::gpDebugPrint
                    << "[s3-action] FIRST arrival of game action id " << liAction
                    << " (size " << liSize << ")\n";
            }
        }

        // ⚠️ ONE-SHOT PER ID: print the leading scalar an offset-read arm actually consumed.
        // The case-23 OOB read (see the payload banner below) was only visible because the witness
        // printed the VALUE, not just "the arm ran". The arms that still read the payload by byte
        // offset get the same treatment for the same reason -- a host record whose first member is
        // not a 4-byte scalar would show up here as garbage instead of as silent wrong behaviour.
        bool gabLeadingWordSeen[256] = { false };

        void WitnessLeadingWord(s32 liAction, s32 liValue)
        {
            if (CgsDev::Log::gpDebugPrint == 0 || liAction < 0 || liAction > 255)
                return;
            if (gabLeadingWordSeen[liAction])
                return;
            gabLeadingWordSeen[liAction] = true;
            *CgsDev::Log::gpDebugPrint
                << "[s3-action] id " << liAction << " leading word = " << liValue << "\n";
        }
    }

    // =============================================================================================
    // PhysicsModule::HandleGameActions  @0x825A72F0  (185 insns)
    // =============================================================================================
    void PhysicsModule::HandleGameActions(
        const BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
        PhysicsModuleIO::OutputBuffer* lpOutputBuffer)
    {
        CGS_ASSERT(lpGameActionQueue, "lpGameActionQueue");

        // [s3-action] first-call witness -- see the banner on WitnessAction.
        {
            static bool sbFirstDrainReported = false;
            if (!sbFirstDrainReported && CgsDev::Log::gpDebugPrint != 0)
            {
                sbFirstDrainReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[s3-action] first drain: PhysicsModule::HandleGameActions is LIVE, queue length "
                    << lpGameActionQueue->GetLength() << "\n";
            }
        }

        const CgsModule::Event* lpEventData = 0;
        s32 liEventSize = 0;
        s32 liAction = lpGameActionQueue->GetFirstEvent(&lpEventData, &liEventSize);

        while (lpEventData)
        {
            // The payload is the raw GameAction record; its own event structs are not homed, so it
            // is read with byte displacements exactly as the console does (the same convention
            // WorldModule::HandleGameActions already uses on the same queue).
            const u8* const lpu8Payload = reinterpret_cast<const u8*>(lpEventData);

            WitnessAction(liAction, liEventSize);

            switch (liAction)
            {
                // -------------------------------------------------------------------------------
                // 7 -- asm 0x825A7890. `lwz r11,0(r29) ; cmpwi 0 ; beq skip ; stwx r16(=2), +191552`
                // A WORD store, not a byte.
                // -------------------------------------------------------------------------------
                case KI_ACTION_SET_STATIONARY_WHEEL_ANGLE:
                {
                    const s32 liLeadingWord =
                        *reinterpret_cast<const s32*>(lpu8Payload + KU_EV_LEADING_WORD);
                    if (liLeadingWord != 0)
                    {
                        mVehicleManager.meStationaryPlayerWheelAngle = 2;
                    }
                    WitnessLeadingWord(liAction, liLeadingWord);
                    break;
                }

                // -------------------------------------------------------------------------------
                // 11 -- VehicleManager::ProcessNetworkCarDisconnect @0x825C53F8 (73 insns).
                // DEFERRED: no body in the tree. It resets one network car's per-slot physics
                // block (56-word stride) and is reachable only in an online session, so it is not
                // on the showtime path this wave is opening.
                // -------------------------------------------------------------------------------
                case KI_ACTION_NETWORK_CAR_DISCONNECT:
                {
                    static bool sbLogged = false;
                    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLogged = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[s3-action] id 11 DEFERRED: VehicleManager::ProcessNetworkCarDisconnect"
                               " @0x825C53F8 (73) has no body in the tree [FLAG]\n";
                    }
                    break;
                }

                // -------------------------------------------------------------------------------
                // 23 -- THE MODE-PREPARE ARM. asm 0x825A76D0..0x825A77E8. ⭐ THIS IS THE SHOWTIME
                // ENTRY: bit 0x200 is the only console road into SetPlayerCarToShowtimeMode.
                //
                // The guard is `ev[0] == 0 || ev[0] == 1` -- the console builds it as a two-branch
                // select into a byte and then tests the byte, which Hex-Rays renders as the
                // `(v9 = 0, ...)` comma expression. Semantically it is just the disjunction.
                // -------------------------------------------------------------------------------
                case KI_ACTION_PREPARE_GAME_MODE:
                {
                    const BrnGameState::GameStateModuleIO::PrepareForModeAction* const lpAction =
                        reinterpret_cast<
                            const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpEventData);

                    // `lwz r11,0(r29) ; cmpwi 0 ; beq take ; cmpwi 1 ; bne skip` -- the console
                    // builds it as a two-branch select into a byte and then tests the byte, which is
                    // what Hex-Rays renders as the `(v9 = 0, ...)` comma expression. Semantically it
                    // is the disjunction, and it already has a named home: the stage word IS
                    // mePrepareForModeStage, and E_PFM_STAGE_ALL_IN_ONE / E_PFM_STAGE_FIRST_OF_TWO
                    // are 0 and 1.
                    if (!lpAction->IsFirstPrepareForMode())
                        break;

                    const BrnGameState::GameModeParams* const lpParams =
                        lpAction->GetGameModeParams();
                    CGS_ASSERT(lpParams, "lpParams");

                    const BrnGameState::GameStateModuleIO::EGameModeType leGameModeType =
                        lpParams->GetGameModeType();                       // `lwz 0x148(r30)`

                    // asm 0x825A76FC: `lbz r11, 0x94(r30) ; stbx r11, r31, r23(=0x69C38)`.
                    // ONE byte -- this is what pins the physics module's last data byte at 433208.
                    mbIsOnlineGameMode = lpParams->mbIsOnline;             // `lbz 0x94(r30)`

                    mVehicleManager.mbEasyCrashingEnabled =
                        lpParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_ENABLE_EASY_CRASHING);

                    // ⭐⭐⭐ THE SHOWTIME DOOR. This `if` is the only console road into
                    // SetPlayerCarToShowtimeMode @0x8259C108 that exists anywhere in the image.
                    if (lpParams->GetFlag(
                            BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR))
                    {
                        mVehicleManager.SetPlayerCarToShowtimeMode(true);
                    }

                    // Set-only: the console has no `else` here. The flag is consumed and cleared by
                    // UpdateVehiclePhysics's deferred crash block, not by this arm.
                    if (lpParams->GetFlag(
                            BrnGameState::GameModeParams::KU_FLAG_PLAYER_MUST_BE_CRASHING))
                    {
                        mVehicleManager.mbCrashPlayerNextUpdate = true;
                    }

                    if (lpParams->GetFlag(BrnGameState::GameModeParams::KU_FLAG_DONUT_START))
                    {
                        mVehicleManager.SwitchPlayerAIDonuttingAttribs(true);
                    }

                    mVehicleManager.mbSlamsAndShuntsOn = false;      // asm: stbx r27(=0) @ +190568

                    mVehicleManager.OnGameModePrepare(static_cast<s32>(leGameModeType));

                    // The cntlzw/extrwi negation -- see the banner. Kept in asm order (the console
                    // emits this store AFTER OnGameModePrepare).
                    mVehicleManager.mbTrafficCheckingAllowed =
                        !lpParams->GetFlag(
                            BrnGameState::GameModeParams::KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED);

                    meCurrentGameMode = leGameModeType;

                    // [s3-action] one-shot: the arm that matters, printing BOTH what it read and its
                    // own post-condition. It is the version of this line that caught the OOB read
                    // described in the banner above; keep it printing the raw flag word.
                    {
                        static bool sbLogged = false;
                        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                        {
                            sbLogged = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[s3-action] case 23 PREPARE_GAME_MODE: size " << liEventSize
                                << " hostSizeof "
                                << static_cast<s32>(
                                       sizeof(BrnGameState::GameStateModuleIO::PrepareForModeAction))
                                << " modeType " << static_cast<s32>(leGameModeType)
                                << " online " << (lpParams->mbIsOnline ? 1 : 0)
                                // muFlags itself is private; print the five bits this arm reads,
                                // each through the same GetFlag the code above uses -- so the
                                // witness cannot disagree with the behaviour it is witnessing.
                                << " easyCrash "
                                << (lpParams->GetFlag(BrnGameState::GameModeParams::
                                                          KU_FLAG_ENABLE_EASY_CRASHING) ? 1 : 0)
                                << " mustCrash "
                                << (lpParams->GetFlag(BrnGameState::GameModeParams::
                                                          KU_FLAG_PLAYER_MUST_BE_CRASHING) ? 1 : 0)
                                << " noTrafficCheck "
                                << (lpParams->GetFlag(BrnGameState::GameModeParams::
                                                          KU_FLAG_TRAFFIC_CHECKING_NOT_ALLOWED) ? 1 : 0)
                                << " donut "
                                << (lpParams->GetFlag(BrnGameState::GameModeParams::
                                                          KU_FLAG_DONUT_START) ? 1 : 0)
                                << " SHOWTIME="
                                << (lpParams->GetFlag(BrnGameState::GameModeParams::
                                                          KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR)
                                        ? 1 : 0)
                                << " -> meCurrentGameModeType now "
                                << mVehicleManager.meCurrentGameModeType << "\n";
                        }
                    }
                    break;
                }

                // -------------------------------------------------------------------------------
                // 34 -- asm 0x825A77EC. Slams/shunts back ON, donutting attribs OFF.
                // -------------------------------------------------------------------------------
                case KI_ACTION_START_PLAYING_MODE:
                {
                    mVehicleManager.mbSlamsAndShuntsOn                    = true;   // +190568
                    mVehicleManager.mbAllowSlamsAndShuntsEffectsForRivals  = true;   // +190569
                    mVehicleManager.SwitchPlayerAIDonuttingAttribs(false);
                    break;
                }

                // -------------------------------------------------------------------------------
                // 37 / 39 -- asm 0x825A7804 (ONE shared arm; the jump table sends cases 30 and 32
                // to the same address). Leave showtime and clear the mode.
                //
                // ⚠️ The `lwz r4, 0(r29)` at 0x825A7824 is a DEAD STORE: OnGameModeStop @0x825B5718
                // is five instructions and never reads r4. Hex-Rays shows no argument either. Not
                // reproduced -- reproducing it would mean inventing a parameter.
                // -------------------------------------------------------------------------------
                case KI_ACTION_STOP_GAME_MODE_A:
                case KI_ACTION_STOP_GAME_MODE_B:
                {
                    mVehicleManager.mbEasyCrashingEnabled = false;     // +191409
                    mVehicleManager.SetPlayerCarToShowtimeMode(false);
                    mbIsOnlineGameMode                        = false; // +433208
                    mVehicleManager.mbTrafficCheckingAllowed  = true;  // +191417
                    mVehicleManager.OnGameModeStop();
                    meCurrentGameMode =
                        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(-1);  // stwx r21(=-1)
                    break;
                }

                // -------------------------------------------------------------------------------
                // 42 -- asm 0x825A76A0. See StartImpactTime's banner: the float in f1 and the bool
                // in r5 are BOTH arguments, and Hex-Rays shows one.
                // -------------------------------------------------------------------------------
                case KI_ACTION_START_IMPACT_TIME:
                {
                    const f32 lfDuration =
                        *reinterpret_cast<const f32*>(lpu8Payload + KU_EV_IMPACT_DURATION);
                    const bool lbAdditive = (lpu8Payload[KU_EV_IMPACT_ADDITIVE] != 0);
                    mVehicleManager.StartImpactTime(lfDuration, lbAdditive);
                    {
                        static bool sbLogged = false;
                        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                        {
                            sbLogged = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[s3-action] case 42 START_IMPACT_TIME: duration " << lfDuration
                                << " additive " << (lbAdditive ? 1 : 0) << "\n";
                        }
                    }
                    break;
                }

                case KI_ACTION_END_IMPACT_TIME:            // asm 0x825A76C4
                {
                    mVehicleManager.EndImpactTime();
                    break;
                }

                case KI_ACTION_CLEAR_STATIONARY_WHEEL_ANGLE:   // asm 0x825A7888, WORD store
                {
                    mVehicleManager.meStationaryPlayerWheelAngle = 0;
                    break;
                }

                // -------------------------------------------------------------------------------
                // 98 -- asm 0x825A78A4 `stbx r28(=1), r31, r18(=0x60B30)`. 0x60B30 == +396080 ==
                // mDeformationInput + 5056 == mbResetPlayerScratches. The console inlines the store;
                // the member is reached by name here (DeformationInputInterface befriends this
                // class for exactly this one byte).
                // -------------------------------------------------------------------------------
                case KI_ACTION_RESET_PLAYER_SCRATCHES:
                {
                    mDeformationInput.mbResetPlayerScratches = true;
                    break;
                }

                // -------------------------------------------------------------------------------
                // 116 -- asm 0x825A7844: `sub_8259FFD8(outputBuffer)` (an assert-and-offset
                // accessor returning outputBuffer + 41952, asserting "Not locked for writing" from
                // BrnPhysicsModuleIO.h:352), then `+0x750` == +1872 and
                // `BaseEventQueue<short>::AddEvent @0x825A3148` (78 insns).
                // DEFERRED: neither the accessor's seat nor that queue is homed on the committed
                // PhysicsModuleIO::OutputBuffer, and the arm is not on the showtime path.
                // -------------------------------------------------------------------------------
                case KI_ACTION_FORWARD_TO_OUTPUT:
                {
                    static bool sbLogged = false;
                    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLogged = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[s3-action] id 116 DEFERRED: the OutputBuffer +41952/+1872 event"
                               " forward (AddEvent @0x825A3148) is not homed [FLAG]\n";
                    }
                    break;
                }

                case KI_ACTION_SET_SHOWTIME_BEHAVIOUR:     // asm 0x825A7868
                {
                    const s32 liBehaviour =
                        *reinterpret_cast<const s32*>(lpu8Payload + KU_EV_LEADING_WORD);
                    WitnessLeadingWord(liAction, liBehaviour);
                    // ⚠️ SetShowtimeBehaviour asserts `< 3`. If that ever fires, read the witness
                    // line above FIRST: a leading word outside 0..2 means the payload was read
                    // wrong, not that the producer sent a bad behaviour.
                    mVehicleManager.SetShowtimeBehaviour(static_cast<u32>(liBehaviour));
                    break;
                }

                // -------------------------------------------------------------------------------
                // 146 -- asm 0x825A785C:
                //     lvx128 v1, r0, r29
                //     bl     RaceCarPhysics::SetShowtimeAimDirection
                // ⭐ NOTE WHAT IS *NOT* THERE: r3 is never set. The callee @0x825B8AF0 is
                //     lis r11, msPlayerParams@ha ; li r10, 0x20 ; addi r11,r11,@l ; stvx128 v1,r11,r10
                // -- a store into the MODULE-STATIC showtime singleton at +0x20, with no `this`
                // touched at all. That is why SetShowtimeAimDirection is declared static: calling it
                // on a car would imply a per-car aim slot the console does not have, and would need
                // a player index this arm never reads. The event's first 16 bytes ARE the vector
                // (`lvx128 v1, r0, r29` -- displacement zero).
                // -------------------------------------------------------------------------------
                case KI_ACTION_SET_SHOWTIME_AIM_DIRECTION:
                {
                    const Vector3& lrAim = *reinterpret_cast<const Vector3*>(lpu8Payload);
                    Vehicle::RaceCarPhysics::SetShowtimeAimDirection(lrAim);
                    {
                        static bool sbLogged = false;
                        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                        {
                            sbLogged = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[s3-action] case 146 SET_SHOWTIME_AIM_DIRECTION: size "
                                << liEventSize << " aim " << lrAim.x << " " << lrAim.y
                                << " " << lrAim.z << "\n";
                        }
                    }
                    break;
                }

                // -------------------------------------------------------------------------------
                // 176 -- asm 0x825A7878 calls 0x8284CB38, which IDA names
                // `CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct`.
                // ⭐⭐ THAT ADDRESS IS ONE INSTRUCTION: `blr`. The linker folded an empty body onto
                // that symbol (ICF), and IDA reported the surviving name. THE ARM IS A NO-OP -- so
                // it is written as one. Calling a `BaseCollisionGenerator::Destruct(&mVehicleManager,
                // ev[1])` here would be inventing an API out of a name collision, and the two
                // "arguments" the call site loads would be doing nothing on the console either.
                // [[unnamed-sub-bodies-and-env-faults]] -- a NAME is not a body.
                // -------------------------------------------------------------------------------
                case KI_ACTION_COLLISION_GENERATOR_NOOP:
                {
                    break;
                }

                // -------------------------------------------------------------------------------
                // 198 -- PhysicsModule::HandlePlayerStatsUpdate @0x8259C910 (26 insns).
                // DEFERRED: declared nowhere in the tree. Not on the showtime path.
                // -------------------------------------------------------------------------------
                case KI_ACTION_PLAYER_STATS_UPDATE:
                {
                    static bool sbLogged = false;
                    if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLogged = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[s3-action] id 198 DEFERRED: PhysicsModule::HandlePlayerStatsUpdate"
                               " @0x8259C910 (26) has no body in the tree [FLAG]\n";
                    }
                    break;
                }

                default:
                    break;
            }

            // The console reads the current event from r29 and writes the next one into a DISTINCT
            // stack slot (`addi r5, r1, var_A0`), then reloads r29 from it. Kept distinct here too
            // rather than aliasing the in and out pointers.
            const CgsModule::Event* lpNextEvent = 0;
            liAction = lpGameActionQueue->GetNextEvent(lpEventData, &lpNextEvent, &liEventSize);
            lpEventData = lpNextEvent;
        }

        (void)lpOutputBuffer;   // only case 116 reads it, and that arm is deferred
    }
}
