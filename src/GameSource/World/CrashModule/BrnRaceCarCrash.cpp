#include "GameSource/World/CrashModule/BrnRaceCarCrash.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::RaceCarCrash accessors, reconstructed from BURNOUT_X360_ARTIST.XEX
// (semantic parity, not byte match). Both live in World/CrashModule/BrnCrashModule.cpp.

namespace BrnWorld
{
    // GetOwner @ 0x827B1538
    //   ld    r11, 0(this)        ; read the 64-bit mRaceCarVolumeInstanceId
    //   srdi  r11, r11, 32        ; take the HIGH dword (embedded entity word)
    //   extrwi r11, r11, 14, 8    ; extract the 14-bit entity index (bit 8 big-endian == [10..23])
    //   cmplwi r11, 8 / blt ...   ; assert index < E_ACTIVE_RACE_CAR_INDEX_COUNT (8), non-gating
    //   ...re-read + re-extract -> r3 (returns the index even when the assert tripped)
    // (BrnCrashModule.cpp:174). The 14-bit extraction is exactly
    // VolumeInstanceId::GetEntityIDEntityIndex() (the canonical accessor the assert message names);
    // reuse it instead of open-coding the shift/mask.
    s32 RaceCarCrash::GetOwner() const
    {
        const u32 luIndex = mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
        CGS_ASSERT(luIndex < 8u,
                   "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        return static_cast<s32>(luIndex);
    }

    // SetSecondsBeforeCleanup @ 0x827B15A0
    //   fmr   f31, f1             ; lfSeconds
    //   lfs   f0, flt_82001CC0    ; 0.0f
    //   fcmpu f31, f0 / bgt ...   ; assert lfSeconds > 0.0f (non-gating)  (BrnCrashModule.cpp:194)
    //   stfs  f31, 0xC(this)      ; mfSecondsBeforeCleanup = lfSeconds  (a 32-bit float store)
    // The Hex-Rays `double a2 ... *(this+12) = a2` is the f32 store (`stfs`, not `stfd`) the asm
    // performs -- the value is held/compared/stored as a single-precision float.
    void RaceCarCrash::SetSecondsBeforeCleanup(f32 lfSeconds)
    {
        CGS_ASSERT(lfSeconds > 0.0f, "lfSeconds > 0.0f");
        mfSecondsBeforeCleanup = lfSeconds;   // stfs @0xC
    }

    // Layout pins (X360 accessor store offsets/widths).
    void RaceCarCrash::_AssertLayout()
    {
        static_assert(offsetof(RaceCarCrash, mRaceCarVolumeInstanceId) == 0,    "mRaceCarVolumeInstanceId @0");
        static_assert(offsetof(RaceCarCrash, mfSecondsBeforeCleanup)   == 0xC,  "mfSecondsBeforeCleanup @0xC");
    }
}
