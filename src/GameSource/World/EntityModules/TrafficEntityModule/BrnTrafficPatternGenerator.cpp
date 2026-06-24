// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPatternGenerator.cpp
//
// BrnTraffic::PatternGenerator::UpdateHeadlightFlash (X360 0x82751C58).
//
// A stateless stepper over a per-pattern headlight-flash timing table (the X360 rodata
// table flt_820C0FB8, modelled here as KAF_HEADLIGHTFLASH_TIMES[3][9]). The caller passes
// the running accumulated-time and state index by pointer; this advances them by one frame
// and reports whether the headlight is currently off and whether the pattern is still live.
//
// Reconstructed store-for-store from the X360 asm (0x82751C58). The X360 inlines the
// per-argument debug guards (Begin/Fire/EndAssert triples); they are reproduced as house
// CGS_ASSERTs. The baked assert file/line are discarded per project convention.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPatternGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{

// DWARF BrnTrafficPatternGenerator.cpp:26 -- the per-pattern state count (table width).
static const u32 KU_MAX_STATES_IN_PATTERN = 9;

// X360 rodata flt_820C0FB8 (DWARF BrnTrafficPatternGenerator.cpp:28): per-pattern on/off
// dwell durations in seconds. A non-positive entry terminates the pattern walk. The state
// index alternates the headlight off/on (even index -> off), so each row encodes the
// blink rhythm for one EHeadlightFlash pattern.
static const f32 KAF_HEADLIGHTFLASH_TIMES[PatternGenerator::E_HEADLIGHTFLASH_COUNT][KU_MAX_STATES_IN_PATTERN] =
{
    { 0.05f,  0.3f,  0.1f,  0.2f,  0.04f, 0.25f, 0.05f, -1.0f, -1.0f },
    { 0.075f, 0.2f,  0.1f,  0.2f,  0.08f, 0.15f, 0.09f,  0.2f,  0.07f },
    { 0.06f,  0.75f, 0.08f, 0.25f, 0.08f, 0.25f, 0.08f, -1.0f, -1.0f },
};

// @ 0x82751C58
bool PatternGenerator::UpdateHeadlightFlash(u32 luPattern, f32 lfTimeDelta, f32* lpfUpdatedTime,
                                            u8* lpuUpdatedState, bool* lpbUpdatedHeadlightOff)
{
    // luPattern >= 0 is vacuous for an unsigned; the X360 emits only the upper-bound guard.
    CGS_ASSERT(luPattern < PatternGenerator::E_HEADLIGHTFLASH_COUNT, "luPattern < E_HEADLIGHTFLASH_COUNT");
    CGS_ASSERT(lfTimeDelta > 0.0f, "lfTimeDelta > 0.0f");
    CGS_ASSERT(lpfUpdatedTime != 0, "lpfUpdatedTime");
    CGS_ASSERT(*lpfUpdatedTime >= 0.0f, "*lpfUpdatedTime >= 0.0f");
    CGS_ASSERT(lpuUpdatedState != 0, "lpuUpdatedState");
    CGS_ASSERT(*lpuUpdatedState <= KU_MAX_STATES_IN_PATTERN, "*lpuUpdatedState <= KU_MAX_STATES_IN_PATTERN");
    CGS_ASSERT(lpbUpdatedHeadlightOff != 0, "lpbUpdatedHeadlightOff");

    u32 luState = *lpuUpdatedState;
    f32 lfTime  = *lpfUpdatedTime;

    f32 lfTimeForState = KAF_HEADLIGHTFLASH_TIMES[luPattern][luState];
    CGS_ASSERT(lfTimeForState != 0.0f, "lfTimeForState != 0.0f");

    if (lfTimeForState > 0.0f && luState < KU_MAX_STATES_IN_PATTERN)
    {
        lfTime += lfTimeDelta;
        if (lfTime >= lfTimeForState)
        {
            lfTime -= lfTimeForState;
            ++luState;
            lfTimeForState = KAF_HEADLIGHTFLASH_TIMES[luPattern][luState];
            CGS_ASSERT(lfTimeForState != 0.0f, "lfTimeForState != 0.0f");
        }
    }

    *lpuUpdatedState = static_cast<u8>(luState);
    *lpfUpdatedTime  = lfTime;

    if (lfTimeForState > 0.0f && luState < KU_MAX_STATES_IN_PATTERN)
    {
        *lpbUpdatedHeadlightOff = ((luState & 1) == 0);
        return true;
    }

    *lpbUpdatedHeadlightOff = false;
    return false;
}

} // namespace BrnTraffic
