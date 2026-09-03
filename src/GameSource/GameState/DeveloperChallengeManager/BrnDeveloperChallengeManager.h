#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h
// ============================================================================
// Home for BrnGameState::DeveloperChallengeManager -- the gameplay watcher that listens to
// game events (flat-spins, takedown chains, road-rule scores, stunt offences/combos, billboard
// collects, event wins, takedowns) and, when a developer-challenge condition trips, marks that
// challenge complete on the player's persisted Profile.
//
// SHAPE is X360-asm-authoritative (no DWARF entry for this class). The member layout is recovered
// from BrnGameState::DeveloperChallengeManager::Construct (X360 0x82372E90) and the per-event
// handlers:
//
//   +0x00  Array<CollectedBillboard,5> maCollectedBillboards   (maElements[5] @+0x00, miCount @+0x50)
//   +0x58  FifoQueue<float,10>         maRoadRageTakedownTimes  (maData[10]@+0x58, read@+0x80,
//                                                                write@+0x84, length@+0x88)
//   +0x90  CgsContainers::FastBitArray<8>  maTakenDownRaceCars   (single u64 field; zeroed in Construct via
//                                                                  `std 0x90`. OnTakedown @0x8238E208's
//                                                                  mode-0 arm SetBit()s the victim's active
//                                                                  index into it (`r22 = this+0x90`, the
//                                                                  inlined FastBitArray<8>::SetBit with its
//                                                                  CgsFastBitArray.h:431 range assert) and
//                                                                  then counts its set bits against
//                                                                  active cars - 1; OnEventEnd @0x82396968
//                                                                  `std 0, 0x90` clears it per event.)
//   +0x98  CgsContainers::FastBitArray<15> maCompletedChallenges (single u64 field; SetChallengeCompleted
//                                                                  sets bit `liChallengeIndex`; zeroed in
//                                                                  Construct via `std 0x98`. It is the
//                                                                  PENDING-PUBLISH set: PreWorldUpdate
//                                                                  @0x8238D7A0..0x8238D840 posts the raw
//                                                                  u64 as action 298 when it is non-zero
//                                                                  and then `std 0, 0x98` clears it.)
//   [takedown P1 wave 2026-09-03] +0x90/+0x98 were swapped in the earlier recon (+0x90 read as a
//   "pending road-rage payload" and +0x98 as a persisted set); the three writers/readers above
//   (OnTakedown / OnEventEnd / PreWorldUpdate) pin the identities.
//   +0xA0  f32                         mlfCurrentRaceTime        (PreWorldUpdate `stfs f0,0xA0`)
//   +0xA4  ProgressionManager*         mpProgressionManager      (Construct `stw r29,0xA4`)
//   +0xA8  StreetManager*              mpStreetManager           (Construct `stw r28,0xA8`)
//   +0xAC  ScoringSystem*              mpScoringSystem           (Construct `stw r27,0xAC`)
//   +0xB0  GameStateModule*            mpGameStateModule         (Construct `stw r26,0xB0`)
//
// NOTE the nested CollectedBillboard element type was previously a provisional stub in
// BrnGameStateLeafContainers.h (the Array<CollectedBillboard,5> explicit-instantiation TU's
// element home). This header is the manager's REAL home and now owns that nested type; the
// leaf-containers stub class has been removed and the Array<CollectedBillboard,5> instantiation TU
// includes THIS header instead (mirroring the StuntModeScoringOnline promotion precedent in
// BrnGameStateLeafContainers.h). The 16-byte X360 stride is preserved.
//
// FLAG: the developer-challenge THRESHOLD constants (min flat-spin angle, min takedown-chain count,
// road-rule score gates, billboard/road-rage time windows, etc.) are loaded from rodata (the X360
// `dword_82CDB99x` / `flt_82CDB98x` config words at 0x82CDB97C..0x82CDB9A8); their literal VALUES
// are NOT present as immediates in the asm and are NOT recoverable from this view. They are modelled
// as flagged placeholder externs (see the KF_/KI_ constants below) -- NEVER fabricated. Ground them
// from the rodata table when it is recovered; do NOT trust the placeholder values for behaviour.

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID (== u64)
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"              // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsFifoQueue.h"          // FifoQueue<T,N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"       // CgsContainers::FastBitArray<N>

