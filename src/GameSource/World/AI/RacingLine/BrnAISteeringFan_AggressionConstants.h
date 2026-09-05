#pragma once

// BrnAI::SteeringFan AGGRESSION TUNABLES -- the file-scope constants the smash / drive-close /
// drift contributors in BrnAISteeringFan_Aggression.cpp read.
//
// TWO of them (KF_SLAM_AHEADNESS, KF_SLAM_FROM_BEHIND_RELATIVE_SPEED) are `.bss` slots that read
// 0x00000000 straight out of the image, because .bss is zero in the image BY DEFINITION -- they
// are written at start-up by the CRT static-initialiser bank at 0x82C67F00..0x82C69500, exactly
// like the ~45 AI tunables GameSource/World/AI/BrnAICar_Constants.h already recovered. Both were
// EVALUATED from their initialiser, so the values below are console-exact, not placeholders.
//
// RECOVERY (repeat for any other silent-zero .bss float; tools/re/findinit.py + tools/re/ppcdis.py):
//   1. `findinit.py <bssaddr>` -> the one site in the 0x82C6xxxx CRT bank that materialises it;
//   2. `ppcdis.py <writer-0x20> 16` -> the leaf, which ends `... ; stfs f0, <bssaddr> ; blr`;
//   3. evaluate the leaf against image.bin (big-endian, file offset == VA - 0x82000000).
//   (On this box artist_i64.raw is absent, so x360rd.py was shimmed onto
//    scratch/postfx_step9_final/envfix/work/image.bin -- same bytes, same decode.)
//
// The other two are `const float32_t` at file scope in the DWARF but the console compiler FOLDED
// them into .rdata literals at their use sites, so there is no .bss slot to read; the derivation
// of each is spelled out on its line.
//
// NAMES come from the DecFIGS DWARF file-scope declarations
// (references/DecFIGS/dwarfdump/GameSource/World/AI/RacingLine/BrnAISteeringFan.cpp:13/:66/:2180/
// :2181); each is matched to its .bss slot / literal by the ROLE the X360 asm gives it, quoted
// on the line.
//
// NOT HERE, deliberately: KF_MAX_SEPERATION_FOR_SLAM (DWARF BrnAISteeringFan.cpp:2015),
// KF_AI_SMASH_SEPERATION (:2076) and KF_PROJECT_AHEAD (:2260) are declared INSIDE their function
// bodies in the DWARF, so they stay function-local `const f32` in the .cpp rather than being
// hoisted into a shared header.

#include "types.hpp"

namespace BrnAI
{
    // ================================================================================
    // BrnAISteeringFan.cpp:66 -- KF_SLAM_AHEADNESS
    //
    // .bss 0x8300D6F8, sole writer 0x82C69348 (findinit.py: exactly one site):
    //     lfd f1, 0x820C8C38            ; the double 1.0471975430846214
    //     bl  0x82C096A0                ; the CRT `cos` kernel -- 0x82F93F00 == pi/2,
    //                                   ; 0x82F93F08 == 1/pi, 0x82F93F24 == 0.5,
    //                                   ; 0x82F93F28/30 == the two-part pi reduction
    //     frsp f0, f1 ; stfs f0, 0x8300D6F8
    // 1.0471975430846214 is EXACTLY 60.0 * float(pi/180), i.e. KF_ANGLE_TO_SLAM_FROM (DWARF :64,
    // .bss 0x8300D6F4, writer 0x82C68B28: <deg-to-rad> * flt_820C4158 == 60) promoted to double.
    // cos(1.0471975430846214) == 0.5000000070251776 -> float == 0.5f exactly.
    //
    // ROLE (FindNeabyAIInTraffic @0x82787DF0): a rival is REJECTED when its relative speed is at
    // or below KF_SLAM_FROM_BEHIND_RELATIVE_SPEED *and* the cosine between the unit vector to it
    // and our useful direction is greater than this -- i.e. do not line up a slam on something
    // dead ahead unless we are actually closing on it.
    const f32 KF_SLAM_AHEADNESS = 0.5f;

    // ================================================================================
    // BrnAISteeringFan.cpp:28 -- KF_SLAM_FROM_BEHIND_RELATIVE_SPEED
    //
    // .bss 0x8300D75C, sole writer 0x82C69310:
    //     lfs f0, flt_82F31928 (0.44704, the mph -> m/s factor)
    //     lfs f13, flt_820C4150 (10.0)
    //     fmuls f0, f0, f13 ; stfs f0, 0x8300D75C          -> 10 mph == 4.4704 m/s
    //
    // ROLE (FindNeabyAIInTraffic @0x82787DE8): compared against
    // `lpCar->GetSpeed() - Magnitude(lpRival->mVelocity)`, our CLOSING speed on the rival.
    const f32 KF_SLAM_FROM_BEHIND_RELATIVE_SPEED = 4.47039986f;

    // ================================================================================
    // BrnAISteeringFan.cpp:2181 -- KF_DESIRED_CLOSE_PASSING_SEPERATION (sic, DWARF spelling)
    //
    // Folded to the .rdata literal flt_820C4880 == 3.5 (metres). IncludeDriveCloseToPlayer
    // @0x82788134/38: `lfs f0, flt_820C4880 ; fmsubs f0, f1, f27, f0` -- the closest-approach
    // distance (signed by which side of the player's line we are on) MINUS this separation is
    // lfPassingSpace, so 3.5 m is the gap the contributor is happy with.
    const f32 KF_DESIRED_CLOSE_PASSING_SEPERATION = 3.5f;

    // ================================================================================
    // BrnAISteeringFan.cpp:2180 -- KF_CLOSE_PASSING_RANGE
    //
    // [INFERRED VALUE -- the image attests the RECIPROCAL.] IncludeDriveCloseToPlayer scales
    // lfPassingSpace by flt_820C4168 == 0.5 on the positive arm (@0x82788144) and by
    // flt_82004C78 == -0.5 on the negative arm (@0x82788160) before clamping to [0,1], so the
    // range over which the weight ramps is 1/0.5 == 2.0 metres. Both literals are shared pooled
    // .rdata constants (the same flt_820C4168 is the plain 0.5 in GenerateFanVectors), so the
    // image cannot distinguish "0.5f literal" from "1/KF_CLOSE_PASSING_RANGE"; the DWARF says the
    // constant exists and the reciprocal is what it must be. The arithmetic below is written as
    // the division so the recovered name carries its value.
    const f32 KF_CLOSE_PASSING_RANGE = 2.0f;
}
