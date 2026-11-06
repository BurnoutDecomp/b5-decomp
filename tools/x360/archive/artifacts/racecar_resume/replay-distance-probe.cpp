#include <vectorintrinsics.h>
extern "C" void ReplayDistanceProbe(const __vector4* pCamera, const __vector4* pPosition, __vector4* pSquared, __vector4* pDistance)
{
    __vector4 d = __vsubfp(*pCamera, *pPosition);
    __vector4 square = __vmsum3fp(d, d);
    __vector4 zero = __vspltisw(0);
    __vector4 one = __vcfsx(__vspltisw(1), 0);
    __vector4 half = __vcfsx(__vspltisw(1), 1);
    __vector4 recip = __vrsqrtefp(square);
    __vector4 mask = __vcmpeqfp(zero, square);
    __vector4 r2 = __vmulfp(recip, recip);
    __vector4 rh = __vmulfp(recip, half);
    __vector4 error = __vnmsubfp(square, r2, one);
    recip = __vmaddfp(rh, error, recip);
    r2 = __vmulfp(recip, recip);
    rh = __vmulfp(recip, half);
    error = __vnmsubfp(square, r2, one);
    recip = __vmaddfp(rh, error, recip);
    *pSquared = square;
    *pDistance = __vsel(__vmulfp(square, recip), zero, mask);
}
