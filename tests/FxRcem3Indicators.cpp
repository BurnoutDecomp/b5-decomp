// FX-RCEM3 (crash-parity 2026-09-23): the production ActiveRaceCar::SetIndicatorState (ARTIST
// 0x822A52B0) and ActiveRaceCar::UpdateIndicators (ARTIST 0x822A5340), extracted VERBATIM from
// BrnActiveRaceCar.cpp by run_fxrcem3_indicators.py and run against a fixture car. When the source
// has no UpdateIndicators (the pre-fix tree), the runner substitutes an empty body -- what the PC
// did, since nothing wrote the render bits.
//   G60-D1  SetIndicatorState arm 1 (r4): if (!+0x1C8D) t = 0 ; +0x1C8C = 0 ; +0x1C8D = 1
//           (0x822A52BC..0x822A52E0); arm 2 is the mirror; else t = 0 and both latches 0.
//   G60-D2 / G61-D5  UpdateIndicators: t += dt while a latch is up; !(t <= 0.5) -> 0;
//           lit = t < 0.25; mbIsIndicatingLeft = left latch && lit; mbIsIndicatingRight = right && lit.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
}

namespace Fixture {
struct ActiveRaceCar {
    struct RenderParams {
        bool mbIsIndicatingLeft = false, mbIsIndicatingRight = false;
        int  miLeftWrites = 0, miRightWrites = 0;
        void SetIndicatingLeft(bool lbOn)  { ++miLeftWrites; mbIsIndicatingLeft = lbOn; }
        void SetIndicatingRight(bool lbOn) { ++miRightWrites; mbIsIndicatingRight = lbOn; }
    };
    f32          mfIndicatorTime = 0.0f;
    bool         mbRightIndicatorActive = false;
    bool         mbLeftIndicatorActive = false;
    RenderParams mRenderParams;
    void SetIndicatorState(bool, bool);
    void UpdateIndicators(f32 lfTimeStep);
};
#include "fxrcem3_indicators.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

int main()
{
    using Fixture::ActiveRaceCar;

    // ---- G60-D1: SetIndicatorState ------------------------------------------------------------
    {
        ActiveRaceCar c; c.mfIndicatorTime = 0.3f;
        c.SetIndicatorState(true, false);
        Check(!c.mbRightIndicatorActive && c.mbLeftIndicatorActive && c.mfIndicatorTime == 0.0f,
              "G60-D1 (1,0) from idle: +0x1C8D (left) = 1, +0x1C8C (right) = 0, timer reset (left was off)");

        ActiveRaceCar a; a.mbLeftIndicatorActive = true; a.mfIndicatorTime = 0.3f;
        a.SetIndicatorState(true, false);
        Check(a.mbLeftIndicatorActive && !a.mbRightIndicatorActive && a.mfIndicatorTime == 0.3f,
              "G60-D1 (1,0) while left already on: the blink phase is kept (lbz +0x1C8D != 0 skips the stfs)");

        ActiveRaceCar r; r.mbRightIndicatorActive = true; r.mfIndicatorTime = 0.3f;
        r.SetIndicatorState(true, false);
        Check(r.mbLeftIndicatorActive && !r.mbRightIndicatorActive && r.mfIndicatorTime == 0.0f,
              "G60-D1 (1,0) while right on: switches to left (stb 0 +0x1C8C ; stb 1 +0x1C8D), timer reset");

        ActiveRaceCar b; b.mfIndicatorTime = 0.3f;
        b.SetIndicatorState(true, true);
        Check(b.mbLeftIndicatorActive && !b.mbRightIndicatorActive,
              "G60-D1 (1,1): arm 1 wins -> left");

        ActiveRaceCar d; d.mfIndicatorTime = 0.3f;
        d.SetIndicatorState(false, true);
        Check(d.mbRightIndicatorActive && !d.mbLeftIndicatorActive && d.mfIndicatorTime == 0.0f,
              "G60-D1 (0,1) from idle: right = 1, left = 0, timer reset (0x822A52F4..0x822A5318)");

        ActiveRaceCar e; e.mbRightIndicatorActive = true; e.mfIndicatorTime = 0.3f;
        e.SetIndicatorState(false, true);
        Check(e.mbRightIndicatorActive && !e.mbLeftIndicatorActive && e.mfIndicatorTime == 0.3f,
              "G60-D1 (0,1) while right already on: phase kept");

        ActiveRaceCar f; f.mbRightIndicatorActive = true; f.mbLeftIndicatorActive = true; f.mfIndicatorTime = 0.3f;
        f.SetIndicatorState(false, false);
        Check(!f.mbRightIndicatorActive && !f.mbLeftIndicatorActive && f.mfIndicatorTime == 0.0f,
              "G60-D1 (0,0): both latches off, timer 0 (0x822A5320..0x822A5334)");
    }

    // ---- G60-D2 / G61-D5: UpdateIndicators ----------------------------------------------------
    {
        ActiveRaceCar c; c.SetIndicatorState(true, false);          // left latch, t = 0
        c.UpdateIndicators(0.1f);
        Check(c.mRenderParams.miLeftWrites == 1 && c.mRenderParams.miRightWrites == 1,
              "G60-D2 both render bits are written every call (stb +0x1BE9 / +0x1BEA)");
        Check(Bits(c.mfIndicatorTime) == Bits(0.0f + 0.1f) && c.mRenderParams.mbIsIndicatingLeft
                  && !c.mRenderParams.mbIsIndicatingRight,
              "G60-D2 t = 0.1 (< 0.25): left lamp lit, right dark");
        c.UpdateIndicators(0.1f); c.UpdateIndicators(0.1f);         // t = 0.3
        Check(!c.mRenderParams.mbIsIndicatingLeft && !c.mRenderParams.mbIsIndicatingRight,
              "G60-D2 t = 0.3 (>= 0.25, flt_82003F40): the off half of the blink");
        c.mfIndicatorTime = 0.45f; c.UpdateIndicators(0.1f);         // 0.55 > 0.5 -> 0
        Check(c.mfIndicatorTime == 0.0f && c.mRenderParams.mbIsIndicatingLeft,
              "G60-D2 t = 0.55 > 0.5 (flt_820147FC): wraps to 0.0 and the lamp is lit again");
        c.mfIndicatorTime = 0.25f; c.UpdateIndicators(0.25f);        // exactly 0.5 -> kept
        Check(c.mfIndicatorTime == 0.5f && !c.mRenderParams.mbIsIndicatingLeft,
              "G60-D2 t == 0.5 is kept (ble), and 0.5 is dark");
        c.mfIndicatorTime = std::numeric_limits<f32>::quiet_NaN(); c.UpdateIndicators(0.1f);
        Check(c.mfIndicatorTime == 0.0f && c.mRenderParams.mbIsIndicatingLeft,
              "G60-D2 a NaN timer fails `ble` and wraps to 0.0 (!(t <= 0.5))");

        ActiveRaceCar r; r.SetIndicatorState(false, true);
        r.UpdateIndicators(1.0f / 60.0f);
        Check(r.mRenderParams.mbIsIndicatingRight && !r.mRenderParams.mbIsIndicatingLeft,
              "G61-D5 right latch -> mbIsIndicatingRight lit, left dark");

        ActiveRaceCar idle; idle.mfIndicatorTime = 0.1f;
        idle.mRenderParams.mbIsIndicatingLeft = idle.mRenderParams.mbIsIndicatingRight = true;
        idle.UpdateIndicators(0.2f);
        Check(idle.mfIndicatorTime == 0.1f && !idle.mRenderParams.mbIsIndicatingLeft && !idle.mRenderParams.mbIsIndicatingRight,
              "G60-D2 no latch: timer frozen, both render bits cleared");

        ActiveRaceCar both; both.mbLeftIndicatorActive = both.mbRightIndicatorActive = true;
        both.UpdateIndicators(0.1f);
        Check(both.mRenderParams.mbIsIndicatingLeft && both.mRenderParams.mbIsIndicatingRight,
              "G60-D2 both latches (hazards): both lamps follow the same phase");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3Indicators: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
