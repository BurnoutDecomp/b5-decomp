// FX-GATE, the AI fused group (crash parity 2026-09-25): the AI lerps and the Power Parking distance square that
// the console computes with ONE rounding (fmadds / fnmsubs / vmaddfp, ROUNDING_RULE 3). run_fxgate_ai_fused.py
// EXTRACTS each production statement into a kernel (fxgate_ai_fused.inc) and this file runs the kernels on the
// console results in FxGateAiFusedData.h (exact arithmetic, checked against the real words on emu64).
//   FanAngle / LookAhead  SteeringFan::CalculateFanAngle          fmadds 0x82768D20 / 0x82768D2C
//   Segment / SpeedLerp   RaceBalancingGraph::ComputeSpeedRatio   fnmsubs 0x8277B894 (then fmuls 7.0) / fmadds 0x8277B8B8
//   Multiplier            RaceBalancingManager::ComputeTargetSpeed fmadds 0x827917AC
//   Completion            RaceBalancingRoute::ComputeRaceCompletionRatio fmadds 0x8277AE08
//   RoadSide              ResetOnTrackManager::GetRoadSideForStartingLine fmadds 0x82784488 (then fsubs 1.0)
//   Proximity / Cornering AIDriver::ProximitySpeed / CorneringTopSpeed   vmaddfp 0x827708C8 / 0x8277D2A0
//   DistanceSq            CheckVehicleForPowerPark                vmulfp128 0x822B1FF8 + vmaddfp 0x822B1FFC
// RoadSide multiplies by 2.0, which is exact, so one rounding and two agree there for every draw: its rows and the
// exhaustive ring sweep below pass on both spellings, and its conversion is pinned by the runner's wiring check.
#include "types.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "fxgate_ai_fused.inc"
#include "FxGateAiFusedData.h"

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }

    // Bit for bit, the sign of a zero included; any NaN matches any NaN (the payload is not modelled).
    bool Same(unsigned luA, unsigned luB) { return (IsNaNBits(luA) && IsNaNBits(luB)) || luA == luB; }

    void Constant(const char* lpcName, float lfValue, unsigned luImage, const char* lpcAddress)
    {
        ++gChecks;
        if (Bits(lfValue) != luImage)
        {
            ++gFailures;
            std::printf("FAIL  %s = 0x%08X, the image has 0x%08X @%s\n", lpcName, Bits(lfValue), luImage, lpcAddress);
        }
    }

    template <typename Kernel>
    void Table(const char* lpcSite, const AiFusedRow* lpRows, unsigned luCount, Kernel lKernel)
    {
        unsigned luFailures = 0, luDiffering = 0, luDifferingFailed = 0;
        for (unsigned luRow = 0; luRow < luCount; ++luRow)
        {
            const AiFusedRow& lrRow = lpRows[luRow];
            const float lfActual = lKernel(lrRow);
            ++gChecks;
            if (lrRow.mbDiffers)
                ++luDiffering;
            if (!Same(Bits(lfActual), lrRow.muOut))
            {
                ++gFailures;
                ++luFailures;
                if (lrRow.mbDiffers)
                    ++luDifferingFailed;
                if (luFailures <= 4)
                    std::printf("FAIL  %s row %u: in (0x%08X, 0x%08X, 0x%08X) -> 0x%08X (%.9g), console 0x%08X (%.9g)\n",
                                lpcSite, luRow, lrRow.muA, lrRow.muB, lrRow.muC, Bits(lfActual), lfActual,
                                lrRow.muOut, Float(lrRow.muOut));
            }
        }
        std::printf("%-11s %3u rows, %2u where one rounding and two differ: %u failures (%u of them on those rows)\n",
                    lpcSite, luCount, luDiffering, luFailures, luDifferingFailed);
    }
}

#define FXGATE_ROWS(table) table, static_cast<unsigned>(sizeof(table) / sizeof(table[0]))

