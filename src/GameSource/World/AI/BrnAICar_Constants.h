#pragma once

// AICar / AIAggression .bss TUNABLES, recovered from their START-UP INITIALISERS.
//
// The three AI TUs BrnAICar.cpp, BrnAICar_Update.cpp and BrnAIAggression.cpp read ~45 float
// tunables out of the .bss block 0x8300D6E8 .. 0x8300DC64. Reading image.bin there returns
// 0x00000000 for every one of them, because .bss is zero in the image BY DEFINITION -- the
// values are written at start-up by the unity-TU STATIC INITIALISERS, which IDA did not export
// as functions. Earlier waves therefore shipped this whole family as `0.0f` placeholders; a
// zero desired speed is why a Road Rage rival computed `desired 0.000000` and stood still.
//
// THE WRITERS ARE IN THE IMAGE. They are the same CRT init bank BrnAIDriver_Constants.h names:
// a run of ~100 tiny leaf routines at 0x82C67F00 .. 0x82C69500, each of the shape
//     lis/lfs f0, <mph rodata> ; fmuls f0, f0, flt_82F31928 ; lis ; stfs f0, <0x8300Dxxx> ; blr
// with flt_82F31928 == 0.44704 (the mph -> m/s factor). Every value below was EVALUATED from
// its initialiser (capstone over scratch/postfx_step9_final/envfix/work/image.bin, file offset
// == VA - 0x82000000), so this whole header is CONSOLE-EXACT.
//
// Recovery recipe (repeatable for any other silent-zero .bss float):
//   1. sweep 0x82000000..0x82D40000 for `stfs frS, d(rA)` with rA formed by a lis/addi pair,
//      and keep the sites whose resolved target is the .bss address you want;
//   2. every address below has EXACTLY ONE writer, and all of them are in 0x82C685B8..0x82C69488
//      (the CRT init bank) -- no game function ever re-writes them, so the value is a constant;
//   3. walk back from the store to the preceding `blr` and evaluate the leaf: the mph source
//      float, times flt_82F31928.
//
// LAYOUT of each line below: value -- .bss address = initialiser address, source rodata, mph.
// The ADDRESS is the load-bearing part: it is what the console body loads.

#include "types.hpp"

namespace BrnAI
{
    // ================================================================================
    // BrnAICar.cpp -- CalcDesiredSpeed @0x82796078
    //
    // ROLE NOTE (the names are historical; the asm's fsel pattern is what matters):
    //   KF_DESIRED_MAX_SPEED is used as a LOWER floor -- `fsel f1, (K - v), K, v` returns K
    //   when K >= v, i.e. max(K, v). KF_DESIRED_PURSUIT_FLOOR is the UPPER cap of the same
    //   clamp (`fsel f1, (K - v), v, K` == min(K, v)). Net: the INFRONT_SEPARATING arm clamps
    //   playerSpeed - 5 mph into [80 mph, 100 mph].
    // ================================================================================
    const f32 KF_DESIRED_MAX_SPEED          = 35.7631989f;  // 0x8300DB04 = init 0x82C68758, flt_82004A18 ( 80) *  0.44704
    const f32 KF_DESIRED_PURSUIT_FALLBEHIND =  2.23519993f; // 0x8300D700 = init 0x82C68798, flt_820C488C (  5) *  0.44704
    const f32 KF_DESIRED_PURSUIT_FLOOR      = 44.7039986f;  // 0x8300DC3C = init 0x82C68738, flt_820C3FAC (100) *  0.44704
    const f32 KF_DESIRED_FREEROAM_BIAS      =  8.94079971f; // 0x8300D97C = init 0x82C68778, flt_820C4890 ( 20) *  0.44704
    const f32 KF_DESIRED_OPP_SCALE_RACE     =  1.78815997f; // 0x8300DC08 = init 0x82C686F8, flt_820C41C0 (  4) *  0.44704
    const f32 KF_DESIRED_OPP_BASE_RACE      = 37.9983978f;  // 0x8300D7D4 = init 0x82C686D8, flt_8200A038 ( 85) *  0.44704
    const f32 KF_DESIRED_OPP_SCALE_DEFAULT  =  2.68224001f; // 0x8300D968 = init 0x82C686B8, flt_820C4250 (  6) *  0.44704
    const f32 KF_DESIRED_OPP_BASE_DEFAULT   = 35.7631989f;  // 0x8300D708 = init 0x82C68698, flt_82004A18 ( 80) *  0.44704
    const f32 KF_DESIRED_INRANGE_PLAYER     = 35.7631989f;  // 0x8300D980 = init 0x82C68678, flt_82004A18 ( 80) *  0.44704

