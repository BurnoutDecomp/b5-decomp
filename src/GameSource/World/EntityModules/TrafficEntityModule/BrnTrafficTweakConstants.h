#pragma once

// BrnTraffic::TweakValues -- the runtime-tunable "mega-tweek" block: 21 float behaviour
// constants the traffic fuzzy logic reads (and the debug tools can reload from file).
// Declaration shape + member names from the DecFIGS DWARF
// (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h:328),
// gated on the X360 ARTIST ledger (GetExtremeSwerveStickiness is attested; the 21-float
// layout is confirmed by FuzzyBehaviourLogic::SetConstantValue writing indices 0..20 to
// offsets 0x00..0x50 and ResetToDefaults reading mfExtremeSwerveStickiness at +0x2C).
//
// NOTE: only the TweakValues type is reconstructed here (the type FuzzyBehaviourLogic
// needs); the rest of BrnTrafficTweakConstants.h (the file-scope KF_*/KU_* constant pool)
// is left for the TU that owns those constants.

#include "types.hpp"   // f32

namespace BrnTraffic
{
    class TweakValues
    {
    public:
        f32 GetStoplineVariation()       const { return mfStoplineVariation; }
        f32 GetRaceCarStopDist()         const { return mfRaceCarStopDist; }
        f32 GetGapClosingFactor()        const { return mfGapClosingFactor; }
        f32 GetMinNormalAcceleration()   const { return mfMinNormalAcceleration; }
        f32 GetMaxNormalAcceleration()   const { return mfMaxNormalAcceleration; }
        f32 GetMinAcceleration()         const { return mfMinAcceleration; }
        f32 GetMaxAcceleration()         const { return mfMaxAcceleration; }
        f32 GetMinSpeedForCutoff()       const { return mfMinSpeedForCutoff; }
        f32 GetMinStopDist()             const { return mfMinStopDist; }
        f32 GetSwerveScoreScale()        const { return mfSwerveScoreScale; }
        f32 GetExtremeSwerveScoreScale() const { return mfExtremeSwerveScoreScale; }
        f32 GetExtremeSwerveStickiness() const { return mfExtremeSwerveStickiness; }
        f32 GetExtremeSwerveMinTime()    const { return mfExtremeSwerveMinTime; }
        f32 GetSpinAirRamMagMin()        const { return mfSpinAirRamMagMin; }
        f32 GetSpinAirRamMagMax()        const { return mfSpinAirRamMagMax; }
        f32 GetSpinAirRamDecay()         const { return mfSpinAirRamDecay; }
        f32 GetSpinAirRamZDist()         const { return mfSpinAirRamZDist; }
        f32 GetRollAirRamMagMin()        const { return mfRollAirRamMagMin; }
        f32 GetRollAirRamMagMax()        const { return mfRollAirRamMagMax; }
        f32 GetRollAirRamDecay()         const { return mfRollAirRamDecay; }
        f32 GetRollAirRamSideDist()      const { return mfRollAirRamSideDist; }

    private:
        // The 21 "mega-tweek" floats, in the order indexed by the behaviour file loader
        // (KAPC_MEGATWEEK_CONSTANT_NAMES[index] -> member at offset index*4).
        f32 mfStoplineVariation;        // index 0   (0x00)
        f32 mfRaceCarStopDist;          // index 1   (0x04)
        f32 mfGapClosingFactor;         // index 2   (0x08)
        f32 mfMinNormalAcceleration;    // index 3   (0x0C)
        f32 mfMaxNormalAcceleration;    // index 4   (0x10)
        f32 mfMinAcceleration;          // index 5   (0x14)
        f32 mfMaxAcceleration;          // index 6   (0x18)
        f32 mfMinSpeedForCutoff;        // index 7   (0x1C)
        f32 mfMinStopDist;              // index 8   (0x20)
        f32 mfSwerveScoreScale;         // index 9   (0x24)
        f32 mfExtremeSwerveScoreScale;  // index 10  (0x28)
        f32 mfExtremeSwerveStickiness;  // index 11  (0x2C)
        f32 mfExtremeSwerveMinTime;     // index 12  (0x30)
        f32 mfSpinAirRamMagMin;         // index 13  (0x34)
        f32 mfSpinAirRamMagMax;         // index 14  (0x38)
        f32 mfSpinAirRamDecay;          // index 15  (0x3C)
        f32 mfSpinAirRamZDist;          // index 16  (0x40)
        f32 mfRollAirRamMagMin;         // index 17  (0x44)
        f32 mfRollAirRamMagMax;         // index 18  (0x48)
        f32 mfRollAirRamDecay;          // index 19  (0x4C)
        f32 mfRollAirRamSideDist;       // index 20  (0x50)

    public:
        // Index-addressed write of the 21-float mega-tweek block (the form the behaviour
        // loader + FuzzyBehaviourLogic::SetConstantValue use). Returns false for an
        // out-of-range index (caller asserts "Unknown mega-tweek value").
        bool SetByIndex(s32 liIndex, f32 lfValue)
        {
            if (liIndex < 0 || liIndex > 20)
                return false;
            (&mfStoplineVariation)[liIndex] = lfValue;
            return true;
        }
    };
}