// Pointer-only members + by-pointer params -> forward declarations (real homes #included by the .cpp).
namespace BrnProgression { class ProgressionManager; }
namespace BrnGameState   { class StreetManager; }
namespace BrnGameState   { class ScoringSystem; }
namespace BrnGameState   { class GameStateModule; }
namespace BrnGameState { namespace GameStateModuleIO { struct PreWorldInputBuffer; } }
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; } }

namespace BrnGameState
{

// The developer-challenge identifiers. Each handler trips a specific challenge; the enumerator
// VALUES are the literal challenge-index immediates the X360 passes to SetChallengeCompleted
// (li r4, <n>) -- recovered directly from the asm, NOT fabricated. The count (15) is the
// SetChallengeCompleted range assert (`a2 >= 15`) and the FastBitArray<15> capacity.
enum EDeveloperChallenge : s32
{
    E_DEV_CHALLENGE_FLAT_SPIN              = 0,   // OnFlatSpin            (li r4,0)
    E_DEV_CHALLENGE_TAKEDOWN_CHAIN         = 1,   // OnTakedownChain       (li r4,1)
    E_DEV_CHALLENGE_TAKEDOWN_CHAIN_CAR     = 2,   // OnTakedownChain (car) (li r4,2)
    E_DEV_CHALLENGE_EVENT_WIN              = 3,   // OnEventWin            (li r4,3)
    E_DEV_CHALLENGE_EVENT_WIN_FLAWLESS     = 4,   // OnEventWin            (li r4,4)
    E_DEV_CHALLENGE_ROAD_RAGE_BURST        = 5,   // OnTakedown            (li r4,5)
    E_DEV_CHALLENGE_BILLBOARD_BURST        = 6,   // OnCollectStunt        (li r4,6)
    E_DEV_CHALLENGE_ROAD_RULE_SET          = 7,   // OnSetRoadRule         (li r4,7)
    E_DEV_CHALLENGE_STUNT_SCORE            = 8,   // OnEventWin            (li r4,8)
    E_DEV_CHALLENGE_STUNT_SCORE_CAR        = 9,   // OnEventWin            (li r4,9)
    E_DEV_CHALLENGE_ROAD_RULE_TIME_CAR     = 10,  // OnSetRoadRule         (li r4,10)
    E_DEV_CHALLENGE_STUNT_OFFENCE          = 11,  // OnStuntOffenceComplete(li r4,11)
    E_DEV_CHALLENGE_ALL_TAKEDOWNS          = 12,  // OnTakedown            (li r4,12)
    E_DEV_CHALLENGE_STUNT_COMBO            = 13,  // OnStuntRunComboPerformed (li r4,13)
    E_DEV_CHALLENGE_EVENT_WIN_STUNT        = 14,  // OnEventWin            (li r4,14)

