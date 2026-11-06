#include "vendor/renderware/collision/GPInstance.hpp"
namespace rw { namespace collision {
void GPTriangle::GetInterval(math::vpu::Vector3 arDir, Interval& arInterval) const
{
    const __vector4 p0 = __vmsum3fp(arDir.mV, mPos.mV);
    const __vector4 p1 = __vmsum3fp(arDir.mV, mFaceNormals[1].mV);
    const __vector4 p2 = __vmsum3fp(arDir.mV, mFaceNormals[2].mV);
    arInterval.min.mV = __vminfp(__vminfp(p0, p1), p2);
    arInterval.max.mV = __vmaxfp(__vmaxfp(p0, p1), p2);
}
void GPTriangle::GetIntervals(const math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const
{
    for (u32 i = 0; i < auNumDirs; ++i)
    {
        const __vector4 p0 = __vmsum3fp(lapDirs[i].mV, mPos.mV);
        const __vector4 p1 = __vmsum3fp(lapDirs[i].mV, mFaceNormals[1].mV);
        const __vector4 p2 = __vmsum3fp(lapDirs[i].mV, mFaceNormals[2].mV);
        lapIntervals[i].min.mV = __vminfp(__vminfp(p0, p1), p2);
        lapIntervals[i].max.mV = __vmaxfp(__vmaxfp(p0, p1), p2);
    }
}
} }
