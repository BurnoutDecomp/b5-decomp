#pragma once

#include "BrnCommonTypes.h"   // VecFloat
#include "types.hpp"          // s32, u8, f32

// BrnPhysics::Deformation absorption-set enum + AbsorptionTable lookup curve.
//
// EAbsorptionSets (DWARF-authoritative) names the five deformation-absorption profiles a vehicle
// can be in (normal, AI-crashing, player-extreme-crash, shutdown, invincible). It is the type of
// ImpulseParams::meAbsorptionSet (BrnCollidableBody.h) and DeformableObject::meAbsorptionSet.
//
// AbsorptionTable is the static energy-absorption lookup curve that scales deformation impulses.
// Its three accessors index a static [5 sets][10 values] table of 16-byte AbsorptionValue vec4s
// (the X360 &unk_82FB9780 rodata, stride 160 == 10*16 per set), each returning ONE component
// broadcast across a VecFloat:
//   GetAbsorption          -> word0 (mfAbsorption) splatted
//   GetProportionToSpeed   -> word2 (mfProportionToSpeed) splatted
//   GetSpeedForMaxAbsorbtion -> word1 (mfSpeedForMaxAbsorbtion) splatted, then multiplied lane-wise
//                              by a second static vec4 table (&unk_82FB9C50, stride 16) indexed by
//                              the scale index.
// X360 addrs: GetAbsorption 0x825C0E70, GetSpeedForMaxAbsorbtion 0x825C0F08,
//             GetProportionToSpeed 0x825C0FB8.
// The Hex-Rays `int result` return is the VMX128 VecFloat-in-v1 idiom (the bodies end in
// vspltw/vmulfp128 into v1); the real signatures are VecFloat-returning with a typed
// EAbsorptionSets set index + a u8 value index. The two rodata tables' numeric contents are NOT in
// the exports -> the tables are carried as honest FLAGGED-0 placeholders in the .cpp (NEVER
// fabricated); the indexing shape/stride is exact.

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnAbsorptionTable.h:37. Selects which row of absorption tuning a deformation impulse
    // is scaled against.
    enum EAbsorptionSets
    {
        E_ABSORPTIONSET_NORMAL              = 0,
        E_ABSORPTIONSET_AI_CRASHING         = 1,
        E_ABSORPTIONSET_PLAYER_EXTREME_CRASH = 2,
        E_ABSORPTIONSET_SHUTDOWN            = 3,
        E_ABSORPTIONSET_INVINCIBLE          = 4,
        E_ABSORPTIONSETS_NUM                = 5
    };

    // Number of value steps per absorption set (BrnAbsorptionTable.h KU_NUM_ABSORPTION_VALUES; the
    // 0xAu bound the asm checks luValue against, and the 16*value stride within a 160-byte set row).
    const u8 KU_NUM_ABSORPTION_VALUES = 10;

    // One 16-byte vec4 row of the absorption curve (the lvx128 unit at 160*set + 16*value). Only
    // three of the four lanes are read by the accessors; the named fields follow the component each
    // accessor splats. The fourth lane is unused storage in this TU.
    struct AbsorptionValue
    {
        f32 mfAbsorption;             // word0 -- GetAbsorption splats this (vspltw v1,v0,0)
        f32 mfSpeedForMaxAbsorbtion;  // word1 -- GetSpeedForMaxAbsorbtion splats this (vspltw v13,v13,1)
        f32 mfProportionToSpeed;      // word2 -- GetProportionToSpeed splats this (vspltw v1,v0,2)
        f32 mfUnused;                 // word3 -- not read by this TU
    };

    // The static energy-absorption lookup curve. All-static accessors (the X360 takes no `this`);
    // modelled as a namespace-level struct of static methods.
    struct AbsorptionTable
    {
        // table[leSet][luValue].mfAbsorption broadcast across a VecFloat.
        static VecFloat GetAbsorption(EAbsorptionSets leSet, u8 luValue);

        // (table[leSet][luValue].mfSpeedForMaxAbsorbtion broadcast) * (scale-table[liScaleIndex]),
        // multiplied lane-by-lane (vmulfp128). liScaleIndex selects a 16-byte row of the secondary
        // static scale table (&unk_82FB9C50, stride 16).
        static VecFloat GetSpeedForMaxAbsorbtion(EAbsorptionSets leSet, u8 luValue, s32 liScaleIndex);

        // table[leSet][luValue].mfProportionToSpeed broadcast across a VecFloat.
        static VecFloat GetProportionToSpeed(EAbsorptionSets leSet, u8 luValue);
    };
}
}
