#pragma once

// ============================================================================
// BrnWorld::RaceCarEntityModule -- the race-car entity module.
//
// This MINIMAL owning header carries only the surface needed to compile this
// TU's two ledgered accessor bodies:
//   RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex)  X360 0x822A34A8
//   RaceCarEntityModule::GetGlobalRaceCar(EGlobalRaceCarIndex)  X360 0x822A3568
// Both are simple in-range-checked &array[index] accessors. The full
// RaceCarEntityModule class (Feb-2007 leak BrnRaceCarEntityModule.h, ~50 module
// dependencies: ModuleSingleBuffered base, the streamer/boost/near-miss/crash-play
// managers, WorldMap2D, replay serialiser, etc.) is far larger and is NOT
// reconstructed here -- only the layout slice the accessors touch.
//
// Member access is BY NAME at the X360-asm-proven byte offsets; the two array
// offsets are locked with static_assert(offsetof(...)) in the .cpp:
//   maRaceCars        +0x250  (== 592)   stride 0xB0   (176B RaceCar,      35 wide)
//   maActiveRaceCars  +0x1A60 (== 6752)  stride 0x1CD0 (7376B ActiveRaceCar, 8 wide)
// The global array runs 0x250 .. 0x250+35*176 = 0x1A60, i.e. the active array
// starts immediately after it (no gap). X360 asm:
//   GetGlobalRaceCar:  return 176 * a2 + this + 592;   // &maRaceCars[a2]
//   GetActiveRaceCar:  return 7376 * a2 + this + 6752; // &maActiveRaceCars[a2]
// ============================================================================

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"             // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h" // BrnGameState::GameStateModuleIO::EPlayerScoringIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h" // BrnProgression::ETrainingType

#include <cstddef>                                   // offsetof

namespace BrnWorld
{

// X360-attested pending-training-request ring depth (DWARF BrnRaceCarEntityModule.h:66).
// AddTrainingRequest asserts miPendingRequestCount < this before appending.
const s32 KI_TRAINING_REQUEST_QUEUE_SIZE = 8;

// The active-race-car output interface (real home:
// SharedIO/BrnRaceCarEntityModuleOutputInterface.h). CopyActiveRaceCarToPlayerScoringMappingToOutput
// only takes a pointer to it, so a forward declaration suffices here.
namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; }

// ---- PLACEHOLDER element types ---------------------------------------------
// The real RaceCar / ActiveRaceCar live in their own (not-yet-committed) homes
// under .../RaceCarEntityModule/BrnRaceCar.h and BrnActiveRaceCar.h. These two
// accessors only ever return the ADDRESS of an element, so for this TU only the
// element SIZE is load-bearing (it sets the array stride the asm proves). The
// byte sizes below are the X360-attested strides (0xB0 / 0x1CD0); the internal
// members are intentionally opaque and FLAGGED -- do NOT treat these as the real
// class layouts. When the real types are committed, replace these stand-ins and
// drop the size static_asserts in the .cpp.
class RaceCar
{
public:
    u8 maPlaceholderBytes[0xB0];   // FLAG: opaque; size only (X360 stride 0xB0 == 176)
};

class ActiveRaceCar
{
public:
    u8 maPlaceholderBytes[0x1CD0]; // FLAG: opaque; size only (X360 stride 0x1CD0 == 7376)
};

class RaceCarEntityModule
{
public:
    // X360 0x822A34A8 -- &maActiveRaceCars[leActiveRaceCarIndex], in-range checked.
    inline ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3568 -- &maRaceCars[leGlobalRaceCarIndex], in-range checked.
    inline RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // ------------------------------------------------------------------------
    // Active-race-car <-> player-scoring-slot mapping (online scoring). The module
    // keeps maActiveRaceCarForPlayerScoringIndex[player] == the active-race-car slot
    // that the given player is scoring as, or E_ACTIVE_RACE_CAR_INDEX_COUNT (8) as the
    // "no mapping" sentinel (that is the value the X360 stores -- `li r10,8` / `li r27,8`).
    // ------------------------------------------------------------------------

    // X360 0x822A3760 -- set every player slot to the "no mapping" sentinel (8).
    void ClearAllActiveRaceCarToPlayerScoringMappings();

