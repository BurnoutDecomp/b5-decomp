// FX-GATE (crash parity 2026-09-25) -- run_fxgate_slerp.py: XboxMath::XMVectorATan (X360 0x821F0A70) and
// rw::math::vpu::SLerp (X360 0x82216858) against the console's words run on emu64 (FxGateSLerpData.h): XMVectorATan on
// 600 inputs (edges and ranges), SLerp WHOLE on 246 frame pairs (every arm: the endpoints and a NaN amount, identical
// frames, the 2-degree edge, arcs, half turns with the degenerate axis, general rows, non-finite rows), comparing all
// sixteen result lanes and the four angle-out lanes. A word compares equal when the bits match, or when both are NaN.
// Built with /DFXGATE_ATAN_AS_STD the ATan section calls std::atan instead (to show the two differ).
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "SDKs/XboxMath/XMVectorATan.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FxGateSLerpData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(unsigned luA, unsigned luB) { return luA == luB || (IsNaNBits(luA) && IsNaNBits(luB)); }

    rw::math::vpu::Vector3 Vec(const unsigned* lpu)
    {
        rw::math::vpu::Vector3 lv;
        lv.x = Float(lpu[0]); lv.y = Float(lpu[1]); lv.z = Float(lpu[2]); lv.w = Float(lpu[3]);
        return lv;
    }
    rw::math::vpu::Matrix44Affine Mat(const unsigned* lpu)
    {
        rw::math::vpu::Matrix44Affine lm;
        lm.xAxis = Vec(lpu); lm.yAxis = Vec(lpu + 4); lm.zAxis = Vec(lpu + 8); lm.wAxis = Vec(lpu + 12);
        return lm;
    }
}

int main()
{
    unsigned luChecks = 0, luFailures = 0, luPrinted = 0;
    for (const ATanRow& lrRow : kaATanRows)
    {
#ifdef FXGATE_ATAN_AS_STD
        const float lfResult = std::atan(Float(lrRow.muIn));
#else
        const float lfResult = XboxMath::XMVectorATan(Float(lrRow.muIn));
#endif
        ++luChecks;
        if (!Same(Bits(lfResult), lrRow.muOut))
        {
            ++luFailures;
            if (luPrinted++ < 5)
                std::printf("FAIL  XMVectorATan(%08X) = %08X, the console has %08X\n", lrRow.muIn, Bits(lfResult), lrRow.muOut);
        }
    }
    luPrinted = 0;
    unsigned luSLerpFailures = 0;
    for (const SLerpRow& lrRow : kaSLerpRows)
    {
        rw::math::vpu::Vector3 lAngle = { Float(0xDEADBEEFu), Float(0xDEADBEEFu), Float(0xDEADBEEFu), Float(0xDEADBEEFu) };
        const rw::math::vpu::Matrix44Affine lResult =
            rw::math::vpu::SLerp(Mat(lrRow.mauFrom), Mat(lrRow.mauTo), Float(lrRow.muAmount), &lAngle);
        const rw::math::vpu::Vector3* lapRows[5] = { &lResult.xAxis, &lResult.yAxis, &lResult.zAxis, &lResult.wAxis, &lAngle };
        bool lbOk = true;
        for (int liRow = 0; liRow < 5; ++liRow)
        {
            const unsigned lau[4] = { Bits(lapRows[liRow]->x), Bits(lapRows[liRow]->y), Bits(lapRows[liRow]->z), Bits(lapRows[liRow]->w) };
            for (int liLane = 0; liLane < 4; ++liLane)
            {
                if (!Same(lau[liLane], lrRow.mauOut[4 * liRow + liLane]))
                {
                    if (lbOk && luPrinted++ < 8)
                        std::printf("FAIL  SLerp (%s, amount %08X): %s.%c = %08X, the console has %08X\n", lrRow.mpcLabel,
                                    lrRow.muAmount, (liRow < 4) ? "row" : "angle", "xyzw"[liLane], lau[liLane],
                                    lrRow.mauOut[4 * liRow + liLane]);
                    lbOk = false;
                }
            }
        }
        ++luChecks;
        if (!lbOk)
        {
            ++luFailures;
            ++luSLerpFailures;
        }
    }
    std::printf("SLerp rows failing: %u of %u\n", luSLerpFailures, static_cast<unsigned>(sizeof(kaSLerpRows) / sizeof(kaSLerpRows[0])));
    std::printf("FxGateSLerp: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
