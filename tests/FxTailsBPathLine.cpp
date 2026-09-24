// FX-TAILS-B (crash parity 2026-09-24, item 4): CgsSound::Utils::PathLine<2>::Update @0x8268F2D0 --
// the ramp under the road-noise, AI-skid and sweetener envelopes -- against the ARTIST machine code.
//
//   0x8268F2F0..0x8268F304  complete or no stage -> nothing
//   0x8268F308..0x8268F340  the "maLength[mnCurrentStage]" tripwire (CgsSoundUtils.cpp:404): `fcmpu 0.0 ;
//                           bne` -- fires on 0.0 only (a NaN length passes)
//   0x8268F344..0x8268F360  elapsed += dt; `fcmpu t, length ; ble interpolate` (a NaN interpolates)
//   0x8268F364..0x8268F3CC  past the length: the finish level; the last stage completes, else the next
//                           stage starts with t - length (a linked one from the previous finish)
//   0x8268F3D0..0x8268F42C  f = t / length (`fdivs`), `fneg ; fsel` + `fsubs ; fsel` -> [0, 1] with a
//                           NaN -> 1.0, then GetOutput(f, curve) * (finish - start) + start (`fmadds`)
// The PC clamped with two ifs, so a NaN position (a NaN step or length, or 0 / 0 on a zero-length
// stage) reached GetOutput unclamped: NaN out of E_LINEAR, entry 0 (the start level) out of E_POWER.
//
// run_fxtailsb_path_line.py extracts the PRODUCTION PathLine<2>::AddStage and ::Update and the
// production Curve::GetOutput (+ table, constant, read helper) from CgsSoundUtils.cpp; PathLine is
// the real CgsSoundUtils.h struct.
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "rw/math/fpu/scalar_operation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace CgsSound { namespace Utils {
#include "fxtailsb_path_curve_bodies.inc"
#include "fxtailsb_path_line_bodies.inc"
} }

using CgsSound::Utils::Curve;
using CgsSound::Utils::PathLine;

static int giChecks = 0;
static int giFailures = 0;
static void Check(bool lbCondition, const char* lpcLabel)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
}

static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

// One stage from 2.0 to 6.0 over 1000 ms (AddStage stores seconds).
static PathLine<2> OneStage(Curve::ECurveType leCurve)
{
    PathLine<2> l;
    l.AddStage(2.0f, 6.0f, 1000.0f, leCurve);
    return l;
}

int main()
{
    // ---- controls: both bodies agree ------------------------------------------------------------
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.Update(0.25f);
        Check(l.mfCurrentValue == 3.0f, "control: f = 0.25 on a linear 2 -> 6 stage -> 3.0");
    }
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.Update(-0.5f);
        Check(l.mfCurrentValue == 2.0f, "control: a negative position clamps to 0 -> the start level");
    }
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.Update(1.5f);
        Check(l.mfCurrentValue == 6.0f && l.mbComplete, "control: past the only stage -> the finish level, complete");
    }
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.AddLinkedStage(10.0f, 1000.0f, Curve::E_LINEAR);
        l.Update(1.25f);
        const bool lbAdvanced = l.mfCurrentValue == 6.0f && l.mnCurrentStage == 1 && l.mfElapsedTime == 0.25f &&
                                l.maStart[1] == 6.0f;
        l.Update(0.25f);
        Check(lbAdvanced && l.mfCurrentValue == 8.0f,
              "control: stage advance carries t - length and a linked stage starts at the previous finish");
    }
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.Update(std::numeric_limits<f32>::infinity());
        Check(l.mfCurrentValue == 6.0f && l.mbComplete, "control: an infinite step finishes the stage");
    }
    Check(guAsserts == 0, "control: no tripwire on ordinary stages");

    // ---- the NaN polarity of the fsel pair ------------------------------------------------------
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.Update(KF_NAN);
        Check(l.mfCurrentValue == 6.0f && !l.mbComplete,
              "NaN step: `ble` interpolates, the fsel pair makes f 1.0 -> the finish level 6.0 (the two ifs gave NaN)");
    }
    {
        PathLine<2> l = OneStage(Curve::E_POWER);
        l.Update(KF_NAN);
        Check(l.mfCurrentValue == 6.0f,
              "NaN step, E_POWER: f 1.0 reads table[511] = 1.0 -> 6.0 (an unclamped NaN read entry 0 -> the start 2.0)");
    }
    {
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.maLength[0] = KF_NAN;
        const unsigned luBefore = guAsserts;
        l.Update(0.25f);
        Check(l.mfCurrentValue == 6.0f && guAsserts == luBefore,
              "NaN length: the tripwire passes it (`bne` on unordered), t / NaN clamps to 1.0 -> 6.0");
    }
    {
        // A zero-length stage (AddStage floors a length to 0.01 s, so it is seeded directly): the
        // tripwire fires, then 0 <= 0 interpolates 0 / 0 = NaN -> 1.0 -> the finish level.
        PathLine<2> l = OneStage(Curve::E_LINEAR);
        l.maLength[0] = 0.0f;
        const unsigned luBefore = guAsserts;
        l.Update(0.0f);
        Check(guAsserts == luBefore + 1 && l.mfCurrentValue == 6.0f,
              "zero-length stage: the maLength tripwire fires once, then 0 / 0 clamps to 1.0 -> 6.0");
    }

    std::printf("FxTailsBPathLine: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
