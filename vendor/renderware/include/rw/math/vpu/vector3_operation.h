#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu Vector3 operation vocabulary
// (EARenderWare rwmath 1.02.00, rw/math/vpu/vector3_operation.h -- sibling to types.h).
// Reconstructed from BURNOUT_X360_ARTIST.XEX with the public-header name and signatures
// locked from the Feb-2007 leak (rwmath/1.02.00/include/rw/math/vpu/vector3_operation.h)
// and the DecFIGS DWARF.
//
// The console SDK implements these over AltiVec/VMX SIMD (operator-/+ = vsubfp/vaddfp,
// Magnitude = rsqrt refinement, IsValid = per-lane vcmpeqfp self-equality NaN test). The
// PC reconstruction operates on the named float lanes of the flat Vector3 aggregate in
// types.h (the SIMD machinery -- VecFloat/VectorIntrinsic/Mask -- is not modelled here).
//
// gen-tool note: tools/gen_rwcore_headers.py only writes rwcore_enums.h/rwcore_structs.h/
// rwcore.h under include/rw/; it never touches rw/math/vpu/, so this hand-maintained header
// (like its sibling types.h) is immune to regeneration.

#include <cmath>                  // std::sqrt for Magnitude/Length
#include "rw/math/vpu/types.h"    // rw::math::vpu::Vector3 (Vector2/3/4 + matrices)

namespace rw
{
namespace math
{
namespace vpu
{
    // X360 0x82276868. Full 4-lane vsubfp (binary `vsubfp v1,v1,v2`, SDK Subtract over mV).
    // Consumed by BrnOfflineGameMode.cpp via lv3LandmarkPosition - lv3Origin.
    inline Vector3 operator-(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{ lLhs.x - lRhs.x, lLhs.y - lRhs.y, lLhs.z - lRhs.z, lLhs.w - lRhs.w };
    }

    // X360 0x825B2620. Full 4-lane vaddfp (binary `vaddfp v1,v1,v2`, SDK Add over mV).
    inline Vector3 operator+(Vector3 lLhs, Vector3 lRhs)
    {
        return Vector3{ lLhs.x + lRhs.x, lLhs.y + lRhs.y, lLhs.z + lRhs.z, lLhs.w + lRhs.w };
    }

    // SDK Dot returns a broadcast VecFloat; scalar call sites (BrnOfflineGameMode.cpp) read
    // one lane -> reconstructed as scalar float. xyz lanes only (w excluded).
    inline float Dot(Vector3 lLhs, Vector3 lRhs)
    {
        return lLhs.x * lRhs.x + lLhs.y * lRhs.y + lLhs.z * lRhs.z;
    }

    // SDK free function is Magnitude; reconstructed game source spells it Length. Both
    // provided. De-optimised from the X360 rsqrt-refinement to a plain std::sqrt over xyz.
    // Consumed via lfDistance = Length(lv3Delta).
    inline float Magnitude(Vector3 lrVector)
    {
        return std::sqrt(lrVector.x * lrVector.x + lrVector.y * lrVector.y + lrVector.z * lrVector.z);
    }
    inline float Length(Vector3 lrVector) { return Magnitude(lrVector); }

    // SDK Vector3::GetX/Y/Z (vector3_type_inline.h) return VecFloatRef lane refs; modelled
    // as free accessors over the named lanes because the committed PC Vector3 is a flat
    // float-lane aggregate (no VecFloatRef machinery). GetY consumed by BrnOfflineGameMode.cpp.
    inline float GetX(const Vector3& lrVector) { return lrVector.x; }
    inline float GetY(const Vector3& lrVector) { return lrVector.y; }
    inline float GetZ(const Vector3& lrVector) { return lrVector.z; }

    // Canonical home is rw::math::vpu (DWARF: rw::math::vpu::IsValid resolved 8x in
    // BrnTrafficLightCollection.cpp; SDK declares extern bool IsValid(Vector3) and defines
    // it as IsValid(X)&&IsValid(Y)&&IsValid(Z)). The scalar IsValid(VecFloat) is the
    // vcmpeqfp self-equality NaN test; reconstructed as per-lane x==x self-comparison.
    // NOT RwMath:: -- that string is only the verbatim baked assert literal preserved at
    // the BrnTrafficLightTrigger.cpp call sites.
    inline bool IsValid(const Vector3& lrVector)
    {
        return lrVector.x == lrVector.x
            && lrVector.y == lrVector.y
            && lrVector.z == lrVector.z;
    }
}
}
}
