#include "SDKs/EATech/rwcollision/volume_debug_access.h"
namespace rw { namespace collision { struct SphereVolume : public Volume { Vector3 GetBBoxDiag() const; };
Vector3 SphereVolume::GetBBoxDiag() const
{
    const __vector4 lRadius = __vspltw(__lvlx(&mfRadius, 0), 0);
    const __vector4 lTwo = __vcfsx(__vspltisw(2), 0);
    return Vector3(__vmulfp(lTwo, lRadius));
}
} }
