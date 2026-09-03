#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>   // std::acos (XMVectorACos), std::fabs, std::isfinite (RwMath::IsValid)

// BrnAIUtils partfile -- the two planar-angle helpers the AIDriver steering chain calls
// (CalculateSteeringAngle / CorneringTopSpeed / DetermineDriftSteeringAngle / GetQuickTurnSteering).
//
//   BrnAI::FindUnsignedAngleBetween2DVectors @0x82766B20  (IDA export; DWARF BrnAIUtils.cpp:19)
//   BrnAI::FindSignedAngleBetween2DVectors   @0x827716A8  (NO IDA export -- the .json is absent
//        and names.tsv lacks the row; the body below is read straight from the image bytes
//        0x827716A8..0x827717FC, disassembled with capstone; it is the only caller-attested
//        symbol at that address: CalculateSteeringAngle bl 0x827716A8.)
//
// Both take their Vector2 args in v1/v2 and return in f1. XMVectorACos is the XNA-math vector
// arccos polynomial; the host uses std::acos (same function, ulp-level differences only).

namespace BrnAI
{
    f32 FindUnsignedAngleBetween2DVectors(Vector2 lA, Vector2 lB)
    {
        // vmulfp128 v0=v1*v2 ; vspltw/vaddfp lanes 0+1 -> dot over (x,y) only.
        const f32 lfDot = lA.x * lB.x + lA.y * lB.y;

        // fabs(dot) >= 1.0 -> return 0.0 (@0x82766B5C..B74). NOT a clamp to acos(+-1).
        if (std::fabs(lfDot) >= 1.0f)
            return 0.0f;

        return std::acos(lfDot);   // XMVectorACos @0x82766B84, lane 0 out
    }

    f32 FindSignedAngleBetween2DVectors(Vector2 lA, Vector2 lB)
    {
        f32 lfAngle = FindUnsignedAngleBetween2DVectors(lA, lB);   // bl @0x827716C8 -> f31

        // fcmpu f31, 0.0 ; beq -> return the (zero) angle unchanged (@0x827716D8/DC).
        if (lfAngle == 0.0f)
            return lfAngle;

        // 2D cross product a.x*b.y - a.y*b.x (@0x827716F8..1718: splat a.y / b.x / b.y / a.x,
        // vmulfp a.y*b.x, vmulfp a.x*b.y, vsubfp) ; assert it is not NaN (BrnAIUtils.cpp:65).
        const f32 lfCross = lA.x * lB.y - lA.y * lB.x;
        CGS_ASSERT(std::isfinite(lfCross), "NAN error in AIDriver::FindSignedAngleBetween2DVectors");

        // @0x8277176C..17B0: sign = (0 > cross) ? +1 : (0 >= cross) ? 0 : -1  (vcmpgtfp /
        // vcmpgefp / two vsel), then angle = sign * unsigned.
        f32 lfSign;
        if (lfCross < 0.0f)
            lfSign = 1.0f;
        else if (lfCross > 0.0f)
            lfSign = -1.0f;
        else
            lfSign = 0.0f;
        lfAngle = lfSign * lfAngle;

        // assert the signed result is not NaN (BrnAIUtils.cpp:70).
        CGS_ASSERT(std::isfinite(lfAngle), "NAN error in AIDriver::FindSignedAngleBetween2DVectors");
        return lfAngle;
    }
}