    // X360 0x822A37C8 -- find the player slot currently mapped to leActiveRaceCarIndex
    // and reset it to the sentinel (8). If no slot maps to it, do nothing.
    void ClearActiveRaceCarToPlayerScoringMapping(EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3888 -- map player scoring slot lePlayerScoringIndex to
    // leActiveRaceCarIndex.
    void SetActiveRaceCarForPlayerScoringIndex(
        BrnGameState::GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex,
        EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822A3918 -- copy the whole player-scoring mapping into the active-race-car
    // output interface (one SetActiveRaceCarIndex per slot).
    void CopyActiveRaceCarToPlayerScoringMappingToOutput(
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpOutputInterface);

    // X360 0x822A3A20 -- (mxGameModeFlags & lxFlagMask) != 0.
    bool GetGameModeFlag(u64 lxFlagMask) const;

    // ------------------------------------------------------------------------
    // Tail-state bookkeeping bodied in BrnRaceCarEntityModule.cpp. These touch only
    // the module's own scalar tail at attested offsets (plus, for UpdateTailgateTimer,
    // a forward-declared sibling query), so they reconstruct BY NAME without modelling
    // the un-homed RaceCar/ActiveRaceCar/manager interiors.
    // ------------------------------------------------------------------------

    // X360 0x822A47A8 -- append leTrainingType to mePendingTrainingRequestQueue (range +
    // capacity asserted), bumping miPendingRequestCount. No-op if the ring is full.
    void AddTrainingRequest(BrnProgression::ETrainingType leTrainingType);

    // X360 0x822CE508 -- if the player car is tailgating any other race car, accumulate
    // lfDeltaTime into mfCurrentTailgateDuration; otherwise reset it to 0. Returns the
    // tailgating predicate. (Calls IsPlayerCarTailgatingOtherRaceCars, declared below.)
    bool UpdateTailgateTimer(f32 lfDeltaTime);

private:
    // FLAG: declaration-only sibling this TU references but does not body here -- it
    // reaches the un-homed ActiveRaceCar interior + a tailgating cone test. Declared so
    // UpdateTailgateTimer links; its body belongs to a later race-car-interior pass.
    bool IsPlayerCarTailgatingOtherRaceCars(
        EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
        const ActiveRaceCar* lpPlayerActiveRaceCar);

    // Compiled-never-called offsetof layout lock (see definition below).
    void LockLayout_();

    // FLAG: opaque leading state. In the full class this span is the
    // ModuleSingleBuffered base plus the early stage/handle/region members; here
    // it exists only to land maRaceCars at the X360-proven +0x250 offset.
    u8 maPrecedingState[0x250];

    // Global race-car slots (player + up to 34 rivals/traffic). +0x250, stride 0xB0.
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];

    // Active race-car slots (local player + rivals). +0x1A60, stride 0x1CD0.
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    // (maActiveRaceCars ends at +0x1A60 + 8*0x1CD0 == +0x100E0 == 65760)

    // FLAG: opaque mid-object state. The full class carries the streamer/boost/near-miss/
    // crash-play managers, timers, RNGs, etc. between the active-car array and the
    // game-mode/scoring tail below; here it is honest padding that lands the named
    // members at their X360-asm-proven byte offsets. The bodied tail functions
    // (scoring map, GetGameModeFlag, AddTrainingRequest, UpdateTailgateTimer) are the
    // only ones in this TU that touch the tail.
    u8 maTailPadA0[0x182F0 - 0x100E0];  // +0x100E0 (65760) .. +0x182F0 (99056)

    // X360 +0x182F0 (99056). Seconds the player has been continuously tailgating another
    // race car; UpdateTailgateTimer accumulates dt into it while tailgating, else zeroes
    // it. DWARF BrnRaceCarEntityModule.h:357 -> float32_t.
    f32 mfCurrentTailgateDuration;      // +0x182F0 (99056) .. +0x182F4 (99060)

    u8 maTailPadA1a[0x182F8 - 0x182F4]; // +0x182F4 (99060) .. +0x182F8 (99064)

    // X360 +0x182F8 (99064). The active-race-car slot the local player is driving, or
    // E_ACTIVE_RACE_CAR_INDEX_INVALID. UpdateTailgateTimer reads it (asm `lwzx` at 0x182F8)
    // to pick the player car. DWARF BrnRaceCarEntityModule.h:360 -> EActiveRaceCarIndex.
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;
                                        // +0x182F8 (99064) .. +0x182FC (99068)

    u8 maTailPadA1b[0x18358 - 0x182FC]; // +0x182FC (99068) .. +0x18358 (99160)

    // X360 +0x18358 (99160). GetGameModeFlag reads it with a 64-bit load (`ldx`) and ANDs
    // it with the caller's mask. DWARF BrnRaceCarEntityModule.h:388 -> uint64_t.
    u64 mxGameModeFlags;                // +0x18358 (99160) .. +0x18360 (99168)

    u8 maTailPadB0[0x18374 - 0x18360];  // +0x18360 (99168) .. +0x18374 (99188)

    // X360 +0x18374 (99188). Ring of training requests queued this frame, drained by the
    // progression handler. AddTrainingRequest appends at miPendingRequestCount; each cell
    // is a 4-byte ETrainingType (the asm `stwx` stores the 32-bit enum). DWARF
    // BrnRaceCarEntityModule.h:401 -> BrnProgression::ETrainingType[8].
    BrnProgression::ETrainingType mePendingTrainingRequestQueue[KI_TRAINING_REQUEST_QUEUE_SIZE];
                                        // +0x18374 (99188) .. +0x18394 (99220)

    // X360 +0x18394 (99220). Number of valid entries in mePendingTrainingRequestQueue.
    // DWARF BrnRaceCarEntityModule.h:402 -> int32_t.
    s32 miPendingRequestCount;          // +0x18394 (99220) .. +0x18398 (99224)

    u8 maTailPadB1[0x187BC - 0x18398];  // +0x18398 (99224) .. +0x187BC (100284)

    // X360 +0x187BC (100284). Player-scoring-slot -> active-race-car-slot map. The X360
    // DWORD index is 0x61EF (25071). Indexed by EPlayerScoringIndex (0..7); each cell is
    // E_ACTIVE_RACE_CAR_INDEX_COUNT (8) when no active car is mapped to that player.
    EActiveRaceCarIndex maActiveRaceCarForPlayerScoringIndex
        [BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT];
};

// X360 0x822A34A8. Asserts the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT) (the asm only emits the upper-bound branch since
// the lower bound on a non-negative enum is trivially true), then returns the
// element address: this + 6752 + 7376*index == &maActiveRaceCars[index].
inline ActiveRaceCar*
RaceCarEntityModule::GetActiveRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return &maActiveRaceCars[leActiveRaceCarIndex];
}

