#pragma once

// BrnAISteeringFan.cpp TRAFFIC-AVOIDANCE TUNABLES, recovered from the image.
//
// SteeringFan::IncludeConstantBearing @0x827873A0 (rows eFan_AvoidTraffic /
// eFan_AvoidOncomingTraffic) reads five named file-scope constants and six unnamed .rdata
// literals. Two of the named ones live in .bss (0x8300D78C / 0x8300DB00) and therefore read
// 0x00000000 straight out of image.bin -- they are written at start-up by the CRT initialiser
// bank, exactly like the AICar/AIDriver tunables in BrnAICar_Constants.h. Every value below was
// EVALUATED from the image (scratch/postfx_step9_final/envfix/work/image.bin, big-endian, file
// offset == VA - 0x82000000), never guessed.
//
// RECOVERY, per constant (the recipe BrnAICar_Constants.h documents):
//   1. tools/re/findinit.py <addr>  -> every site that materialises the address; the outlier in
//      the 0x82C5xxxx..0x82C6xxxx CRT bank is the WRITER, the rest are readers.
//   2. tools/re/ppcdis.py <writer-0x20> 12  -> the leaf that stores it, and the .rdata float(s)
//      it multiplies.
//   3. read those .rdata floats out of the image.
//
// NAMES. The DWARF (references/DecFIGS/dwarfdump/GameSource/World/AI/RacingLine/
// BrnAISteeringFan.cpp) declares the file-scope constants in source order; the .data run
// 0x82F30298..0x82F302D4 holds the statically-initialised ones in exactly that order, which is
// what pins the three .data names below:
//     0x82F30298..0x82F302B4  the eight KF_*_MAX weights (:33 .. :43)   <- kfBias's own
//                             initialiser reads them as -0x1c(r10) .. 0(r10) with
//                             r10 == 0x82F302B4, at 0x82C69384..0x82C693E4
//     0x82F302B8  KF_MAX_DISTANCE_FOR_EDGE_INTERCEPT (:47)
//     0x82F302BC  KF_START_TO_SLOW                   (:48)  == 0.5
//     0x82F302C0  KF_HNG_AHEAD                       (:50)  == 15.0
//     0x82F302C4  KF_SLOW_PASSING_SPACE              (:53)  <- read here
//     0x82F302C8  KF_TRAFFIC_IMPACT_TIME             (:55)  <- read here
//                 KF_TRAFFIC_IMPACT_TIME_SQUARED     (:56)  -> .bss, dynamic init (it is x*x)
//     0x82F302CC  KF_CLOSENESS_TO_BRAKE              (:59)  <- read here
//     0x82F302D0  KF_STEP_AWAY_FROM_HNG              (:61)  == 0.0625
//     0x82F302D4  KF_STEP_AWAY_FROM_HNG_WHEN_HUGGING (:62)
//     0x82F302D8..0x82F302F0  miSteeringFanPM[7], the PerfMon ids (already named in the tree)
// The run is contiguous and lands every DWARF line on a slot, with the ONE dynamically
// initialised member (:56) missing from it -- which is the corroboration that the mapping is not
// an off-by-one.

#include "types.hpp"

namespace BrnAI
{
    // ================================================================================
    // .bss -- SILENT ZERO IN THE IMAGE, written by a CRT initialiser. RECOVERED.
    // ================================================================================

    // Speed normaliser: IncludeConstantBearing @0x827873E8 divides AICar::GetSpeed() by it and
    // clamps the quotient to [0,1]. DWARF BrnAISteeringFan.cpp:26 (const float32_t
    // KF_GUESSED_MAX_SPEED). The name is pinned by the SECOND reader: findinit reports exactly
    // three sites for 0x8300D78C -- the writer, our load, and 0x82768CF8, which is inside
    // CalculateFanAngle @0x82768CB0, the one other function the DWARF says consumes
    // KF_STEER_AT_LOW_SPEED / KF_STEER_AT_HIGH_SPEED / KF_GUESSED_MAX_SPEED (:24..:26). It is
    // the only SPEED among those three, and both readers use it as a divisor of a car speed.
    // Same value (100 mph) as BrnAICar_Constants.h's KF_DESIRED_PURSUIT_FLOOR, different slot.
    const f32 KF_GUESSED_MAX_SPEED = 44.7039986f;   // 0x8300D78C = init 0x82C692F0, flt_820C3FAC (100 mph) * flt_82F31928 (0.44704)

    // DWARF BrnAISteeringFan.cpp:56. The initialiser is literally x = x * x over
    // KF_TRAFFIC_IMPACT_TIME below -- 0x82C69330 loads flt_82F302C8, fmuls f0, f0, f0, stores
    // 0x8300DB00 -- so the ":55 X / :56 X_SQUARED" pairing in the DWARF is proved by the asm, not
    // inferred from the name. Read at 0x827876C4 as the cut-off on (distance/closing speed)^2.
    const f32 KF_TRAFFIC_IMPACT_TIME_SQUARED = 16.0f;  // 0x8300DB00 = init 0x82C69330, KF_TRAFFIC_IMPACT_TIME * itself

