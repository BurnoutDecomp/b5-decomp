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
//
// SIZE (X360, authoritative): sizeof == 24. Pinned by the Array<RaceCarCrash,8> instantiation
// (CgsArrayRaceCarCrash8.cpp): every element accessor uses a 24-byte stride (`24 * index`,
// e.g. GetItem @ 0x827BA3B0 `slwi r11,r,1; add r11,r,r11; slwi r11,r11,3` == index*24) and the
// live-count word sits at +0xC0 == 8 * 24, while EraseFast @ 0x827B5410 copies a whole element as
// three 8-byte loads/stores (`ld/std` at +0, +8, +0x10) == 24 bytes. The accessors above only pin
// fields up to +0xC, so the bytes +0x10..+0x17 are an additional opaque tail (interior not
// recovered by this slice; sized only to make the element 24 bytes wide so the array stride is
// exact). ADDITIVE GROW: appended after the existing fields; the +0/+0xC offsets and their
// static_asserts are unchanged.
#include "types.hpp"                                                  // f32, u32
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId

namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnWorld
{
    // BrnCrashModule.cpp -- one tracked in-progress race-car crash.
    struct RaceCarCrash
    {
        // X360 0x827B1538. Returns the active-race-car slot (the embedded 14-bit entity index of
        // mRaceCarVolumeInstanceId), asserting it is < E_ACTIVE_RACE_CAR_INDEX_COUNT (8).
        // Called by BrnWorld::CrashModule::ClearupCrashes.
        s32 GetOwner() const;

        // X360 0x827B14A0. Seat a brand-new crash record over the slot Array<RaceCarCrash,8>::Grow
        // just reserved: assert the volume instance really belongs to a race car, store the id and
        // the initial linger countdown, and zero the two timers and the extension counter.
        // Called by CrashModule::ProcessCrashedRaceCarEvents (the ONLY caller).
        void Construct(CgsSceneManager::VolumeInstanceId lRaceCarVolumeInstanceId, f32 lfSeconds);

        // X360 0x827BF0B8 (152 insns). Advance this wreck by one sim step and decide whether the
        // player's crash is about to end. See the .cpp for the full argument map -- Hex-Rays
        // renders this with 29 positional parameters and it really has eight.
        void Tick(f32 lfTimeStep,
                  const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                      lpActiveRaceCarInterface,
                  bool lbPlayerPressingBoostOutsideShowtime,
                  s32  liMaxCrashExtensions,
                  bool lbIsOfflineGameMode,
                  bool lbIsPlayerCrash,
                  bool lbIsInAGameMode,
                  bool* lpbNeedToSendEndingMessage);

        // X360 0x827B15A0. Sets the linger countdown (the wreck's seconds-before-cleanup),
        // asserting lfSeconds > 0.0f. The Hex-Rays `double a2` is the f32 store (`stfs`) the asm
        // performs. Called by BrnWorld::RaceCarCrash::Tick and CrashModule::HandleGameActions.
        void SetSecondsBeforeCleanup(f32 lfSeconds);

        // ---- read accessors the crash module's own bodies need (2026-08-25, crash exit).
        // HEADER INLINES: the console folds all three into its callers as bare displacements
        // (ClearupCrashes `lfs f, 0xC(item)` @0x827CDFB6, ResetRaceCarFromCrashIndex
        // `ld r30, 0(item)` @0x827C6CB8, TickCrashes `lbz 0x14` -- no out-of-line symbol exists
        // for any of them). Declared so the reconstructed bodies reach the fields BY NAME instead
        // of by offset; no layout, no sizeof, no existing member changed.
        f32 GetSecondsBeforeCleanup() const { return mfSecondsBeforeCleanup; }
        CgsSceneManager::VolumeInstanceId GetVolumeInstanceId() const { return mRaceCarVolumeInstanceId; }

        // Layout pins (X360 accessor store offsets/widths); defined in BrnRaceCarCrash.cpp.
        static void _AssertLayout();

        // True when lrOther is the same crash record (required by the equality-based generic
        // Array<RaceCarCrash,8> members the explicit instantiation forces; the only identity the
        // crash module ever keys on is the crashing race car's volume-instance id at +0). The X360
        // never emitted a standalone RaceCarCrash::operator==, so this compares by the identifying
        // field; it does not affect layout/sizeof (a member function).
        bool operator==(const RaceCarCrash& lrOther) const
        {
            return mRaceCarVolumeInstanceId.muId == lrOther.mRaceCarVolumeInstanceId.muId;
        }

    private:
        // ⭐ [crash exit 2026-08-25] THE INTERIOR IS NO LONGER OPAQUE. The banner above recorded
        // "+0x08 a 4-byte gap ... interior opaque" and "+0x10..+0x17 an additional opaque tail";
        // both were sized-only placeholders because the two committed accessors touch nothing but
        // +0 and +0xC. RaceCarCrash::Construct @0x827B14A0 and RaceCarCrash::Tick @0x827BF0B8
        // write and read EVERY byte of the record, so all four remaining fields are now named
        // from their own stores/loads. Nothing about +0x00 / +0x0C / sizeof 24 changes.
        //
        //   Construct @0x827B14A0:  stfs f31, 0xC(this)   -- mfSecondsBeforeCleanup = lfSeconds
        //                           std  r30, 0(this)     -- mRaceCarVolumeInstanceId
        //                           lfs  f0, flt_82001CC0 (== 0.0f, image-read)
        //                           stfs f0, 8(this) ; stfs f0, 0x10(this)
        //                           stb  r11(0), 0x14(this) ; stb r11(0), 0x15(this)
        //   Tick @0x827BF0B8:       lfs 0xC / stfs 0xC        (the countdown)
        //                           lfs 8   / fadds f31 / stfs 8   (+= dt, unconditional)
        //                           lfs 0x10 / fadds f31 / stfs 0x10 (+= dt) and
        //                           stfs 0.0f, 0x10 when |speedMPH| > 6.5f or |velocity| > 1.5f
        //                           lbz/extsb 0x14 -> compared SIGNED against miNumCrashExtensions,
        //                           then `addi r11,r11,1 ; stb 0x14`   => a signed 8-bit counter
        CgsSceneManager::VolumeInstanceId mRaceCarVolumeInstanceId;   // +0x00 (u64)

        // +0x08. Total time this wreck has been tracked by the crash module. Accumulated every
        // Tick with the sim timestep and never reset; the crash module itself never reads it back
        // (the console publishes it elsewhere), so it is written-only on this path.
        f32 mfTimeCrashing;                                           // +0x08

        // +0x0C. The linger countdown. Ticks down by the sim timestep; ClearupCrashes retires the
        // record once it goes NEGATIVE.
        f32 mfSecondsBeforeCleanup;                                   // +0x0C

        // +0x10. How long the wreck has been essentially STATIONARY. Accumulated with the timestep
        // but slammed back to 0.0f whenever the car is still moving (|mfSpeedMPH| > 6.5f or
        // |linear velocity| > 1.5f). Tick only grants a cleanup extension while this is < 1.0f,
        // i.e. only while the wreck is still visibly sliding.
        f32 mfTimeStationary;                                         // +0x10

        // +0x14. How many 1-second cleanup extensions this crash has already been granted
        // (capped by CrashModule::miNumCrashExtensions). SIGNED: the asm reads it with
        // `lbz ; extsb` before a signed compare.
        s8 mi8NumCleanupExtensions;                                   // +0x14

        // +0x15. Construct zeroes it; nothing on the reconstructed race-car crash path reads it.
        // FLAG (name not recovered) -- kept as an explicitly-named reserved byte rather than
        // folded into padding, because Construct's `stb r11, 0x15(this)` proves it is a member.
        u8 mu8Reserved15;                                             // +0x15
        u8 maPad16[2];                                                // +0x16 -> sizeof 24
    };
}
