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

        // ⛔ SIGN FIXED 2026-09-04 (aiwave R7): this used to read `sign = (0 > cross) ? +1 : ...`,
        // i.e. -signum(cross) -- the compare operands were transposed, so every steering error fed
        // to the PID had the WRONG SIGN and the controller drove the car AWAY from its target and
        // wound up against the +-1 clamp. The image bytes settle it (re-disassembled word by word
        // from 0x8277176C, capstone + manual VMX128 decode):
        //   0x8277176C  vspltisw v13,1              0x82771774  vspltisw v0,0
        //   0x82771780  vcfsx    v12,v13,0          -> v12 = +1.0
        //   0x82771784  lvx      v13,r0,r11         -> v13 = the cross product (splatted below)
        //   0x82771788  vmr      v11,v0             -> v11 =  0.0
        //   0x8277178C  vspltw   v13,v13,0
        //   0x82771794  vcmpgtfp v10,v13,v0         -> v10 = (cross >  0)      [vA=cross, vB=0]
        //   0x82771798  vcmpgefp v13,v13,v0         -> v13 = (cross >= 0)
        //   0x8277179C  vsubfp   v0,v0,v12          -> v0  = -1.0
        //   0x827717A0  vsel     v12,v11,v12,v10    -> (cross >  0) ? +1.0 : 0.0
        //   0x827717A4  vsel     v0,v0,v12,v13      -> (cross >= 0) ? that   : -1.0
        //   0x827717B0  fmuls    f31,f0,f31         -> angle = sign * unsigned
        // (vsel vD,vA,vB,vC selects vB where the mask vC is set, vA where it is clear.)
        // So sign == signum(cross): POSITIVE when cross(lA,lB) = lA.x*lB.y - lA.y*lB.x > 0.
        f32 lfSign;
        if (lfCross > 0.0f)
            lfSign = 1.0f;
        else if (lfCross < 0.0f)
            lfSign = -1.0f;
        else
            lfSign = 0.0f;
        lfAngle = lfSign * lfAngle;

        // assert the signed result is not NaN (BrnAIUtils.cpp:70).
        CGS_ASSERT(std::isfinite(lfAngle), "NAN error in AIDriver::FindSignedAngleBetween2DVectors");
        return lfAngle;
    }
}
