#pragma once

// ===================================================================================
// BrnTraffic::PatternGenerator -- owning header
//   b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPatternGenerator.h
//
// Drives the traffic-vehicle headlight-flash blink patterns. PatternGenerator carries no
// per-instance state -- UpdateHeadlightFlash is a pure stateless stepper over a constant
// per-pattern timing table; the caller (BrnTraffic::Vehicle) owns the running timer/state.
//
// Layout/method set from the DecFIGS DWARF (BrnTrafficPatternGenerator.h:43/:63).
// ===================================================================================

#include "types.hpp"

namespace BrnTraffic
{
    struct PatternGenerator
    {
        // The set of headlight-flash patterns. COUNT == 3 (the three rows of the timing
        // table the X360 indexes at flt_820C0FB8); DWARF references E_HEADLIGHTFLASH_COUNT
        // as the upper bound of luPattern.
        enum EHeadlightFlash
        {
            E_HEADLIGHTFLASH_NORMAL = 0,
            E_HEADLIGHTFLASH_FAST   = 1,
            E_HEADLIGHTFLASH_SLOW   = 2,
            E_HEADLIGHTFLASH_COUNT  = 3,
        };

        // @ 0x82751C58 -- advance one headlight-flash step.
        //   luPattern              which pattern row to walk (0..E_HEADLIGHTFLASH_COUNT-1)
        //   lfTimeDelta            this frame's time step (asserted > 0)
        //   lpfUpdatedTime  in/out the time accumulated in the current state (asserted >= 0)
        //   lpuUpdatedState in/out the current state index (asserted <= KU_MAX_STATES_IN_PATTERN)
        //   lpbUpdatedHeadlightOff out  whether the headlight is OFF this step
        // Returns true while the pattern is still running (a positive duration remains for the
        // current state), false once the pattern has reached a terminator (<= 0) or run out of states.
        bool UpdateHeadlightFlash(u32 luPattern, f32 lfTimeDelta, f32* lpfUpdatedTime,
                                  u8* lpuUpdatedState, bool* lpbUpdatedHeadlightOff);
    };
}