    E_DEV_CHALLENGE_COUNT                  = 15,  // SetChallengeCompleted range assert (a2 >= 15)
};

// ----------------------------------------------------------------------------
// FLAG: PLACEHOLDER threshold config constants (rodata @ 0x82CDB97C..0x82CDB9A8).
// The literal values are NOT in the asm (loaded from .data). Declared extern here; defined as
// flagged placeholders in the .cpp. Ground from rodata when recovered. NEVER trust these values.
// ----------------------------------------------------------------------------
extern const f32 KF_DEV_CHALLENGE_FLAT_SPIN_MIN_ANGLE;        // flt_82CDB97C (OnFlatSpin gate)
extern const f32 KF_DEV_CHALLENGE_ROAD_RAGE_WINDOW;           // flt_82CDB980 (UpdateBufferedRoadRageTakedowns window)
extern const f32 KF_DEV_CHALLENGE_BILLBOARD_WINDOW;           // flt_82CDB984 (UpdateBufferedCollectedBillboards window)
extern const f32 KF_DEV_CHALLENGE_STUNT_OFFENCE_MIN_SCORE;    // flt_82CDB988 (OnStuntOffenceComplete gate)
extern const s32 KI_DEV_CHALLENGE_ROAD_RAGE_BURST_COUNT;      // dword_82CDB98C (OnTakedown road-rage burst count)
extern const s32 KI_DEV_CHALLENGE_BILLBOARD_BURST_COUNT;      // dword_82CDB990 (OnCollectStunt billboard burst count)
extern const s32 KI_DEV_CHALLENGE_TAKEDOWN_CHAIN_MIN;         // dword_82CDB994 (OnTakedownChain gate)
extern const s32 KI_DEV_CHALLENGE_ROAD_RULE_SCORE_MIN;        // dword_82CDB998 (OnSetRoadRule gate)
extern const s32 KI_DEV_CHALLENGE_STUNT_SCORE_MIN;            // dword_82CDB99C (OnEventWin stunt-score gate)
extern const s32 KI_DEV_CHALLENGE_STUNT_SCORE_CAR_MIN;        // dword_82CDB9A0 (OnEventWin stunt-score-car gate)
extern const s32 KI_DEV_CHALLENGE_ROAD_RULE_TIME_MIN;         // dword_82CDB9A4 (OnEventWin / road-rule gate)
extern const f32 KF_DEV_CHALLENGE_STUNT_COMBO_MIN;            // flt_82CDB9A8 (OnStuntRunComboPerformed gate)

// ----------------------------------------------------------------------------
// BrnGameState::DeveloperChallengeManager
// ----------------------------------------------------------------------------
class DeveloperChallengeManager
{
public:
    // Nested element type of maCollectedBillboards (the manager owns it here; the leaf-containers
    // provisional stub was removed). FLAG: 16-byte X360 stride is proven; the field names are
    // provisional -- the burst-window check reads a collect time and the buffered billboard id.
    struct CollectedBillboard
    {
        u8 maBlob[16];   // X360 stride 16 (provisional fields)
    };

    // X360 0x82372E90. Cache the four owning subsystems (asserting each non-null) and bring the
    // tracker to its empty state: the billboard array, the road-rage takedown ring buffer, the
    // pending-event word and the completed-challenge bit set are all cleared to zero.
    void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                   StreetManager*                      lpStreetManager,
                   ScoringSystem*                      lpScoringSystem,
                   GameStateModule*                    lpGameStateModule);

    // X360 0x8238D698. Per-frame pre-world hook: refresh the current race time, publish + clear any
    // pending buffered road-rage takedown event onto the output queue, then age out the buffered
    // road-rage takedowns + collected billboards past their time windows.
    void PreWorldUpdate(GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                        GameStateModuleIO::OutputBuffer*         lpOutput);

    // X360 0x82373258. A flat-spin completed with magnitude lfFlatSpinAngle (radians/degrees per
    // the gameplay units). Trips E_DEV_CHALLENGE_FLAT_SPIN when the angle clears the min threshold
    // and the challenge is not already complete.
    void OnFlatSpin(f32 lfFlatSpinAngle);

    // X360 0x8238E020. A takedown chain of length liChainLength completed. Trips the takedown-chain
    // challenge (and the car-specific variant when the player car id matches "PUSRI01").
    void OnTakedownChain(s32 liChainLength);