    // BrnAICar.cpp -- CalcPersonalitySpeed @0x8276F610
    const f32 KF_PERSONALITY_SPEED_OFFSET   =  8.94079971f; // 0x8300D788 = init 0x82C68718, flt_820C4890 ( 20) *  0.44704
    const f32 KF_PERSONALITY_CLAMP_LO       =  8.94079971f; // 0x8300D93C = init 0x82C685D8, flt_820C4890 ( 20) *  0.44704
    const f32 KF_PERSONALITY_CLAMP_HI       = 107.289597f;  // 0x8300D740 = init 0x82C685F8, flt_8207B114 (240) *  0.44704

    // BrnAICar.cpp -- CalcRoadRageSpeed @0x8276F8A0.
    // BEHIND_BIAS and AHEAD_DROP are the SAME 60 mph: the blended arm lerps
    // playerSpeed + 60 mph (at separation 0) down to playerSpeed - 60 mph (at separation 20 m).
    const f32 KF_ROAD_RAGE_BEHIND_BIAS      = 26.8223991f;  // 0x8300D724 = init 0x82C68638, flt_820C4158 ( 60) *  0.44704
    const f32 KF_ROAD_RAGE_SLOW_FLOOR       = 53.6447983f;  // 0x8300D810 = init 0x82C68658, flt_82004A28 (120) *  0.44704
    const f32 KF_ROAD_RAGE_AHEAD_DROP       = 26.8223991f;  // 0x8300D83C = init 0x82C68618, flt_820C4158 ( 60) *  0.44704

    // BrnAICar.cpp -- UpdateRaceDistance @0x8278AEB8
    const f32 KF_RACE_DISTANCE_MIN_SPEED    = 17.8815994f;  // 0x8300D814 = init 0x82C687B8, flt_82004D0C ( 40) *  0.44704

    // ================================================================================
    // BrnAICar_Update.cpp -- GetUsefulDirection (readers @0x82770028)
    // ================================================================================
    const f32 KF_USEFUL_DIRECTION_MIN_SPEED =  8.94079971f; // 0x8300D964 = init 0x82C685B8, flt_820C4890 ( 20) *  0.44704

    // ================================================================================
    // BrnAIAggression.cpp
    // ================================================================================
    // CarIsTooSlow @0x82766948 / DecideToAttack @0x82770C50
    const f32 KF_CAR_TOO_SLOW_SPEED         = 17.8815994f;  // 0x8300D9A0 = init 0x82C68DE0, flt_82004D0C ( 40) *  0.44704
    const f32 KF_MARKED_MAN_ATTACK_SPEED    = 40.2336006f;  // 0x8300D974 = init 0x82C68EC0, flt_820C42C0 ( 90) *  0.44704

    // GetMinFallBackSpeed @0x82770A68 -- dispatch on meRouteFindingStyle (3/2/6/default).
    const f32 KF_MIN_FALLBACK_SPEED_PURSUIT    = 40.2336006f; // 0x8300D988 = init 0x82C68C20, flt_820C42C0 ( 90) * 0.44704
    const f32 KF_MIN_FALLBACK_SPEED_ROAD_RAGE  = 26.8223991f; // 0x8300D744 = init 0x82C68BE0, flt_820C4158 ( 60) * 0.44704
    const f32 KF_MIN_FALLBACK_SPEED_MARKED_MAN =  0.0f;       // 0x8300DB2C = init 0x82C68C00, flt_82001CC0 (  0) * 0.44704 -- a GENUINE console zero
    const f32 KF_MIN_FALLBACK_SPEED_DEFAULT    = 44.7039986f; // 0x8300D7DC = init 0x82C68BC0, flt_820C3FAC (100) * 0.44704

    // GetMaxOvertakeSpeed @0x82771618 -- lerp(LO, HI, aggressionLevel)
    // (vmaddfp v0, (HI-LO), LO, aggression; IDA raw field order D,A,B,C => D = A*C + B).
    const f32 KF_MAX_OVERTAKE_SPEED_MARKED_MAN_HI = 89.4079971f; // 0x8300D714 = init 0x82C68D40, flt_820C4318 (200) * 0.44704
    const f32 KF_MAX_OVERTAKE_SPEED_MARKED_MAN_LO = 71.5263977f; // 0x8300D6F0 = init 0x82C68D60, flt_820C482C (160) * 0.44704
    const f32 KF_MAX_OVERTAKE_SPEED_DEFAULT_HI    = 84.9375992f; // 0x8300D7A0 = init 0x82C68D00, flt_82022E60 (190) * 0.44704
    const f32 KF_MAX_OVERTAKE_SPEED_DEFAULT_LO    = 71.5263977f; // 0x8300D970 = init 0x82C68D20, flt_820C482C (160) * 0.44704

