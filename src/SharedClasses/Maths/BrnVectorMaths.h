#pragma once

#include <cmath>              // std::sqrt for Length()
#include "BrnCommonTypes.h"   // rw::math::vpu::Vector3 (the generated aggregate type)

// TEMPORARY project-side helper for the rw::math::vpu::Vector3 operation vocabulary
// (operator-/Dot/Length + component accessors) that reconstructed game code needs.
//
// The RenderWare SDK declares these under the GENERATED vendor tree:
//   - operator-(Vector3, Vector3) and Dot(Vector3, Vector3) in
//     rw/math/vpu/vector3_operation_inline.h (operator- has X360 symbol 0x82276868);
//   - GetX/GetY/GetZ in rw/math/vpu/vector3_type_inline.h.
// vendor/renderware/include/rw/math/vpu/* is produced by tools/gen_rwcore_headers.py, so
// editing it directly risks being clobbered on regeneration. They are therefore kept HERE,
// in the project src tree, as free functions in the rw::math::vpu namespace (found via ADL
// on a Vector3 argument). The Vector3 aggregate itself is left untouched.
//
// PENDING MAINTAINER DECISION on the canonical home (reconstruct the SDK's
// vector3_operation_inline.h / vector3_type_inline.h, or a dedicated CgsMaths helper).
namespace rw { namespace math { namespace vpu {

// X360 0x82276868 (ledger-attested). Touches the xyz lanes; w lane := 0.
inline Vector3 operator-(Vector3 lLhs, Vector3 lRhs)
{
    return Vector3{ lLhs.x - lRhs.x, lLhs.y - lRhs.y, lLhs.z - lRhs.z, 0.0f };
}

// SDK Dot returns a broadcast VecFloat; scalar float-context call sites read one lane, so
// the PC reconstruction returns a scalar float directly.
inline float Dot(Vector3 lLhs, Vector3 lRhs)
{
    return lLhs.x * lRhs.x + lLhs.y * lRhs.y + lLhs.z * lRhs.z;
}

// DWARF: Vector3::GetX/GetY/GetZ (vector3_type_inline.h) -- modelled as free accessors so
// the generated Vector3 aggregate is not modified.
inline float GetX(const Vector3& lv) { return lv.x; }
inline float GetY(const Vector3& lv) { return lv.y; }
inline float GetZ(const Vector3& lv) { return lv.z; }

// Euclidean length of the xyz lanes (the SDK spells this as the free function Magnitude).
inline float Length(Vector3 lv) { return std::sqrt(lv.x * lv.x + lv.y * lv.y + lv.z * lv.z); }

}}}
