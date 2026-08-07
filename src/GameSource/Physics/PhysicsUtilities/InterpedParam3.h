#pragma once

// BrnPhysics::InterpedParam3 -- a three-parameter interpolated curve packed into one 16-byte
// SIMD register.
//
// CANONICAL HOME. DecFIGS puts this type at GameSource/Physics/PhysicsUtilities/InterpedParam3.h
// (see references/DecFIGS/dwarfdump/GameSource/Physics/PhysicsUtilities/InterpedParam3.h and the
// matching .cpp); before this file existed, VehicleAttribs.h carried a private declaration of it.
//
// LAYOUT: the DWARF's single member is `Vector3 mvParams` -- three live lanes, `.w` unused. That
// is confirmed by the asm: Construct @0x8259CD30 and Prepare @0x8259CDC0 each perform exactly
// THREE lane inserts and never touch lane 3. (The Feb-2007 partial source spells the same member
// as the pre-SIMD `float32_t mafParams[3]`.)

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3

namespace BrnPhysics
{
    class InterpedParam3
    {
    public:
        // @0x8259CD30 (36 instrs) -- zero the three parameters.
        void Construct();

        // @0x8259CDC0 (34 instrs) -- seed the three parameters.
        void Prepare(f32 lParamA, f32 lParamB, f32 lParamC);

        // DWARF InterpedParam3.h:60. The console asserts 0 <= liParamIndex < 3.
        f32 GetParam(s32 liParamIndex) const
        {
            return (&mvParams.x)[liParamIndex];
        }

        // DWARF InterpedParam3.h:57 `VecFloat GetInterped(VecFloat) const`.
        // ⭐ BODIED 2026-08-07 (wheel-cluster wave) from the copy the console INLINES into
        // VehiclePhysics::UpdateBrakesAndGetBrakingFactor @0x825D0510..0x825D0574 (a header
        // inline on the console -- no out-of-line copy exists in the image, which is why the
        // decl sat unbodied). The asm is one de Casteljau step pair over the three params:
        //   vsubfp  v10 = B - A ; vsubfp v9 = C - B
        //   vmaddfp v13 = (B-A)*t + A ; vmaddfp v12 = (C-B)*t + B
        //   vsubfp/vmaddfp result = (v12-v13)*t + v13
        // i.e. a quadratic Bezier through A (t=0) and C (t=1) with B as the control point.
        VecFloat GetInterped(VecFloat lvfI) const
        {
            const f32 lfT = lvfI.x;
            const f32 lfAB = mvParams.x + (mvParams.y - mvParams.x) * lfT;   // lerp(A, B, t)
            const f32 lfBC = mvParams.y + (mvParams.z - mvParams.y) * lfT;   // lerp(B, C, t)
            const f32 lfResult = lfAB + (lfBC - lfAB) * lfT;                 // lerp(AB, BC, t)
            return VecFloat{ lfResult, lfResult, lfResult, lfResult };
        }

    private:
        Vector3 mvParams;   // +0x00 (.x = ParamA, .y = ParamB, .z = ParamC; .w unused)
    };

    static_assert(sizeof(InterpedParam3) == 16, "InterpedParam3 is one 16-byte register");
}
