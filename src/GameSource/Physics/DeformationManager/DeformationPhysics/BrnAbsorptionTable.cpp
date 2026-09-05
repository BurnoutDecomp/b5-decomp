#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnAbsorptionTable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
// ENextSensorDirection / E_NSD_NUM -- the real bound of the secondary scale table below.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnSharedDeformationEnums.h"

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
// ⭐⭐⭐ BOTH TABLES RECOVERED 2026-08-15 (walls leg 7). They were FLAGGED-0 placeholders because
// "the numeric contents are NOT present in the per-function exports" -- true, but they are not in
// the exports because they are DYNAMIC-INIT globals: both read 0.0 straight out of the image, and
// their values only exist in their static-initialiser functions. Those initialisers were located by
// scanning the image for the tables' `@l` halfwords and then SIMULATED exactly (the initialisers are
// branch-free: they fill stack slots from named float literals and copy them out 16 bytes at a time).
//   &unk_82FB9780  <- sub_82C5DFD8 (0x82C5DFD8..0x82C5E6BC), 18 distinct literals, 0x320 bytes
//                     written as 50 aligned vector stores at +0x000..+0x310 == exactly [5][10]x16.
//   &unk_82FB9C50  <- the thunk at 0x82C5DE90..0x82C5DF3C, SIX splatted rows at +0x00..+0x50.
// The reader (x360rd) was re-validated before the values were believed: flt_8209D738 -> 0.00015,
// flt_82009E10 -> 1000.0, flt_8200426C -> 5.0, 3/3 exact.
//
// ⭐ WHY THE RESULT IS TRUSTWORTHY, beyond "the simulator ran": the recovered curve is
// self-validating in three independent ways.
//   1. Every set's mfAbsorption column is MONOTONE DECREASING across the ten damage steps
//      (0.98 -> 0.1), which is what an absorption curve indexed by accumulated damage must be.
//   2. E_ABSORPTIONSET_INVINCIBLE comes out ALL-ZERO absorption with mfSpeedForMaxAbsorbtion 100 --
//      an invincible car absorbs nothing into deformation. The enum name predicts the row.
//   3. E_ABSORPTIONSET_AI_CRASHING and E_ABSORPTIONSET_SHUTDOWN come out byte-identical, which is
//      what two aliases of the same tuning look like -- and not something a mis-strided read
//      produces.
//
// ⚠️⚠️ THE OLD FLAGGED-0 WAS NOT INERT -- it was a WRONG VALUE. Zero absorption is not this
// expression's identity element: it IS the INVINCIBLE row. With the placeholder in place every
// vehicle in the game was running the invincible absorption profile. This is the "a flagged-zero
// placeholder is only safe when 0 is the identity element" rule biting for real.
//
// ⭐⭐ THE GROUND-CONTACT ROWS RE-VERIFIED INDEPENDENTLY 2026-09-05 (crash wave). The crash
// campaign's pole-vault solve (a tail sensor driven into the road at 7.5 m/s converting into
// 1.25 m/s of CoM lift in one frame, effective mass 448 kg) rests on the absorption row those tail
// sensors use, and that row had never been checked on its own. It has now, WITHOUT re-running the
// simulation above: the initialiser's two loads were traced by hand and the literals read with
// x360rd.
//   PUSMC01's rear/underside sensors carry mu8AbsorbtionLevel 2 (sensors 14/16/18) and 3
//   (15/17/19); the crash corpus runs absorption SET 0 (E_ABSORPTIONSET_NORMAL, noDamageTimer
//   expired), so the rows in play are [0][2] and [0][3].
//   [0][2] is the vector stored at &unk_82FB9780+0x20 by `stvx128 v0,r11,r10` @0x82C5E194, whose
//     source stack quad (r1-0xE0) is filled at 0x82C5E050/0x82C5E004/0x82C5E05C/0x82C5E060 from
//     flt_8208F9C8 / flt_82004A18 / flt_82004D00 / flt_82001CC0
//     == 0.800000011920929 / 80.0 / 0.6000000238418579 / 0.0
//   [0][3] is the +0x30 store @0x82C5E1A4 out of r1-0x20, filled at 0x82C5E074/0x82C5E008/
//     0x82C5E064/0x82C5E068 from flt_82004C68 / flt_82004A18 / flt_82004D00 / flt_82001CC0
//     == 0.699999988079071 / 80.0 / 0.6000000238418579 / 0.0
// Both match the table below to the last digit. ⇒ the ground contact's absorption row is the
// image's; the pole-vault arithmetic is not standing on a recovered guess.
//
// Callers (X360 xrefs): GetAbsorption <- DeformationSensor::RecievePassedOnImpulse,
// DeformationSensor::ApplyLocalImpulse; the other two <- DeformationSensor::ApplyLocalImpulse.
// ⚠️ ALL of those consumers are downstream of DeformationSensor::ApplyLocalImpulse, which is still
// a log-once gate -- so landing the real numbers changes NOTHING observable today. It is banked
// correct now so the sensor slice, when it lands, is not built on an invincible car.

