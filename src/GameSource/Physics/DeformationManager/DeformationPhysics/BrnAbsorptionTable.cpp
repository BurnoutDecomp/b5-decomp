#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Deformation::AbsorptionTable -- the static energy-absorption lookup curve that scales
// deformation impulses. Three accessors, reconstructed store-for-store from the X360 ARTIST asm:
//   GetAbsorption            @ 0x825C0E70
//   GetSpeedForMaxAbsorbtion @ 0x825C0F08
//   GetProportionToSpeed     @ 0x825C0FB8
//
// Each indexes the primary static table at &unk_82FB9780 as `160*set + 16*value`, i.e.
// [E_ABSORPTIONSETS_NUM (5) sets][KU_NUM_ABSORPTION_VALUES (10) values] of a 16-byte AbsorptionValue
// vec4 (160 == 10 * 16 per set). The lvx128 loads the whole vec4; a vspltw broadcasts ONE component
// into v1 -- modelled here as `Vector4{ lf, lf, lf, lf }`, the project's standard broadcast idiom.
//
// Each accessor has the same two bounds asserts (set < E_ABSORPTIONSETS_NUM, value <
// KU_NUM_ABSORPTION_VALUES). They are non-gating tripwires: in the asm execution continues past a
// failed assert and the table lookup runs regardless, so the C++ falls through the same way.
//
// GetSpeedForMaxAbsorbtion additionally loads a 16-byte row of a SECOND static table at
// &unk_82FB9C50 (indexed `16*liScaleIndex`) and multiplies the splatted word1 by it lane-by-lane
// (vmulfp128 v1, v13, v0) -- a full vec4 * vec4 product, not a scalar.
//
// FLAGGED-0 placeholders: the numeric contents of BOTH rodata tables (&unk_82FB9780 and
// &unk_82FB9C50) are NOT present in the per-function exports. Per project rule they are carried as
// correctly-shaped, zero-initialized file-static placeholders -- NEVER fabricated. The indexing
// shape/stride is exact; the numeric output stays inert until the real bytes are recovered from the
// XEX rodata.
//
// Callers (X360 xrefs): GetAbsorption <- DeformationSensor::RecievePassedOnImpulse,
// DeformationSensor::ApplyLocalImpulse; the other two <- DeformationSensor::ApplyLocalImpulse.

namespace BrnPhysics
{
namespace Deformation
{
    // FLAGGED-0 PLACEHOLDER for the primary absorption curve at X360 rodata &unk_82FB9780.
    // Shape/stride is authoritative (160*set + 16*value => [5][10] of a 16-byte AbsorptionValue);
    // the numeric values are NOT in the exports and are carried as honest zeros (NEVER fabricated).
    static const AbsorptionValue
        KsaAbsorptionCurve[E_ABSORPTIONSETS_NUM][KU_NUM_ABSORPTION_VALUES] = {};

    // FLAGGED-0 PLACEHOLDER for the secondary scale table at X360 rodata &unk_82FB9C50.
    // Indexed `16*liScaleIndex` => an array of 16-byte vec4 rows. The row count is not pinned by the
    // exports; modelled at E_ABSORPTIONSETS_NUM rows (the only index source visible in this TU is a
    // set-shaped value) so the lookup is well-formed. Values are honest zeros (NEVER fabricated).
    static const VecFloat KsaSpeedScaleTable[E_ABSORPTIONSETS_NUM] = {};

    // -----------------------------------------------------------------------------------------
    // GetAbsorption @ 0x825C0E70
    //   lvx128 v0,[&unk_82FB9780 + 160*set + 16*value]; vspltw v1,v0,0  => word0 broadcast.
    // -----------------------------------------------------------------------------------------
    VecFloat AbsorptionTable::GetAbsorption(EAbsorptionSets leSet, u8 luValue)
    {
        CGS_ASSERT(leSet < E_ABSORPTIONSETS_NUM, "leSet < E_ABSORPTIONSETS_NUM");
        CGS_ASSERT(luValue < KU_NUM_ABSORPTION_VALUES, "luValue < KU_NUM_ABSORPTION_VALUES");

        const f32 lfAbsorption = KsaAbsorptionCurve[leSet][luValue].mfAbsorption;
        return Vector4{ lfAbsorption, lfAbsorption, lfAbsorption, lfAbsorption };
    }

    // -----------------------------------------------------------------------------------------
    // GetSpeedForMaxAbsorbtion @ 0x825C0F08
    //   lvx128 v0,[&unk_82FB9C50 + 16*liScaleIndex]               // full vec4 scale row
    //   lvx128 v13,[&unk_82FB9780 + 160*set + 16*value]; vspltw v13,v13,1   // word1 broadcast
    //   vmulfp128 v1,v13,v0                                       // lane-wise product
    // -----------------------------------------------------------------------------------------
    VecFloat AbsorptionTable::GetSpeedForMaxAbsorbtion(EAbsorptionSets leSet, u8 luValue, s32 liScaleIndex)
    {
        CGS_ASSERT(leSet < E_ABSORPTIONSETS_NUM, "leSet < E_ABSORPTIONSETS_NUM");
        CGS_ASSERT(luValue < KU_NUM_ABSORPTION_VALUES, "luValue < KU_NUM_ABSORPTION_VALUES");

        const VecFloat lScale = KsaSpeedScaleTable[liScaleIndex];
        const f32 lfSpeed = KsaAbsorptionCurve[leSet][luValue].mfSpeedForMaxAbsorbtion;
        return Vector4{ lfSpeed * lScale.x, lfSpeed * lScale.y, lfSpeed * lScale.z, lfSpeed * lScale.w };
    }

    // -----------------------------------------------------------------------------------------
    // GetProportionToSpeed @ 0x825C0FB8
    //   lvx128 v0,[&unk_82FB9780 + 160*set + 16*value]; vspltw v1,v0,2  => word2 broadcast.
    // -----------------------------------------------------------------------------------------
    VecFloat AbsorptionTable::GetProportionToSpeed(EAbsorptionSets leSet, u8 luValue)
    {
        CGS_ASSERT(leSet < E_ABSORPTIONSETS_NUM, "leSet < E_ABSORPTIONSETS_NUM");
        CGS_ASSERT(luValue < KU_NUM_ABSORPTION_VALUES, "luValue < KU_NUM_ABSORPTION_VALUES");

        const f32 lfProportion = KsaAbsorptionCurve[leSet][luValue].mfProportionToSpeed;
        return Vector4{ lfProportion, lfProportion, lfProportion, lfProportion };
    }
}
}
