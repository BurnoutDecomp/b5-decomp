// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_AssertLayout.cpp
// ============================================================================
// COMPILE-ONLY LAYOUT ORACLE for BrnGameState::ModeManager (wave-B keystone, agent 1).
// Same role as BrnModeManager_EmbedGate.cpp: it is never called and never mounted; it exists so
// that a wrong edit to the member run in BrnModeManager.h FAILS THE COMPILE GATE instead of being
// discovered later as a silently mis-read field.
//
// WHAT IT CAN AND CANNOT PIN
// --------------------------
// The host object is NOT byte-identical to the console's. Pointers widen 4 -> 8, and every embedded
// sub-object (the fourteen modes, ScoringSystem, HUDMessageLogic, GameModeParams,
// PrepareForModeAction, ...) has its own host size. So ABSOLUTE offsets are meaningless here and are
// never asserted. What IS load-bearing, and what is asserted below, is:
//   (a) the SIZES of the pointer-free wire/record types the console memcpy's or bit-tests;
//   (b) the ARRAY EXTENTS -- an off-by-one on any of these is a silent out-of-bounds in the spine;
//   (c) the ORDER of the member run (an offsetof monotonicity chain), so nobody re-sorts it into
//       "tidier" groups and quietly breaks the console correspondence the comments claim;
//   (d) the DELTAS inside the pointer-free tail, which ARE host-stable and are the ones the spine's
//       arithmetic actually depends on (the 25-byte bool block, the three clocks, the PFM cache).
//
// [!] offsetof on a class with private members / embedded polymorphic sub-objects is
// conditionally-supported; MSVC accepts it. These asserts are all `<` or fixed deltas, never
// absolute values.

#include <cstddef>   // offsetof

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

