#include "vendor/renderware/collision/GPInstance.hpp"
namespace rw { namespace collision {
void GPBox::GetInterval(math::vpu::Vector3 arDir, Interval& arInterval) const
{
    const __vector4 centre = __vmsum3fp(mPos.mV, arDir.mV);
    arInterval.max.mV = centre;
    arInterval.min.mV = centre;
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 x = __vandc(__vmulfp(__vspltw(mDimensions.mV, 0), __vmsum3fp(mFaceNormals[0].mV, arDir.mV)), sign);
    const __vector4 y = __vandc(__vmulfp(__vspltw(mDimensions.mV, 1), __vmsum3fp(mFaceNormals[1].mV, arDir.mV)), sign);
    const __vector4 z = __vandc(__vmulfp(__vspltw(mDimensions.mV, 2), __vmsum3fp(mFaceNormals[2].mV, arDir.mV)), sign);
    const __vector4 extent = __vaddfp(__vaddfp(x, y), z);
    arInterval.min.mV = __vsubfp(centre, extent);
    arInterval.max.mV = __vaddfp(centre, extent);
}
void GPBox::GetIntervals(const math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    const __vector4 x = __vmulfp(mFaceNormals[0].mV, __vspltw(mDimensions.mV, 0));
    const __vector4 y = __vmulfp(mFaceNormals[1].mV, __vspltw(mDimensions.mV, 1));
    const __vector4 z = __vmulfp(mFaceNormals[2].mV, __vspltw(mDimensions.mV, 2));
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    for (u32 i = 0; i < auNumDirs; ++i)
    {
        const __vector4 direction = lapDirs[i].mV;
        const __vector4 projection = __vmsum3fp(direction, mPos.mV);
        const __vector4 px = __vandc(__vmsum3fp(x, direction), sign);
        const __vector4 py = __vandc(__vmsum3fp(y, direction), sign);
        const __vector4 pz = __vandc(__vmsum3fp(z, direction), sign);
        const __vector4 extent = __vaddfp(__vaddfp(px, py), pz);
        lapIntervals[i].min.mV = __vsubfp(projection, extent);
        lapIntervals[i].max.mV = __vaddfp(projection, extent);
    }
}
} }
