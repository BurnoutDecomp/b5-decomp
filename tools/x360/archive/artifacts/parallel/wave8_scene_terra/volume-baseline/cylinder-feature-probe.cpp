#include "vendor/renderware/collision/CylinderVolume.hpp"
#include <cstring>
namespace rw { namespace collision {
RwBool CylinderVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vector3& /*arDir*/, Feature& arResult) const
{
    const __vector4 axis = mTransform.yAxis.mV;
    const __vector4 centre = mTransform.wAxis.mV;
    const f32 halfHeight = mCylinderData.mfHalfHeight;
    const __vector4 height = __vspltw(__lvlx(&halfHeight, 0), 0);
    const Vector3 plus(__vmaddfp(axis, height, centre));
    const Vector3 minus(__vsubfp(centre, __vmulfp(axis, height)));
    const FeatureEdge edge(plus, minus);
    memcpy(&arResult.edges[0], &edge, sizeof(edge));
    arResult.numedges = 1;
    return 1;
}
} }
