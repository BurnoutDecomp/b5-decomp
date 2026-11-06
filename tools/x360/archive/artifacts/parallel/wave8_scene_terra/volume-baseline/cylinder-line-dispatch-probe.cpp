#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/CylinderVolume.hpp"
namespace rw { namespace collision {
RwBool CylinderVolume::LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2,
                                        const Matrix44Affine* lpTransform,
                                        VolumeLineSegIntersectResult& arResult, f32 afFatness) const
{
    if (mfRadius + afFatness != 0.0f)
        return FatLineSegIntersect(arPt1, arPt2, lpTransform, arResult, afFatness);
    return ThinLineSegIntersect(arPt1, arPt2, lpTransform, arResult);
}
} }
