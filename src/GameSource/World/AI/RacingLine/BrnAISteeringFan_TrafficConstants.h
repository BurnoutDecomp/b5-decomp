#pragma once

#include <cfloat>   // FLT_MAX (0x7F7FFFFF)

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
    // The DWARF-named members of this set (KF_GUESSED_MAX_SPEED :26, KF_SLOW_PASSING_SPACE :53,
    // KF_TRAFFIC_IMPACT_TIME :55, KF_TRAFFIC_IMPACT_TIME_SQUARED :56, KF_CLOSENESS_TO_BRAKE :59)
    // are defined in BrnAISteeringFan.cpp's file-scope block in their DWARF form.

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
    const f32 KF_NO_CLOSEST_COLLISION = FLT_MAX;                         // flt_8204F664 == 0x7F7FFFFF
}