namespace BrnPhysics
{
namespace Deformation
{
    // ⭐ THE REAL absorption curve -- X360 &unk_82FB9780, recovered from its static initialiser
    // sub_82C5DFD8 (see the TU banner). Layout `160*set + 16*value` == [5][10] AbsorptionValue.
    // Every row below carries the image address of the 16 bytes it reproduces.
    static const AbsorptionValue
        KsaAbsorptionCurve[E_ABSORPTIONSETS_NUM][KU_NUM_ABSORPTION_VALUES] =
    {
        // E_ABSORPTIONSET_NORMAL
        {
            { 0.980000019f, 80.0f, 0.0f,          0.0f },   // [0]  @0x82FB9780
            { 0.899999976f, 80.0f, 0.0f,          0.0f },   // [1]  @0x82FB9790
            { 0.800000012f, 80.0f, 0.600000024f,  0.0f },   // [2]  @0x82FB97A0
            { 0.699999988f, 80.0f, 0.600000024f,  0.0f },   // [3]  @0x82FB97B0
            { 0.600000024f, 80.0f, 0.600000024f,  0.0f },   // [4]  @0x82FB97C0
            { 0.5f,         80.0f, 0.600000024f,  0.0f },   // [5]  @0x82FB97D0
            { 0.400000006f, 80.0f, 0.600000024f,  0.0f },   // [6]  @0x82FB97E0
            { 0.300000012f, 80.0f, 0.600000024f,  0.0f },   // [7]  @0x82FB97F0
            { 0.200000003f, 80.0f, 0.600000024f,  0.0f },   // [8]  @0x82FB9800
            { 0.100000001f, 80.0f, 0.600000024f,  0.0f },   // [9]  @0x82FB9810
        },
        // E_ABSORPTIONSET_AI_CRASHING
        {
            { 0.980000019f, 30.0f, 0.5f,          0.0f },   // [0]  @0x82FB9820
            { 0.949999988f, 30.0f, 0.5f,          0.0f },   // [1]  @0x82FB9830
            { 0.920000017f, 30.0f, 0.5f,          0.0f },   // [2]  @0x82FB9840
            { 0.910000026f, 30.0f, 0.400000006f,  0.0f },   // [3]  @0x82FB9850
            { 0.899999976f, 30.0f, 0.400000006f,  0.0f },   // [4]  @0x82FB9860
            { 0.800000012f, 30.0f, 0.400000006f,  0.0f },   // [5]  @0x82FB9870
            { 0.5f,         30.0f, 0.400000006f,  0.0f },   // [6]  @0x82FB9880
            { 0.300000012f, 30.0f, 0.400000006f,  0.0f },   // [7]  @0x82FB9890
            { 0.200000003f, 30.0f, 0.400000006f,  0.0f },   // [8]  @0x82FB98A0
            { 0.100000001f, 30.0f, 0.400000006f,  0.0f },   // [9]  @0x82FB98B0
        },
        // E_ABSORPTIONSET_PLAYER_EXTREME_CRASH
        {
            { 0.980000019f, 30.0f, 1.0f,          0.0f },   // [0]  @0x82FB98C0
            { 0.899999976f, 30.0f, 1.0f,          0.0f },   // [1]  @0x82FB98D0
            { 0.800000012f, 30.0f, 0.699999988f,  0.0f },   // [2]  @0x82FB98E0
            { 0.699999988f, 30.0f, 0.699999988f,  0.0f },   // [3]  @0x82FB98F0
            { 0.600000024f, 30.0f, 0.699999988f,  0.0f },   // [4]  @0x82FB9900
            { 0.5f,         30.0f, 0.699999988f,  0.0f },   // [5]  @0x82FB9910
            { 0.400000006f, 30.0f, 0.699999988f,  0.0f },   // [6]  @0x82FB9920
            { 0.300000012f, 30.0f, 0.699999988f,  0.0f },   // [7]  @0x82FB9930
            { 0.200000003f, 30.0f, 0.699999988f,  0.0f },   // [8]  @0x82FB9940
            { 0.100000001f, 30.0f, 0.699999988f,  0.0f },   // [9]  @0x82FB9950
        },
        // E_ABSORPTIONSET_SHUTDOWN  -- byte-identical to AI_CRASHING in the image (two tunings that
        // happen to share values; reproduced as the image has them, not aliased).
        {
            { 0.980000019f, 30.0f, 0.5f,          0.0f },   // [0]  @0x82FB9960
            { 0.949999988f, 30.0f, 0.5f,          0.0f },   // [1]  @0x82FB9970
            { 0.920000017f, 30.0f, 0.5f,          0.0f },   // [2]  @0x82FB9980
            { 0.910000026f, 30.0f, 0.400000006f,  0.0f },   // [3]  @0x82FB9990
            { 0.899999976f, 30.0f, 0.400000006f,  0.0f },   // [4]  @0x82FB99A0
            { 0.800000012f, 30.0f, 0.400000006f,  0.0f },   // [5]  @0x82FB99B0
            { 0.5f,         30.0f, 0.400000006f,  0.0f },   // [6]  @0x82FB99C0
            { 0.300000012f, 30.0f, 0.400000006f,  0.0f },   // [7]  @0x82FB99D0
            { 0.200000003f, 30.0f, 0.400000006f,  0.0f },   // [8]  @0x82FB99E0
            { 0.100000001f, 30.0f, 0.400000006f,  0.0f },   // [9]  @0x82FB99F0
        },
        // E_ABSORPTIONSET_INVINCIBLE -- absorbs nothing, at any damage step. ⭐ This row is the
        // recovery's own sanity check: the enum name predicts exactly this shape.
        {
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [0]  @0x82FB9A00
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [1]  @0x82FB9A10
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [2]  @0x82FB9A20
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [3]  @0x82FB9A30
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [4]  @0x82FB9A40
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [5]  @0x82FB9A50
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [6]  @0x82FB9A60
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [7]  @0x82FB9A70
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [8]  @0x82FB9A80
            { 0.0f, 100.0f, 0.0f, 0.0f },   // [9]  @0x82FB9A90
        },
    };