// X360 0x822A3568. Asserts the index is in [E_GLOBAL_RACE_CAR_INDEX_0,
// E_GLOBAL_RACE_CAR_INDEX_COUNT), then returns the element address:
// this + 592 + 176*index == &maRaceCars[index].
inline RaceCar*
RaceCarEntityModule::GetGlobalRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
               "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    return &maRaceCars[leGlobalRaceCarIndex];
}

// Layout lock. offsetof on the private array members needs member-scope access
// under MSVC, so the X360-proven offsets are asserted here (compiled, never
// called). maRaceCars @+0x250 and maActiveRaceCars @+0x1A60 are the offsets the
// accessor asm bakes in (this + 592 / this + 6752).
inline void RaceCarEntityModule::LockLayout_()
{
    static_assert(offsetof(RaceCarEntityModule, maRaceCars) == 0x250,
                  "maRaceCars @+0x250 (== 592)");
    static_assert(offsetof(RaceCarEntityModule, maActiveRaceCars) == 0x1A60,
                  "maActiveRaceCars @+0x1A60 (== 6752)");
    static_assert(offsetof(RaceCarEntityModule, mxGameModeFlags) == 0x18358,
                  "mxGameModeFlags @+0x18358 (== 99160)");
    static_assert(offsetof(RaceCarEntityModule, maActiveRaceCarForPlayerScoringIndex) == 0x187BC,
                  "maActiveRaceCarForPlayerScoringIndex @+0x187BC (== 100284)");
    static_assert(offsetof(RaceCarEntityModule, mfCurrentTailgateDuration) == 0x182F0,
                  "mfCurrentTailgateDuration @+0x182F0 (== 99056)");
    static_assert(offsetof(RaceCarEntityModule, mePendingTrainingRequestQueue) == 0x18374,
                  "mePendingTrainingRequestQueue @+0x18374 (== 99188)");
    static_assert(offsetof(RaceCarEntityModule, miPendingRequestCount) == 0x18394,
                  "miPendingRequestCount @+0x18394 (== 99220)");
    static_assert(offsetof(RaceCarEntityModule, mePlayerActiveRaceCarIndex) == 0x182F8,
                  "mePlayerActiveRaceCarIndex @+0x182F8 (== 99064)");
}

}
