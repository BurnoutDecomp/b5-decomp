#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"
#include <xtl.h>

namespace CgsGeometric
{
    // 825B27F8 returns all four original lanes through the member sret buffer.
    Vector3 Sphere::GetPosition() const
    {
        return Vector3(mData.mV);
    }

    // 825BD1F8 broadcasts the W lane through the member sret buffer.
    VecFloat Sphere::GetRadius() const
    {
        return VecFloat(__vspltw(mData.mV, 3));
    }
}
