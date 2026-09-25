// FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update's four rotation builders, EXTRACTED from the
// production source by run_fxgate_aftertouch_sincos.py (fxgate_aftertouch_sincos.inc), against the console's own
// words run on emu64 (FxGateAftertouchSinCosData.h): the three inlined XMVectorSinCos blocks (random start, pitch,
// roll) and XMMatrixRotationY @0x82203560 (XMScalarSinCos). The console's w lanes of the start direction and the roll
// rows carry copies that Mult never reads, so those lanes are not compared.
#include "types.hpp"
#include "SDKs/XboxMath/XMVectorSinCos.h"
#include "SDKs/XboxMath/XMScalarSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    struct Vector3 { f32 x, y, z, w; };
    struct Matrix44Affine { Vector3 xAxis, yAxis, zAxis, wAxis; };

#include "fxgate_aftertouch_sincos.inc"   // RotationYAxisZ / RotationX / RotationZ / XMMatrixRotationY
}

#include "FxGateAftertouchSinCosData.h"

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }

    void Lanes(const Vector3& lrVector, unsigned* lpuOut)
    {
        lpuOut[0] = Bits(lrVector.x); lpuOut[1] = Bits(lrVector.y); lpuOut[2] = Bits(lrVector.z); lpuOut[3] = Bits(lrVector.w);
    }

    // Compare lanes [first, first + count) of a 4-lane vector with the golden lanes starting at lpuGolden.
    bool Match(const Vector3& lrVector, const unsigned* lpuGolden, int liCount)
    {
        unsigned lau[4];
        Lanes(lrVector, lau);
        for (int liLane = 0; liLane < liCount; ++liLane)
            if (lau[liLane] != lpuGolden[liLane])
                return false;
        return true;
    }

    void Verdict(const char* lpcSite, unsigned luAngle, bool lbOk, unsigned& lruPrinted)
    {
        ++gChecks;
        if (lbOk)
            return;
        ++gFailures;
        if (lruPrinted++ < 4)
            std::printf("FAIL  %s angle 0x%08X (%.9g)\n", lpcSite, luAngle, Float(luAngle));
    }
}

int main()
{
    unsigned luPrinted = 0;
    for (const AtcRow3& lrRow : kaStartRows)
    {
        const Vector3 lvDirection = RotationYAxisZ(Float(lrRow.muAngle));
        Verdict("start (sin, 0, cos)", lrRow.muAngle, Match(lvDirection, lrRow.mauLanes, 3), luPrinted);
    }
    luPrinted = 0;
    for (const AtcRow8& lrRow : kaPitchRows)
    {
        const Matrix44Affine lMatrix = RotationX(Float(lrRow.muAngle));
        const unsigned kauRow0[4] = { 0x3F800000u, 0u, 0u, 0u };
        const unsigned kauZero[4] = { 0u, 0u, 0u, 0u };
        Verdict("pitch rows", lrRow.muAngle,
                Match(lMatrix.xAxis, kauRow0, 4) && Match(lMatrix.yAxis, lrRow.mauLanes, 4)
                && Match(lMatrix.zAxis, lrRow.mauLanes + 4, 4) && Match(lMatrix.wAxis, kauZero, 4), luPrinted);
    }
    luPrinted = 0;
    for (const AtcRow12& lrRow : kaRollRows)
    {
        const Matrix44Affine lMatrix = RotationZ(Float(lrRow.muAngle));
        Verdict("roll rows (x, y, z)", lrRow.muAngle,
                Match(lMatrix.xAxis, lrRow.mauLanes, 3) && Match(lMatrix.yAxis, lrRow.mauLanes + 4, 3)
                && Match(lMatrix.zAxis, lrRow.mauLanes + 8, 3), luPrinted);
    }
    luPrinted = 0;
    for (const AtcRow16& lrRow : kaOrbitRows)
    {
        const Matrix44Affine lMatrix = XMMatrixRotationY(Float(lrRow.muAngle));
        Verdict("XMMatrixRotationY rows", lrRow.muAngle,
                Match(lMatrix.xAxis, lrRow.mauLanes, 4) && Match(lMatrix.yAxis, lrRow.mauLanes + 4, 4)
                && Match(lMatrix.zAxis, lrRow.mauLanes + 8, 4) && Match(lMatrix.wAxis, lrRow.mauLanes + 12, 4),
                luPrinted);
    }
    std::printf("FxGateAftertouchSinCos: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