    // X360 0x8238E208. A single takedown occurred. liGameModeType is the CURRENT GAME MODE TYPE
    // (GameStateModule::ProcessTakedownEvents @0x8238FEBC passes `lwz r4, 0x1DB4(r29)` ==
    // meCurrentGameModeType, and r5 = the event's victim index): 0 == E_MODE_OFFLINE_RACE runs the
    // "took every rival down once" sweep over maTakenDownRaceCars, 3 == E_MODE_ROAD_RAGE buffers this
    // takedown's time for the burst check. Any other mode is a no-op. (s32, not the enum, so this
    // header pulls no GameStateModuleIO dependency; the caller passes the enum's value.)
    void OnTakedown(s32 liGameModeType, u32 luVictimActiveRaceCarIndex);

    // X360 0x8238D860. A road-rule score event. liRoadRuleType: 0 == time-based (car-specific
    // "PUSMC01" challenge when fast enough), 1 == score-based (E_DEV_CHALLENGE_ROAD_RULE_SET).
    void OnSetRoadRule(s32 liRoadRuleType, s32 liScore, s32 liCarId);

    // X360 0x82373338. A stunt offence (e.g. a near-miss / boost chain) completed. Trips
    // E_DEV_CHALLENGE_STUNT_OFFENCE when its score clears the min threshold.
    void OnStuntOffenceComplete(const u8* lpStuntOffence);

    // X360 0x82373418. A stunt-run combo was performed. Trips E_DEV_CHALLENGE_STUNT_COMBO when the
    // combo score clears the min threshold.
    void OnStuntRunComboPerformed(const f32* lpComboScore);

    // X360 0x82373028. A stunt element (billboard) was collected. luStuntType == 2 buffers the
    // collect for the burst-window check (and trips E_DEV_CHALLENGE_BILLBOARD_BURST when the buffer
    // tops the burst count). Other types are no-ops / asserts.
    void OnCollectStunt(u32 luStuntType, CgsID lBillboardId);

    // X360 0x82396940. End-of-event hook. lbWon == the event was won -> forwards to OnEventWin.
    void OnEventEnd(s32 liGameModeType, bool lbWon);

    // X360 0x8238DB10. The player won the event of type liGameModeType. Evaluates the win-driven
    // developer challenges (flawless race win, stunt-score wins, etc.) against the player's score.
    void OnEventWin(s32 liGameModeType);

    // X360 0x8237F770. True iff lCarId, following its parent-car chain, reaches lTargetCarId. Used by
    // the car-specific challenges to test the player car against a special-event car id.
    bool CheckCarID(CgsID lCarId, CgsID lTargetCarId);

private:
    // X360 0x823674B8. Mark developer-challenge liChallengeIndex complete: set it on the persisted
    // Profile and record it locally in maCompletedChallenges. Asserts the index in [0, 15).
    void SetChallengeCompleted(s32 liChallengeIndex);

    // X360 0x82367428. Drop buffered road-rage takedowns whose time has aged past the window.
    void UpdateBufferedRoadRageTakedowns();

    // X360 0x82372F80. Drop buffered collected billboards whose time has aged past the window.
    void UpdateBufferedCollectedBillboards();

    // ---- layout (X360-asm-recovered; see file header) ----
    Array<CollectedBillboard, 5>          maCollectedBillboards;     // +0x00 (count @ +0x50)
    FifoQueue<f32, 10>                    maRoadRageTakedownTimes;   // +0x58 (read@+0x80/write@+0x84/len@+0x88)
    CgsContainers::FastBitArray<8>        maTakenDownRaceCars;       // +0x90 (one u64 field: victims this event)
    CgsContainers::FastBitArray<15>       maCompletedChallenges;     // +0x98 (one u64 field: pending publish)
    f32                                   mlfCurrentRaceTime;        // +0xA0
    BrnProgression::ProgressionManager*   mpProgressionManager;      // +0xA4
    StreetManager*                        mpStreetManager;           // +0xA8
    ScoringSystem*                        mpScoringSystem;           // +0xAC
    GameStateModule*                      mpGameStateModule;         // +0xB0
};
}
