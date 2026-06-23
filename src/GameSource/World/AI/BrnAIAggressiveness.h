#pragma once

// BrnAI::Aggressiveness -- the per-racer aggression tuning block: a small bundle of
// 0..1 parameters (aggression level + the speed-matching proximity / time / relative-
// speed / acceleration-rate knobs) that the AI uses to shadow and overtake a target
// car. This is the lightweight tuning struct, distinct from BrnAI::AIAggression (the
// large per-frame aggression state machine in BrnAIAggression.h).
//
// DWARF home: GameSource/World/AI/BrnAIAggressiveness.h (struct BrnAI::Aggressiveness,
// :28 in the DecFIGS dump; members :77-85). LAYOUT confirmed against the X360 asm of
// the three bodied setters: mfProximitySpeedMatch @ this+0x8
// (SetProximityToSpeedMatch @0x827645E0), mfTimeForSpeedMatch @ this+0xC
// (SetTimeForSpeedMatch @0x827647B0), mfAcclerationRateForSpeedMatch @ this+0x14
// (SetAcclerationRateForSpeedMatch @0x827646C8). The DWARF spells the floats
// float32_t; the project scalar is f32 (same width). mRandom is a static (extern) member
// shared across instances, so it contributes nothing to the per-instance layout. Access
// is by name.
//
// Only the three speed-match setters are bodied in this TU; the remaining members
// (Construct/Prepare/SetAggression/the getters/...) are declared here and bodied in
// their own TUs.

#include "types.hpp"

namespace CgsNumeric { class Random; }

namespace BrnAI
{
    // DWARF BrnAIAggressiveness.h:28.
    struct Aggressiveness
    {
        void Construct();                                 // :32
        bool Prepare();                                   // :35
        void SetAggression(f32 lfAggression);             // :39
        s32  GetTakedownCount();                          // :42
        f32  GetAggressionLevel() const;                  // :45
        f32  GetRandomNumber() const;                     // :48

        // @0x827645E0 -- store the speed-match proximity (asserts lfValue in [0,1]).
        void SetProximityToSpeedMatch(f32 lfValue);       // :52
        // @0x827647B0 -- store the speed-match time (asserts lfValue in [0,1]).
        void SetTimeForSpeedMatch(f32 lfValue);           // :56

        f32  GetProximityToSpeedMatch() const;            // :59
        f32  GetTimeForSpeedMatch() const;                // :62
        f32  GetRelativeSpeedForMatch() const;            // :65
        void SetRelativeSpeedForMatch(f32 lfValue);       // :68
        f32  GetAcclerationRateForSpeedMatch() const;     // :71
        // @0x827646C8 -- store the speed-match acceleration rate (asserts lfValue in [0,1]).
        void SetAcclerationRateForSpeedMatch(f32 lfValue); // :74

    private:
        // Shared across instances (DWARF :77 extern); not part of the per-instance layout.
        static CgsNumeric::Random mRandom;

        f32  mfAggressionLevel;              // +0x00  :79
        bool mbAggressionLevelSet;           // +0x04  :80
        f32  mfProximitySpeedMatch;          // +0x08  :82
        f32  mfTimeForSpeedMatch;            // +0x0C  :83
        f32  mfRelativeSpeedForSpeedMatch;   // +0x10  :84
        f32  mfAcclerationRateForSpeedMatch; // +0x14  :85
    };
}