    // ⭐ THE REAL secondary scale table -- X360 &unk_82FB9C50, recovered from its initialiser thunk
    // at 0x82C5DE90..0x82C5DF3C. ⚠️ THE OLD BOUND WAS WRONG: it was modelled at
    // E_ABSORPTIONSETS_NUM (5) rows on the guess that liScaleIndex is set-shaped. The initialiser
    // writes SIX rows (`stvx128` at +0x00,0x10,0x20,0x30,0x40,0x50) -- so the index is an
    // ENextSensorDirection, not a set, and a 5-row array was one element short of the real bound.
    // Every row is a single float splatted to all four lanes (`vspltw vN,vN,0` before each store).
    // ⭐ Semantic cross-check with KA_IMPULSE_DIRECTIONS {+X,-X,+Y,-Y,+Z,-Z}: sides 0.5/0.5,
    // UP 1.0, DOWN 0.2, front/back 1.0/1.0 -- the car resists deformation most from underneath,
    // which is the physically right answer and not something a mis-strided read produces.
    static const VecFloat KsaSpeedScaleTable[E_NSD_NUM] =
    {
        { 0.5f, 0.5f, 0.5f, 0.5f },   // [0] @0x82FB9C50  flt_82001DA0 = 0.5
        { 0.5f, 0.5f, 0.5f, 0.5f },   // [1] @0x82FB9C60  flt_82001DA0 = 0.5
        { 1.0f, 1.0f, 1.0f, 1.0f },   // [2] @0x82FB9C70  flt_82001C98 = 1.0
        { 0.200000003f, 0.200000003f, 0.200000003f, 0.200000003f },  // [3] @0x82FB9C80  flt_82004744 = 0.2
        { 1.0f, 1.0f, 1.0f, 1.0f },   // [4] @0x82FB9C90  flt_82001C98 = 1.0
        { 1.0f, 1.0f, 1.0f, 1.0f },   // [5] @0x82FB9CA0  flt_82001C98 = 1.0
    };

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
