#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include "vendor/renderware/collision/AABBox.hpp"
namespace rw { namespace collision { struct SphereVolume : public Volume { RwBool GetBBox(const Matrix44Affine*,RwBool,AABBox&) const; }; struct BoxVolume : public Volume { RwBool GetBBox(const Matrix44Affine*,RwBool,AABBox&) const; };
RwBool SphereVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 centre = mTransform.wAxis.mV;
    if (lpTransform)
    {
        __vector4 result = __vmaddfp(__vspltw(centre, 0), lpTransform->xAxis.mV, lpTransform->wAxis.mV);
        result = __vmaddfp(__vspltw(centre, 1), lpTransform->yAxis.mV, result);
        centre = __vmaddfp(__vspltw(centre, 2), lpTransform->zAxis.mV, result);
    }
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    arResult.mMin.mV = __vsubfp(centre, radius);
    arResult.mMax.mV = __vaddfp(centre, radius);
    return 1;
}
RwBool BoxVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 lx = mTransform.xAxis.mV;
    __vector4 ly = mTransform.yAxis.mV;
    __vector4 lz = mTransform.zAxis.mV;
    __vector4 lw = mTransform.wAxis.mV;
    if (lpTransform)
    {
        const __vector4 tx = lpTransform->xAxis.mV;
        const __vector4 ty = lpTransform->yAxis.mV;
        const __vector4 tz = lpTransform->zAxis.mV;
        const __vector4 tw = lpTransform->wAxis.mV;
        __vector4 x = __vmulfp(__vspltw(lx, 0), tx);
        __vector4 y = __vmulfp(__vspltw(ly, 0), tx);
        __vector4 z = __vmulfp(__vspltw(lz, 0), tx);
        __vector4 w = __vmaddfp(__vspltw(lw, 0), tx, tw);
        x = __vmaddfp(__vspltw(lx, 1), ty, x);
        y = __vmaddfp(__vspltw(ly, 1), ty, y);
        z = __vmaddfp(__vspltw(lz, 1), ty, z);
        w = __vmaddfp(__vspltw(lw, 1), ty, w);
        lx = __vmaddfp(__vspltw(lx, 2), tz, x);
        ly = __vmaddfp(__vspltw(ly, 2), tz, y);
        lz = __vmaddfp(__vspltw(lz, 2), tz, z);
        lw = __vmaddfp(__vspltw(lw, 2), tz, w);
    }
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 hx = __vspltw(__lvlx(&mBoxData.mfHx, 0), 0);
    const __vector4 hy = __vspltw(__lvlx(&mBoxData.mfHy, 0), 0);
    const __vector4 hz = __vspltw(__lvlx(&mBoxData.mfHz, 0), 0);
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    __vector4 extent = __vmulfp(__vandc(ly, sign), hy);
    extent = __vmaddfp(__vandc(lx, sign), hx, extent);
    extent = __vmaddfp(__vandc(lz, sign), hz, extent);
    extent = __vaddfp(extent, radius);
    arResult.mMin.mV = __vsubfp(lw, extent);
    arResult.mMax.mV = __vaddfp(lw, extent);
    return 1;
}
} }
