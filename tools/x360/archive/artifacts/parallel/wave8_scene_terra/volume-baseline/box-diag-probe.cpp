#include "SDKs/EATech/rwcollision/volume_debug_access.h"
namespace rw { namespace collision { struct BoxVolume : public Volume { Vector3 GetBBoxDiag() const; };
Vector3 BoxVolume::GetBBoxDiag() const
{
    const __vector4 lOnes = __vspltisw(-1);
    const __vector4 lSign = __vslw(lOnes, lOnes);
    const __vector4 lX = __vandc(mTransform.xAxis.mV, lSign);
    const __vector4 lY = __vandc(mTransform.yAxis.mV, lSign);
    const __vector4 lZ = __vandc(mTransform.zAxis.mV, lSign);
    const __vector4 lHx = __vspltw(__lvlx(&mBoxData.mfHx, 0), 0);
    const __vector4 lHy = __vspltw(__lvlx(&mBoxData.mfHy, 0), 0);
    const __vector4 lHz = __vspltw(__lvlx(&mBoxData.mfHz, 0), 0);
    const __vector4 lRadius = __vspltw(__lvlx(&mfRadius, 0), 0);
    __vector4 lExtent = __vmulfp(lY, lHy);
    lExtent = __vmaddfp(lX, lHx, lExtent);
    lExtent = __vmaddfp(lZ, lHz, lExtent);
    lExtent = __vaddfp(lExtent, lRadius);
    return Vector3(__vmulfp(lExtent, __vcfsx(__vspltisw(2), 0)));
}
} }
