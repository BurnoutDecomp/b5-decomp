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
    // KF_SLAM_AHEADNESS (:66) and KF_SLAM_FROM_BEHIND_RELATIVE_SPEED (:28) are defined in
    // BrnAISteeringFan.cpp's file-scope block with their original (dynamic) initialisers.

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
