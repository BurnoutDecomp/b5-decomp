#include "GameSource/World/AI/BrnAIUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "SDKs/XboxMath/XMVectorACos.h"         // XboxMath::XMVectorACos (X360 0x821F0980)

#include <cmath>   // std::fabs
                   // (RwMath::IsValid is a NaN SELF-COMPARE on the console, not isfinite -- see below)

// BrnAIUtils partfile -- the two planar-angle helpers the AIDriver steering chain calls
// (CalculateSteeringAngle / CorneringTopSpeed / DetermineDriftSteeringAngle / GetQuickTurnSteering).
//
//   BrnAI::FindUnsignedAngleBetween2DVectors @0x82766B20  (IDA export; DWARF BrnAIUtils.cpp:19)
//   BrnAI::FindSignedAngleBetween2DVectors   @0x827716A8  (NO IDA export -- the .json is absent
//        and names.tsv lacks the row; the body below is read straight from the image bytes
//        0x827716A8..0x827717FC, disassembled with capstone; it is the only caller-attested
//        symbol at that address: CalculateSteeringAngle bl 0x827716A8.)
//
// Both take their Vector2 args in v1/v2 and return in f1. XMVectorACos is the XDK math library's
// arccos polynomial (not a correctly rounded acos: XMVectorACos(1) = 4.8e-7), written out in the
// shared XboxMath::XMVectorACos (crash parity FX-GATE; std::acos stood in for it before).

namespace BrnAI
{
    f32 FindUnsignedAngleBetween2DVectors(Vector2 lA, Vector2 lB)
    {
        // vmulfp128 v0=v1*v2 ; vspltw/vaddfp lanes 0+1 -> dot over (x,y) only.
        const f32 lfDot = lA.x * lB.x + lA.y * lB.y;

        // ⛔⛔ ISSUE #31 -- THE BRANCH SENSE FOR THE **UNORDERED** CASE WAS INVERTED, AND THAT ONE
        // BIT IS THE WHOLE ASSERT STORM. This used to read `if (fabs(dot) >= 1.0f) return 0.0f;`,
        // which is the same thing as the console's test ONLY while `dot` is a number. The console:
        //   0x82766B4C  fabs   f13, f0                 ; f13 = |dot|
        //   0x82766B54  lfs    f0, flt_82001C98        ; f0  = 1.0
        //   0x82766B58  fcmpu  cr6, f13, f0
        //   0x82766B5C  blt    cr6, loc_82766B78       ; ONLY a strict, ORDERED less-than reaches
        //   0x82766B60  lfs    f1, flt_82001CC0        ;   XMVectorACos; everything else...
        //   0x82766B74  blr                            ;   ...returns 0.0f
        // `fcmpu` raises the UNORDERED bit (CR[FU]) for a NaN operand and leaves LT/GT/EQ CLEAR, so
        // `blt` is NOT taken and ARTIST **returns 0.0f for a NaN dot**. `!(|dot| >= 1.0f)` is TRUE
        // for that same NaN, so this file used to fall through into `std::acos(NaN) == NaN` --
        // the exact opposite answer, and the only place in the chain where the NaN survived.
        // Consequence of the old sense (the reported bug): FindSignedAngleBetween2DVectors got a
        // NaN angle instead of 0.0, so it missed its `fcmpu f31,0.0 ; beq` early-out @0x827716D8,
        // computed the 2D cross, fired BOTH of its NaN asserts, and handed NaN to
        // SteeringFan::GenerateFanVectors -- NaN mUnitDirection/mTarget/mCentreTarget, hence the
        // GetSectionInterpPosition (:2616) / GetIterativeHermite (:1849) / GetSimpleHermite (:1808,
        // :1826) / :1899 / :1906 cascade in the issue's log. With the console's sense the base angle
        // is 0.0, every fan ray stays finite and none of those asserts is reachable.
        // Written the way the asm branches so the unordered case cannot be re-inverted by rewriting
        // the condition: acos is reached ONLY on an ordered `|dot| < 1.0`.
        if (std::fabs(lfDot) < 1.0f)
            return XboxMath::XMVectorACos(lfDot);   // bl XMVectorACos @0x82766B84, lane 0 out

        return 0.0f;                   // @0x82766B60..B74 (flt_82001CC0 == 0.0) -- and the NaN arm
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
        // ⛔ PREDICATE FIXED (issue #31): this read `std::isfinite(lfCross)`, which ALSO rejects
        // +-Inf. The console's test is a NaN SELF-COMPARE and nothing else --
        //   0x8277172C vspltw v0,v0,0 ; 0x82771730 vcmpeqfp. v0,v0,v0 ; 0x82771734 mfocrf ;
        //   0x82771750 bne -> skip ; else BeginAssert/FireAssert(li r5,0x41 == :65)/EndAssert
        // -- and `Inf == Inf` is TRUE, so an infinite cross product does NOT assert on ARTIST.
        // `isfinite` was a stricter predicate than the binary has, i.e. an assert we invented.
        CGS_ASSERT(lfCross == lfCross, "NAN error in AIDriver::FindSignedAngleBetween2DVectors");

        // The sign is two vector selects over two masks, (cross > 0) and (cross >= 0):
        //   sign = (cross >= 0) ? ((cross > 0) ? +1.0 : 0.0) : -1.0
        // POSITIVE when cross(lA,lB) = lA.x*lB.y - lA.y*lB.x > 0. Both masks are clear for a NaN
        // cross, so a NaN takes the -1.0 arm (not 0.0): written as the selects so the unordered
        // case keeps that arm.
        const f32 lfPositiveSign = (lfCross > 0.0f) ? 1.0f : 0.0f;
        const f32 lfSign        = (lfCross >= 0.0f) ? lfPositiveSign : -1.0f;
        lfAngle = lfSign * lfAngle;

        // assert the signed result is not NaN (BrnAIUtils.cpp:70) -- the same self-compare, at
        // 0x827717BC vspltw / 0x827717C0 vcmpeqfp. v0,v0,v0 / 0x827717D8 li r5,0x46 (== :70).
        CGS_ASSERT(lfAngle == lfAngle, "NAN error in AIDriver::FindSignedAngleBetween2DVectors");
        return lfAngle;
    }
}
