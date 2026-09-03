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

        // The four speed-match READERS. None of them has an X360 symbol of its own (names.tsv
        // carries only the three setters + GetAggressionLevel for this class), i.e. the console
        // inlines every one of them at its call site -- so they are restored as header inlines
        // rather than as out-of-line bodies in BrnAIAggressiveness.cpp. Each is the plain load
        // of the member its same-named setter stores, addressed BY NAME:
        //   GetProximityToSpeedMatch        <- SetProximityToSpeedMatch @0x827645E0 (this+0x08)
        //   GetTimeForSpeedMatch            <- SetTimeForSpeedMatch @0x827647B0     (this+0x0C)
        //   GetRelativeSpeedForMatch        <- SetRelativeSpeedForMatch             (this+0x10)
        //   GetAcclerationRateForSpeedMatch <- SetAcclerationRateForSpeedMatch @0x827646C8 (this+0x14)
        // mAggressiveness sits at AICar+0x140C, so every one of these loads shows up in the
        // asm as an `lfs` off the AICar pointer. The four call sites that pin them:
        //   0x1414 (== +0x08 proximity)  GetSpeedMatchSpeed @0x8277E24C,
        //                                OutOfSpeedMatchRange @0x8278B6BC and @0x8278B728
        //   0x1418 (== +0x0C time)       UpdateAggressionStateFallPast @0x827937A0
        //   0x141C (== +0x10 rel. speed) GetSpeedMatchSpeed @0x8277E330,
        //                                SetSlowFallbackSpeed @0x82770AEC
        //   0x1420 (== +0x14 accel rate) CalcSpeedMatchSpeed @0x8278B814
        f32  GetProximityToSpeedMatch() const        { return mfProximitySpeedMatch; }          // :59
        f32  GetTimeForSpeedMatch() const            { return mfTimeForSpeedMatch; }            // :62
        f32  GetRelativeSpeedForMatch() const        { return mfRelativeSpeedForSpeedMatch; }   // :65
        void SetRelativeSpeedForMatch(f32 lfValue);       // :68
        f32  GetAcclerationRateForSpeedMatch() const { return mfAcclerationRateForSpeedMatch; } // :71
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
