// FX-AIBUZZ item 1 (crash parity 2026-09-24): BrnAI::BuzzBy::MaintainAheadOrBehind @0x82766C40, run on the
// PRODUCTION body (extracted from BrnAIBuzzBy.cpp by run_fxaibuzz_maintain.py, with the file's two .bss speeds
// and its console IsZero helper) through the REAL ResetOnTrackRequest::Construct (BrnAIModuleRequestInterface.cpp,
// compiled in whole), against expectations read off the ARTIST asm:
//   rel = lPosition - lPlayerPosition                               vsubfp v0, v1, v3        @0x82766C40
//   IsZero(rel): no |x|,|y|,|z| above flt_820C3B70 (FLT_EPSILON)     vcmpgtfp. + CR6 all-false @0x82766C74..84
//   else rel = rel * rsqrt(|rel|^2)                                 0x82766C88..0x82766CC4
//   Dot(rel, playerDir) > 0.0 (flt_82001CC0)?                       vcmpgtfp. all-true, beq   @0x82766CF8..0x82766D0C
//     AHEAD : type = (Dot(dir, playerDir) > 0.0) | 4                ori r9, r10, 4           @0x82766D4C
//             speed = flt_8300DBEC, distance = flt_820C4318 (200.0)
//     BEHIND: type = 3, distance = flt_820C431C (-60.0)
//             speed = |playerVel| (vcmpeqfp/vsel -> 0 for zero) + flt_8300D7F4     vaddfp @0x82766DE8
//   +0 (the race car index) = 0 in both arms                        stw r11(0), 0(r3)
// flt_8300DBEC / flt_8300D7F4 are .bss slots the CRT fills at start-up: thunk 0x82C68EE8 = flt_82004A18 (80.0) *
// flt_82F31928 (0x3EE4E26D, 0.44704) and thunk 0x82C68EC8 = flt_820C4870 (25.0) * 0.44704 -- single-precision
// products 0x420F0D84 (35.7632) and 0x4132D0E5 (11.176), checked bit for bit.
// The PC had no body (declaration only), so PlaceRaceCarOnLoad parked on it: every numeric check here fails on
// the old tree because the numeric side cannot even be built.
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("  [assert] %s\n", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnAI
{
// The production constants, the console IsZero helper and the body, extracted verbatim.
#include "fxaibuzz_maintain.inc"
}

using BrnAI::AIModuleIO::ResetOnTrackRequest;

namespace
{
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++gChecks;
        if (!lbPass) { ++gFailures; std::printf("FAIL %s\n", lpcLabel); }
    }

    u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, sizeof(lu)); return lu; }
    f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, sizeof(lf)); return lf; }

    // The console's values (x360rd / the CRT thunks), not the production file's.
    const f32 KF_CONSOLE_ON_COMING_SPEED = FromBits(0x420F0D84u);   // flt_8300DBEC = 80 * 0.44704
    const f32 KF_CONSOLE_FASTER          = FromBits(0x4132D0E5u);   // flt_8300D7F4 = 25 * 0.44704

    Vector3 V(f32 lfX, f32 lfY, f32 lfZ, f32 lfW = 0.0f) { return Vector3{ lfX, lfY, lfZ, lfW }; }

    ResetOnTrackRequest Run(Vector3 lCarPosition, Vector3 lCarDirection, Vector3 lPlayerPosition,
                            Vector3 lPlayerVelocity, Vector3 lPlayerDirection)
    {
        ResetOnTrackRequest lRequest;
        std::memset(&lRequest, 0xCD, sizeof(lRequest));   // the caller's stack slot (var_B0) is not initialised
        BrnAI::BuzzBy::MaintainAheadOrBehind(&lRequest, lCarPosition, lCarDirection,
                                             lPlayerPosition, lPlayerVelocity, lPlayerDirection);
        return lRequest;
    }

    // One request against the console's four stores.
    void Expect(const char* lpcCase, const ResetOnTrackRequest& lrRequest, s32 liType, f32 lfSpeed, f32 lfDistance)
    {
        char lacLabel[256];
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: type %d (got %d)", lpcCase, liType,
                      static_cast<s32>(lrRequest.meResetType));
        Check(static_cast<s32>(lrRequest.meResetType) == liType, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: speed %.9g (got %.9g)", lpcCase, lfSpeed,
                      lrRequest.mfResetSpeed);
        Check(Bits(lrRequest.mfResetSpeed) == Bits(lfSpeed), lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: distance %.9g (got %.9g)", lpcCase, lfDistance,
                      lrRequest.mfResetDistance);
        Check(Bits(lrRequest.mfResetDistance) == Bits(lfDistance), lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: race car index 0 in +0 (got %d)", lpcCase,
                      static_cast<s32>(lrRequest.meGlobalRaceCarIndex));
        Check(static_cast<s32>(lrRequest.meGlobalRaceCarIndex) == 0, lacLabel);
    }
}