    // ================================================================================
    // .data -- statically initialised, and findinit finds NO writer anywhere in the image for
    // any of the three, so the value baked into image.bin is the value the console runs with.
    // ================================================================================

    // The lateral room the AI insists on when passing. Read at 0x82787410. Its "is zero" test at
    // 0x82787418 is the assert whose text is "Passing Space is zero" (BrnAISteeringFan.cpp:1512),
    // which is what ties this slot to the *_PASSING_SPACE family. DWARF :53.
    const f32 KF_SLOW_PASSING_SPACE = 4.0f;         // 0x82F302C4 (static; no writer)

    // Seconds. Read at 0x827876E8 as the divisor that turns a time-to-collision into the 0..1
    // "risk" the contribution is scaled by. DWARF :55.
    const f32 KF_TRAFFIC_IMPACT_TIME = 4.0f;        // 0x82F302C8 (static; no writer)

    // Read at 0x82787844: only a collision whose 0..1 closeness exceeds this updates the racing
    // line's mfImmediateDistanceToTrafficImpact / mfImmmediateApproachSpeedOfTrafficAhead, i.e.
    // this is the threshold at which the driver is told to brake. DWARF :59.
    const f32 KF_CLOSENESS_TO_BRAKE = 0.949999988f; // 0x82F302CC (static; no writer)

    // ================================================================================
    // [FLAG name-unattested] UNNAMED .rdata LITERALS the compiler folded into the shared pool.
    // The VALUES are exact (read from the image at the addresses named); the NAMES below are
    // DESCRIPTIVE and are NOT in the DWARF, because a folded const float32_t leaves no storage
    // and therefore no DWARF entry. None of them is a .bss silent zero: every address is in the
    // low shared literal pool, is statically non-zero (except the one that is deliberately zero,
    // below), and findinit shows no CRT writer for any of them.
    // DELETE-WHEN a source drop, or a DWARF carrying folded-constant records, names them.
    // ================================================================================

    // flt_82004A18 == 80.0 (88 readers image-wide -- a shared literal). Traffic further away than
    // this is ignored outright (0x8278779C fcmpu f0, f26 / bgt).
    const f32 KF_MAX_TRAFFIC_CONSIDERATION_RANGE = 80.0f;              // flt_82004A18

    // flt_82009B98 == 0.0125 == 1/80, the reciprocal of the range above (0x827877A4). The console
    // keeps them as two independent literals; they are kept apart here for the same reason.
    const f32 KF_RECIPROCAL_MAX_TRAFFIC_CONSIDERATION_RANGE = 0.0125f; // flt_82009B98

    // flt_82005D9C == 10000.0, stored into RacingLine::mfImmediateDistanceToTrafficImpact at
    // 0x8278751C BEFORE the fan loop. The function square-roots that member unconditionally on
    // the way out (0x8278790C..0x8278794C), so "no traffic found" reports 100.0 m, not 10000.0.
    const f32 KF_NO_TRAFFIC_IMPACT_DISTANCE_SQUARED = 10000.0f;        // flt_82005D9C

    // flt_820C4300 == 0.2, the per-tick smoothing weight both rows are interpolated with at
    // 0x82787888..0x827878A8. CANDIDATE NAME KF_RISK_LERP (DWARF BrnAISteeringFan.cpp:388) --
    // NOT adopted: the literal has six game-code readers spread across three different TUs
    // (0x82770A48 AIDriver, 0x8277E14C/1CC/260 PID, 0x82787504 here, 0x82799A20), which is what a
    // shared pool entry looks like, so the address cannot pin the name.
    const f32 KF_TRAFFIC_BIAS_LERP = 0.2f;                             // flt_820C4300

    // flt_82001CC0 == 0.0 -- the shared zero literal (3498 readers image-wide; it is also the
    // register the fsel clamp floor and the "Passing Space is zero" compare use in this very
    // function, so it is provably a real compile-time 0.0 and NOT an unrecovered .bss slot).
    // It is the multiplier of the speed term in
    //     lfAllowedPassingSpace = lfSpeedRatio * <this> + KF_SLOW_PASSING_SPACE   (0x82787414)
    // i.e. the X360 compiler folded a compile-time-constant speed term to zero, so the clamped
    // speed ratio is computed and then contributes nothing. Reproduced verbatim rather than
    // dropped, because dropping it would hide the fact that the console computes lfSpeedRatio.
    const f32 KF_PASSING_SPACE_SPEED_TERM = 0.0f;                      // flt_82001CC0

    // flt_8204F664 == FLT_MAX, the initial "closest collision so far" (0x827874C4).
    const f32 KF_NO_CLOSEST_COLLISION = 3.40282347e38f;                // flt_8204F664
}