    // Shared "speed match disabled / no passing" speed.
    const f32 KF_NO_PASSING_SPEED           = 26.8223991f;  // 0x8300D6F4 = init 0x82C68B40, flt_820C4158 ( 60) *  0.44704

    // GetSpeedMatchSpeed @0x8277E058 -- OVERTAKE_FAST (case 4)
    const f32 KF_OVERTAKE_FAST_MIN_SPEED    = 53.6447983f;  // 0x8300D71C = init 0x82C68D80, flt_82004A28 (120) *  0.44704
    const f32 KF_OVERTAKE_FAST_SPEED_BIAS   =  8.94079971f; // 0x8300DBE0 = init 0x82C68CC0, flt_820C4890 ( 20) *  0.44704

    // GetSpeedMatchSpeed -- SLOW_TO_CLIP (case 3)
    const f32 KF_SLOW_TO_CLIP_SPEED_DROP    =  4.47039986f; // 0x8300D7F0 = init 0x82C68C60, flt_820C4150 ( 10) *  0.44704
    const f32 KF_SLOW_TO_CLIP_FALLBACK      =  8.94079971f; // 0x8300D754 = init 0x82C68C40, flt_820C4890 ( 20) *  0.44704

    // GetSpeedMatchSpeed -- SLOWER (case 2)
    const f32 KF_SLOWER_BEHIND_SPEED        = 17.8815994f;  // 0x8300D784 = init 0x82C68B60, flt_82004D0C ( 40) *  0.44704

    // GetSpeedMatchSpeed -- default (aggressive fall-back) branch
    const f32 KF_AGG_FALLBACK_POS_MAGNITUDE = 13.4111996f;  // 0x8300D830 = init 0x82C68EA0, flt_820C3FA8 ( 30) *  0.44704
    const f32 KF_AGG_FALLBACK_RELSPEED_HI   =  4.47039986f; // 0x8300DB08 = init 0x82C68E80, flt_820C4150 ( 10) *  0.44704
    const f32 KF_AGG_FALLBACK_RELSPEED_LO   = 22.3519993f;  // 0x8300D72C = init 0x82C68E60, flt_820C4244 ( 50) *  0.44704
    const f32 KF_AGG_FALLBACK_BASE_MARKED   =  0.0f;        // 0x8300D6E8 = init 0x82C68E40, flt_82001CC0 (  0) *  0.44704 -- a GENUINE console zero
    const f32 KF_AGG_FALLBACK_BASE_DEFAULT  = 22.3519993f;  // 0x8300D7D0 = init 0x82C68E20, flt_820C4244 ( 50) *  0.44704

    // SetSlowOvertakingSpeed @0x8277DB38
    const f32 KF_SLOW_OVERTAKE_SPEED_BIAS   =  2.23519993f; // 0x8300D758 = init 0x82C68CE0, flt_820C488C (  5) *  0.44704
    const f32 KF_SLOW_OVERTAKE_CAP_BIAS     =  8.94079971f; // 0x8300D778 = init 0x82C68DA0, flt_820C4890 ( 20) *  0.44704

    // SetSlowFallbackSpeed @0x82770AB8 -- lerp(LO, HI, mpCar->mfRelativeSpeedForMatch).
    // HI < LO here: the fall-back offset shrinks from 20 mph to 2 mph as the factor rises.
    const f32 KF_SLOW_FALLBACK_RELSPEED_HI  =  0.894079983f;// 0x8300D7D8 = init 0x82C68BA0, flt_820C41F4 (  2) *  0.44704
    const f32 KF_SLOW_FALLBACK_RELSPEED_LO  =  8.94079971f; // 0x8300D728 = init 0x82C68B80, flt_820C4890 ( 20) *  0.44704

    // UpdateAggressionStateClipOff
    const f32 KF_CLIP_OFF_MIN_SPEED         = 35.7631989f;  // 0x8300D720 = init 0x82C68C80, flt_82004A18 ( 80) *  0.44704

    // AIAggression::Prepare -- the seeded mfNonSpeedMatchedSpeed.
    const f32 KF_NON_SPEED_MATCHED_SPEED_DEFAULT = 35.7631989f; // 0x8300DC64 = init 0x82C68CA0, flt_82004A18 (80) * 0.44704
}