int main()
{
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    // ---- the file's two .bss speeds are the console's single-precision products ----------------------------
    Check(Bits(BrnAI::KF_ON_COMING_RESET_SPEED) == 0x420F0D84u,
          "KF_ON_COMING_RESET_SPEED == flt_8300DBEC = 80 * 0.44704 (thunk 0x82C68EE8), not the 200.0 distance");
    Check(Bits(BrnAI::KF_FASTER_THAN_PLAYER) == 0x4132D0E5u,
          "KF_FASTER_THAN_PLAYER == flt_8300D7F4 = 25 * 0.44704 (thunk 0x82C68EC8)");

    // Player at (100, 0, 200) heading +Z at 20 m/s.
    const Vector3 lPlayerPosition  = V(100.0f, 0.0f, 200.0f);
    const Vector3 lPlayerVelocity  = V(0.0f, 0.0f, 20.0f);
    const Vector3 lPlayerDirection = V(0.0f, 0.0f, 1.0f);

    // ---- AHEAD (Dot(rel, playerDir) > 0) -------------------------------------------------------------------
    Expect("ahead, same heading -> 4 | 1 (FROM_TURNINGS_ROAD_RAGE)",
           Run(V(100.0f, 0.0f, 300.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           5, KF_CONSOLE_ON_COMING_SPEED, 200.0f);
    Expect("ahead, facing the player -> 4 | 0 (AHEAD_PLAYER_ON_COMING)",
           Run(V(100.0f, 0.0f, 300.0f), V(0.0f, 0.0f, -1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           4, KF_CONSOLE_ON_COMING_SPEED, 200.0f);
    Expect("ahead, crossing (heading dot exactly 0 is not > 0) -> 4",
           Run(V(130.0f, 5.0f, 260.0f), V(1.0f, 0.0f, 0.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           4, KF_CONSOLE_ON_COMING_SPEED, 200.0f);
    Expect("ahead with a NaN car heading -> 4 (vcmpgtfp false)",
           Run(V(100.0f, 0.0f, 300.0f), V(KF_NAN, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           4, KF_CONSOLE_ON_COMING_SPEED, 200.0f);
    // A player-relative offset under FLT_EPSILON is NOT normalised (the IsZero screen skips the rsqrt) and its
    // raw Dot decides: 1e-23 along the heading is > 0 -> AHEAD. (Squared it underflows to 0 in single, so a
    // normalise that returned the zero vector for it would have said BEHIND.)
    Expect("sub-epsilon offset along the heading is not normalised and still reads AHEAD",
           Run(V(0.0f, 0.0f, 1.0e-23f), V(0.0f, 0.0f, 1.0f), V(0.0f, 0.0f, 0.0f), lPlayerVelocity,
               lPlayerDirection),
           5, KF_CONSOLE_ON_COMING_SPEED, 200.0f);
    // Only the xyz lanes take part (vmsum3fp128): w garbage on every input changes nothing.
    Expect("ahead with w-lane garbage on every input (vmsum3fp128 is xyz only)",
           Run(V(100.0f, 0.0f, 300.0f, 7.0f), V(0.0f, 0.0f, 1.0f, -9.0f), V(100.0f, 0.0f, 200.0f, 3.0f),
               V(0.0f, 0.0f, 20.0f, 1.0e30f), V(0.0f, 0.0f, 1.0f, 1.0e30f)),
           5, KF_CONSOLE_ON_COMING_SPEED, 200.0f);

    // ---- BEHIND (everything else) --------------------------------------------------------------------------
    Expect("behind -> 3, |v| + flt_8300D7F4, -60",
           Run(V(100.0f, 0.0f, 100.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           3, 20.0f + KF_CONSOLE_FASTER, -60.0f);
    Expect("level with the player (projection exactly 0) -> BEHIND",
           Run(V(150.0f, 0.0f, 200.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           3, 20.0f + KF_CONSOLE_FASTER, -60.0f);
    Expect("on top of the player (zero offset: IsZero, Dot 0) -> BEHIND",
           Run(lPlayerPosition, V(0.0f, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity, lPlayerDirection),
           3, 20.0f + KF_CONSOLE_FASTER, -60.0f);
    Expect("|(3, 4, 12)| = 13 over all three lanes, w ignored",
           Run(V(90.0f, 2.0f, 150.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, V(3.0f, 4.0f, 12.0f, 100.0f),
               lPlayerDirection),
           3, 13.0f + KF_CONSOLE_FASTER, -60.0f);
    Expect("a stopped player: Magnitude's vsel gives 0 -> exactly flt_8300D7F4",
           Run(V(100.0f, 0.0f, 100.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, V(0.0f, 0.0f, 0.0f),
               lPlayerDirection),
           3, KF_CONSOLE_FASTER, -60.0f);
    Expect("a NaN player heading fails `> 0` -> BEHIND",
           Run(V(100.0f, 0.0f, 300.0f), V(0.0f, 0.0f, 1.0f), lPlayerPosition, lPlayerVelocity,
               V(KF_NAN, 0.0f, 1.0f)),
           3, 20.0f + KF_CONSOLE_FASTER, -60.0f);

    Check(gAsserts == 0, "no assertion on any of these (the inlined Construct's index is 0)");

    std::printf("FxAiBuzzMaintain: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
