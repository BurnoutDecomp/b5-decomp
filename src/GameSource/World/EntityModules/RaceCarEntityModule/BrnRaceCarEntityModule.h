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
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>                                   // offsetof

namespace BrnWorld
{

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

private:
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
}

}
