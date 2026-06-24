#pragma once

// Canonical (DWARF) home for BrnWorld::RaceCarCrash (World/CrashModule/BrnCrashModule.cpp).
// A RaceCarCrash records one in-progress race-car crash the crash module tracks: which collision
// volume instance the crashing race car is (mRaceCarVolumeInstanceId) and a countdown timer
// controlling how long the wreck lingers before the module cleans it up. Sibling of
// BrnWorld::TrafficCrash (the traffic-vehicle variant) in the same BrnCrashModule.cpp.
//
// Member layout is pinned store-for-store by the two out-of-line accessors the X360 ARTIST build
// emitted (see BrnRaceCarCrash.cpp); field *meanings* come from their asserts (against
// World/CrashModule/BrnCrashModule.cpp:174/194):
//   GetOwner               @ 0x827B1538 -- reads the 64-bit mRaceCarVolumeInstanceId at +0 (`ld`),
//                                          returns its embedded 14-bit entity index, asserting
//                                          "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() <
//                                          E_ACTIVE_RACE_CAR_INDEX_COUNT" (index < 8).
//   SetSecondsBeforeCleanup@ 0x827B15A0 -- stfs the lfSeconds arg into mfSecondsBeforeCleanup at
//                                          +0xC, asserting "lfSeconds > 0.0f".
//
// LAYOUT (X360 accessor store widths/offsets, authoritative):
//   +0x00  CgsSceneManager::VolumeInstanceId mRaceCarVolumeInstanceId  (u64; `ld r,0(this)`)
//   +0x0C  f32                               mfSecondsBeforeCleanup    (`stfs f,0xC(this)`)
// The f32 lands at +0xC, not +0x08, so there is a 4-byte gap at +0x08 (interior opaque -- not
// recovered by this slice; sized only to place the asm-attested mfSecondsBeforeCleanup at +0xC).
#include "types.hpp"                                                  // f32, u32
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId

namespace BrnWorld
{
    // BrnCrashModule.cpp -- one tracked in-progress race-car crash.
    struct RaceCarCrash
    {
        // X360 0x827B1538. Returns the active-race-car slot (the embedded 14-bit entity index of
        // mRaceCarVolumeInstanceId), asserting it is < E_ACTIVE_RACE_CAR_INDEX_COUNT (8).
        // Called by BrnWorld::CrashModule::ClearupCrashes.
        s32 GetOwner() const;

        // X360 0x827B15A0. Sets the linger countdown (the wreck's seconds-before-cleanup),
        // asserting lfSeconds > 0.0f. The Hex-Rays `double a2` is the f32 store (`stfs`) the asm
        // performs. Called by BrnWorld::RaceCarCrash::Tick and CrashModule::HandleGameActions.
        void SetSecondsBeforeCleanup(f32 lfSeconds);

        // Layout pins (X360 accessor store offsets/widths); defined in BrnRaceCarCrash.cpp.
        static void _AssertLayout();

    private:
        CgsSceneManager::VolumeInstanceId mRaceCarVolumeInstanceId;   // +0x00 (u64)
        u8                                maReserved8[4];             // +0x08 (gap, interior opaque)
        f32                               mfSecondsBeforeCleanup;     // +0x0C
    };
}