namespace BrnGameState
{

void ModeManager::_AssertLayout()
{
    // ---------------------------------------------------------------------------------------
    // (a) SIZES -- pointer-free record types the console treats as raw bytes.
    // ---------------------------------------------------------------------------------------
    static_assert(sizeof(GameStateModuleIO::CarCheckpointData) == 8,
                  "CarCheckpointData is an 8-byte per-car checkpoint bit set (X360 stride at "
                  "ModeManager+32480; 35 of them span 280 bytes to +32760)");
    static_assert(sizeof(CgsContainers::BitArray<E_GLOBAL_RACE_CAR_INDEX_COUNT>) == 8,
                  "BitArray<35> is ONE 64-bit field -- ModeManager::Construct zeroes each of the two "
                  "reached-sets with a single `std` (X360 +0x8BF0 / +0x8BF8)");
    static_assert(sizeof(LandmarkIndex) == 2,
                  "LandmarkIndex wraps one s16 region index -- ClearLandmarkAndFinishLineData stores "
                  "the 0xFFFF sentinel into mPlayerCurrentLandmark with a half-word `sthx`");
    static_assert(sizeof(CgsSystem::TimerStatusInterface) == 48,
                  "TimerStatusInterface is two back-to-back 24-byte TimerStatus blocks. This size is "
                  "what seats mTimerStatusInterface at console +28064 (mRandom's 48 bytes from a "
                  "16-aligned +28016) and makes UpdateCurrentMode's +28092 / +28096 reads land on the "
                  "SIM sub-status's mfBaseTimeStep / mfTimeStepMultiplier");
    static_assert(sizeof(CgsNumeric::Random) == 48,
                  "mRandom must stay 48 bytes or mTimerStatusInterface's console seat moves");

    // ---------------------------------------------------------------------------------------
    // (b) ARRAY EXTENTS.
    // ---------------------------------------------------------------------------------------
    static_assert(KI_GAME_MODE_SLOTS == 18,
                  "EIGHTEEN dispatch slots, not the DWARF's 17: Construct zeroes 18 and ctor-loops "
                  "18, StartGameMode / GetCountdownTimeForMode assert `< 0x12`, the countdown table "
                  "at X360 0x82020E98 is 18 f32s, and slot 17 is a real dispatchable mode");
    static_assert(static_cast<s32>(GameStateModuleIO::E_MODE_ONLINE_MODE_END) < KI_GAME_MODE_SLOTS,
                  "every EGameModeType value must index inside mapGameModes -- this is the assert "
                  "that catches anyone 'fixing' E_MODE_COUNT/E_MODE_ONLINE_MODE_END upward");
    static_assert(sizeof(reinterpret_cast<ModeManager*>(0)->mapGameModes) / sizeof(GameMode*) == KI_GAME_MODE_SLOTS,
                  "mapGameModes extent");

    static_assert(GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE == 16, "KI_MAX_LANDMARKS_IN_MODE");
    static_assert(E_GLOBAL_RACE_CAR_INDEX_COUNT == 35, "E_GLOBAL_RACE_CAR_INDEX_COUNT");
    static_assert(E_ACTIVE_RACE_CAR_INDEX_COUNT == 8,  "E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // The three landmark arrays and the two per-global-car arrays, pinned by their byte spans (all
    // pointer-free, so these deltas are console-true as well as host-true).
    static_assert(offsetof(ModeManager, maLandmarkCgsIDs) - offsetof(ModeManager, maLandmarkIndices) == 32,
                  "maLandmarkIndices is u16[16] == 32 bytes (X360 +32288 -> +32320)");
    static_assert(offsetof(ModeManager, mauLandmarkSectionIndices) - offsetof(ModeManager, maLandmarkCgsIDs) == 128,
                  "maLandmarkCgsIDs is CgsID[16] == 128 bytes (X360 +32320 -> +32448)");
    static_assert(offsetof(ModeManager, maCarCheckpointData) - offsetof(ModeManager, mauLandmarkSectionIndices) == 32,
                  "mauLandmarkSectionIndices is u16[16] == 32 bytes (X360 +32448 -> +32480)");
    static_assert(offsetof(ModeManager, mauNextLandmark) - offsetof(ModeManager, maCarCheckpointData) == 280,
                  "maCarCheckpointData is CarCheckpointData[35] == 280 bytes (X360 +32480 -> +32760)");

    // ---------------------------------------------------------------------------------------
    // (c) ORDER -- the offsetof monotonicity chain over the whole console-ordered run. Never an
    //     absolute offset: every one of these is a `<`, so it survives the pointer widening and the
    //     sub-object size drift while still catching a re-sort.
    // ---------------------------------------------------------------------------------------
    static_assert(offsetof(ModeManager, mapGameModes)              < offsetof(ModeManager, mRace),                  "order: table before the modes");
    static_assert(offsetof(ModeManager, mRace)                     < offsetof(ModeManager, mOnlineShowtime),        "order: mode block");
    static_assert(offsetof(ModeManager, mOnlineShowtime)           < offsetof(ModeManager, meCurrentGameModeType),  "order: modes before the mode state");
    static_assert(offsetof(ModeManager, meCurrentGameModeType)     < offsetof(ModeManager, mpCurrentGameMode),      "order (console +3476 < +3480)");
    static_assert(offsetof(ModeManager, mpCurrentGameMode)         < offsetof(ModeManager, mePreviousGameModeType), "order (console +3480 < +3484)");
    static_assert(offsetof(ModeManager, mePreviousGameModeType)    < offsetof(ModeManager, mePreviousGameModeState),"order (console +3484 < +3488)");
    static_assert(offsetof(ModeManager, mePreviousGameModeState)   < offsetof(ModeManager, meLastAttemptedGameModeType), "order (console +3488 < +3492)");
    static_assert(offsetof(ModeManager, meLastAttemptedGameModeType) < offsetof(ModeManager, miNumUnsucessfulGameModeAttempts), "order (console +3492 < +3496)");
    static_assert(offsetof(ModeManager, miNumUnsucessfulGameModeAttempts) < offsetof(ModeManager, mScoringSystem),  "order (console +3496 < +3504)");
    static_assert(offsetof(ModeManager, mScoringSystem)            < offsetof(ModeManager, mHUDMessageLogic),       "order (console +3504 < +27392)");
    static_assert(offsetof(ModeManager, mHUDMessageLogic)          < offsetof(ModeManager, mpGameStateModule),      "order (console +27392 < +27992)");
    static_assert(offsetof(ModeManager, mpGameStateModule)         < offsetof(ModeManager, mpProgressionManager),   "order (console +27992 < +27996)");
    static_assert(offsetof(ModeManager, mpProgressionManager)      < offsetof(ModeManager, mpTriggerQueryManager),  "order (console +27996 < +28000)");
    static_assert(offsetof(ModeManager, mpTriggerQueryManager)     < offsetof(ModeManager, mpNetworkRoundManager),  "order (console +28000 < +28004)");
    static_assert(offsetof(ModeManager, mpNetworkRoundManager)     < offsetof(ModeManager, mpGameActionQueue),      "order (console +28004 < +28008)");
    static_assert(offsetof(ModeManager, mpGameActionQueue)         < offsetof(ModeManager, mRandom),                "order (console +28008 < +28016)");
    static_assert(offsetof(ModeManager, mRandom)                   < offsetof(ModeManager, mTimerStatusInterface),  "order (console +28016 < +28064)");
    static_assert(offsetof(ModeManager, mTimerStatusInterface)     < offsetof(ModeManager, mScoringSystemDebugComponent), "order (console +28064 < +28136)");
    static_assert(offsetof(ModeManager, mScoringSystemDebugComponent) < offsetof(ModeManager, maLandmarkIndices),   "order (console +28136 < +32288)");
    static_assert(offsetof(ModeManager, mauNextLandmark)           < offsetof(ModeManager, muNumLandmarks),         "order (console +32760 < +32796)");
    static_assert(offsetof(ModeManager, muNumLandmarks)            < offsetof(ModeManager, mPlayerCurrentLandmark), "order (console +32796 < +32800)");
    static_assert(offsetof(ModeManager, mPlayerCurrentLandmark)    < offsetof(ModeManager, mfModeTimeLimit),        "order (console +32800 < +32804)");
    static_assert(offsetof(ModeManager, mfModeTimeLimit)           < offsetof(ModeManager, mfPlayerTotalledTime),   "order (console +32804 < +32808)");
    static_assert(offsetof(ModeManager, mfPlayerTotalledTime)      < offsetof(ModeManager, mRaceId),                "order (console +32808 < +32816)");
    static_assert(offsetof(ModeManager, mRaceId)                   < offsetof(ModeManager, mePlayerActiveRaceCarIndex), "order (console +32816 < +32824)");
    static_assert(offsetof(ModeManager, mePlayerActiveRaceCarIndex) < offsetof(ModeManager, mePlayerGlobalRaceCarIndex), "order (console +32824 < +32828)");
    static_assert(offsetof(ModeManager, mePlayerGlobalRaceCarIndex) < offsetof(ModeManager, mStartGameModeParams),  "order (console +32828 < +32832)");
    static_assert(offsetof(ModeManager, mStartGameModeParams)      < offsetof(ModeManager, mCurrentGameModeParams), "order (console +32832 < +33664)");
    static_assert(offsetof(ModeManager, mCurrentGameModeParams)    < offsetof(ModeManager, mRaceCarReachedCheckpoint), "order (console +33664 < +35824)");
    static_assert(offsetof(ModeManager, mRaceCarReachedCheckpoint) < offsetof(ModeManager, mRaceCarReachedFinish),  "order (console +35824 < +35832)");
    static_assert(offsetof(ModeManager, mRaceCarReachedFinish)     < offsetof(ModeManager, mPlayersPreSpecialEventCarID), "order (console +35832 < +35840)");
    static_assert(offsetof(ModeManager, mPlayersPreSpecialEventCarID) < offsetof(ModeManager, muNextDistanceRequestCheckpoint), "order (console +35840 < +35848)");
    static_assert(offsetof(ModeManager, muNextDistanceRequestCheckpoint) < offsetof(ModeManager, mPFMActionCache),  "order (console +35848 < +35856)");
    static_assert(offsetof(ModeManager, mPFMActionCache)           < offsetof(ModeManager, mfPFMSecondPhaseTimer),  "order (console +35856 < +38128)");
    static_assert(offsetof(ModeManager, mfPFMSecondPhaseTimer)     < offsetof(ModeManager, mbInModeStartRegion),    "order (console +38128 < +38133)");
    static_assert(offsetof(ModeManager, mbStuntChallengeActive)    < offsetof(ModeManager, miFramesUntilModeSwitchSend), "order (console +38157 < +38160)");
    static_assert(offsetof(ModeManager, miDebugFinishPosition)     < offsetof(ModeManager, mfTimeInFreeBurn),       "order (console +38168 < +38172)");

    // ---------------------------------------------------------------------------------------
    // (d) POINTER-FREE TAIL DELTAS -- host-stable AND console-true. These are the ones the spine's
    //     arithmetic depends on, so they are pinned exactly rather than as an ordering.
    // ---------------------------------------------------------------------------------------

    // THE PFM CACHE. The console memcpy's exactly one PrepareForModeAction into mPFMActionCache and
    // mfPFMSecondPhaseTimer begins immediately after it (+35856 + 2272 == +38128).
    // [!] CLAIM DOWNGRADED 2026-08-26 (wave-B fix round). An earlier banner here called this "the
    // invariant that actually protects the cache". IT IS NOT. For two ADJACENT members of a
    // size-aligned type the delta below is very nearly tautological: it catches a member being
    // INSERTED between the cache and the timer, and nothing else. In particular it does NOT catch
    // the thing that would really break the wire format -- PrepareForModeAction itself changing
    // size -- because both sides of the equation move together. The real protection is the hard
    // `sizeof(PrepareForModeAction) == 2272`, and that one cannot be landed yet (see below).
    // Keep this assert; just do not read more into it than "no member was inserted here".
    // [!] THE CONSOLE'S 2272 IS DELIBERATELY *NOT* ASSERTED: the host type currently measures 1792
    // because its GameModeParams measures 1680 against the console's 2160. That is a real hole in
    // BrnGameModeParams.h (reported as a wave item), not a layout choice here, and a hard
    // `sizeof(...) == 2272` would only fail the gate without fixing it. When GameModeParams is
    // completed, this assert keeps passing and a `== 2272` one can be added beside it.
    static_assert(offsetof(ModeManager, mfPFMSecondPhaseTimer) - offsetof(ModeManager, mPFMActionCache)
                      == sizeof(GameStateModuleIO::PrepareForModeAction),
                  "mPFMActionCache must be exactly ONE PrepareForModeAction wide -- the console "
                  "memcpy's 2272 bytes into it and re-posts them verbatim 0.2 s later");

    static_assert(offsetof(ModeManager, mbIsWaitingForSecondPFM) - offsetof(ModeManager, mfPFMSecondPhaseTimer) == 4,
                  "console +38128 -> +38132");

    // THE 25-BYTE BOOL BLOCK. Console +38133 (0x94F5) .. +38157 (0x950D) inclusive, contiguous. This
    // is THE assert that catches a byte being inserted, dropped or widened inside the block that
    // hazards H4 marks as the wave's highest-collision area -- ModeManager::Construct zeroes 23 of
    // these 25 in one run, which is what fixes the extent.
    static_assert(offsetof(ModeManager, mbStuntChallengeActive) - offsetof(ModeManager, mbInModeStartRegion) == 24,
                  "the bool block is exactly 25 contiguous 1-byte slots (+38133..+38157); if this "
                  "fires, someone widened a flag or inserted/removed one -- re-read hazards H4");
    static_assert(offsetof(ModeManager, mbDistanceToFinishLineTransmitted) - offsetof(ModeManager, mbInModeStartRegion) == 15,
                  "mbDistanceToFinishLineTransmitted is +38148 (the PINNED intro pre-gate)");
    static_assert(offsetof(ModeManager, mbModeIntroStarted) - offsetof(ModeManager, mbInModeStartRegion) == 16,
                  "mbModeIntroStarted is +38149 (the PINNED intro fire-once latch)");
    static_assert(offsetof(ModeManager, mbInstantIntroSplash) - offsetof(ModeManager, mbInModeStartRegion) == 19,
                  "mbInstantIntroSplash is +38152 (IsOnlineModeWithInstantIntro's flag)");
    static_assert(offsetof(ModeManager, mbPlayerCrashedLastFrame) - offsetof(ModeManager, mbInModeStartRegion) == 21,
                  "mbPlayerCrashedLastFrame is +38154 (the road-rage crash arm), NOT +38152");
    static_assert(offsetof(ModeManager, mbIsInTimeUpOutro) - offsetof(ModeManager, mbInModeStartRegion) == 23,
                  "mbIsInTimeUpOutro is +38156 -- the console's own assert string names this byte");

    // THE THREE MODE CLOCKS. GameStateModule reads them at gsm+42300 / 42304 / 42308, i.e. 4 apart.
    static_assert(offsetof(ModeManager, mfTimeInMode)   - offsetof(ModeManager, mfTimeInFreeBurn) == 4,
                  "console +38172 -> +38176 (== gsm+42300 -> +42304)");
    static_assert(offsetof(ModeManager, mfTimeInOnline) - offsetof(ModeManager, mfTimeInMode)     == 4,
                  "console +38176 -> +38180 (== gsm+42304 -> +42308)");

    // The tail scalars.
    static_assert(offsetof(ModeManager, mfTimeUpStateTimer) - offsetof(ModeManager, miFramesUntilModeSwitchSend) == 4,
                  "console +38160 -> +38164");
    static_assert(offsetof(ModeManager, miDebugFinishPosition) - offsetof(ModeManager, mfTimeUpStateTimer) == 4,
                  "console +38164 -> +38168");
    static_assert(offsetof(ModeManager, mfTimeInFreeBurn) - offsetof(ModeManager, miDebugFinishPosition) == 4,
                  "console +38168 -> +38172. This is ALSO the assert that says miDebugFinishPosition "
                  "occupies +38168..+38171 in full -- which is why the retired "
                  "`mbResultsEliminatorValid @ +0x9519` could never have been a real member");

    // The checkpoint-distance-request trio.
    static_assert(offsetof(ModeManager, mbNeedToSendNextRequest) - offsetof(ModeManager, muNextDistanceRequestCheckpoint) == 4,
                  "console +35848 -> +35852");
    static_assert(offsetof(ModeManager, mbIsCalculatingCheckpointDistances) - offsetof(ModeManager, mbNeedToSendNextRequest) == 1,
                  "console +35852 -> +35853 (two adjacent single bytes)");

    // ---------------------------------------------------------------------------------------
    // NOT ASSERTABLE AT COMPILE TIME, RECORDED HERE SO IT IS NOT FORGOTTEN
    // ---------------------------------------------------------------------------------------
    // * THE SLOT ALIAS: mapGameModes[12], [14] and [17] must all be &mOnlineStuntRun, and [6] / [9]
    //   must stay null. Those are address facts written by Construct, not layout facts, so no
    //   static_assert can see them -- the verifier's behavioural oracle (hazards H10) covers them,
    //   and Construct's own comments name every slot.
    // * THE COUNTDOWN TABLE extent (18 f32s from X360 0x82020E98) lives in GetCountdownTimeForMode's
    //   TU (agent 8) and is asserted there.
    // * mauLastLandmarkHit[35] (DWARF :964) is deliberately NOT declared -- it cannot fit between
    //   mauNextLandmark's end (+32795) and muNumLandmarks (+32796). See the FLAG in the header.
}

} // namespace BrnGameState