int main()
{
    // The production constants against the image (tools/re/x360rd.py).
    Constant("KF_STEER_AT_LOW_SPEED", KF_STEER_AT_LOW_SPEED, 0x3FB2B8C2u, "0x82F30428");
    Constant("KF_STEER_AT_HIGH_SPEED", KF_STEER_AT_HIGH_SPEED, 0x3F000000u, "0x820C4168");
    Constant("KF_FAN_LOOK_AHEAD_BASE", KF_FAN_LOOK_AHEAD_BASE, 0x41200000u, "0x820C4150");
    Constant("KF_FAN_LOOK_AHEAD_GROWTH", KF_FAN_LOOK_AHEAD_GROWTH, 0x41700000u, "0x820C4238");
    Constant("KF_SPEED_DIFFERENCE_MULTIPLIER", KF_SPEED_DIFFERENCE_MULTIPLIER, 0x3DCCCCCDu, "0x820C424C");
    Constant("KF_STARTING_LINE_SPREAD", KF_STARTING_LINE_SPREAD, 0x40000000u, "0x820C41F4");
    Constant("KF_STARTING_LINE_CAR_WIDTH", KF_STARTING_LINE_CAR_WIDTH, 0x41000000u, "0x820C41E0");

    Table("FanAngle", FXGATE_ROWS(kaFanAngleRows),
          [](const AiFusedRow& r) { return K_FanAngle(Float(r.muA)); });
    Table("LookAhead", FXGATE_ROWS(kaLookAheadRows),
          [](const AiFusedRow& r) { return K_LookAhead(Float(r.muA)); });
    Table("Segment", FXGATE_ROWS(kaSegmentRows),
          [](const AiFusedRow& r) { return K_Segment(Float(r.muA), static_cast<s32>(r.muB)); });
    Table("SpeedLerp", FXGATE_ROWS(kaSpeedLerpRows),
          [](const AiFusedRow& r) { return K_SpeedLerp(Float(r.muA), Float(r.muB), Float(r.muC)); });
    Table("Multiplier", FXGATE_ROWS(kaMultiplierRows),
          [](const AiFusedRow& r) { return K_Multiplier(Float(r.muA), Float(r.muB)); });
    Table("Completion", FXGATE_ROWS(kaCompletionRows),
          [](const AiFusedRow& r) { return K_Completion(Float(r.muA), Float(r.muB), Float(r.muC)); });
    Table("RoadSide", FXGATE_ROWS(kaRoadSideRows),
          [](const AiFusedRow& r) { return K_RoadSide(Float(r.muA), Float(r.muB)); });
    Table("Proximity", FXGATE_ROWS(kaProximityRows),
          [](const AiFusedRow& r) { return K_Proximity(Float(r.muA), Float(r.muB), Float(r.muC)); });
    Table("Cornering", FXGATE_ROWS(kaCorneringRows),
          [](const AiFusedRow& r) { return K_Cornering(Float(r.muA), Float(r.muB), Float(r.muC)); });
    Table("DistanceSq", FXGATE_ROWS(kaDistanceSqRows),
          [](const AiFusedRow& r) { return K_DistanceSq(Float(r.muA), Float(r.muB), Float(r.muC)); });

    // RoadSide over every ring draw (k / 2^23, k < 2^23) at four widths: the production statement against the
    // console's fmadds + fsubs, and the fused value against the twice-rounded one (the product by 2.0 is exact).
    const float kaWidths[] = { 4.0f, 7.25f, 12.5f, 33.125f };
    for (float lfWidth : kaWidths)
    {
        unsigned luConsole = 0, luNeutral = 0;
        for (unsigned luK = 0; luK < (1u << 23); ++luK)
        {
            const float lfDraw = static_cast<float>(luK) * (1.0f / 8388608.0f);
            const float lfConsole = std::fmaf(lfDraw, 2.0f, 8.0f / lfWidth) - 1.0f;
            if (Bits(K_RoadSide(lfDraw, lfWidth)) != Bits(lfConsole))
                ++luConsole;
            if (Bits((lfDraw * 2.0f + 8.0f / lfWidth) - 1.0f) != Bits(lfConsole))
                ++luNeutral;
        }
        gChecks += 2;
        if (luConsole != 0)
        {
            ++gFailures;
            std::printf("FAIL  RoadSide width %g: %u of 2^23 draws differ from the console's fmadds\n", lfWidth, luConsole);
        }
        if (luNeutral != 0)
        {
            ++gFailures;
            std::printf("FAIL  RoadSide width %g: %u of 2^23 draws round differently unfused\n", lfWidth, luNeutral);
        }
    }

    std::printf("FxGateAiFused: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
