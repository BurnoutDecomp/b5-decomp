// FX-GATE: the extracted UpdateTargetAssist argmin guard (RaceCarPhysics.cpp) against the console's branch,
// 0x82620104 fcmpu weight, best ; 0x82620108 bgt -> skip, else take (run_fxgate_target_assist_argmin.py).
#include <cmath>
#include <cstdio>
#include <limits>

#include "fxgate_target_assist_argmin.inc"

namespace
{
    unsigned gChecks = 0, gFailures = 0;

    // fcmpu's GT bit is set only for an ORDERED greater; bgt branches on it.
    bool ConsoleAccept(float lfWeight, float lfBest)
    {
        const bool lbGreater = !std::isnan(lfWeight) && !std::isnan(lfBest) && lfWeight > lfBest;
        return !lbGreater;
    }

    void Check(bool lbPassed, const char* lpcLabel)
    {
        ++gChecks;
        if (!lbPassed)
        {
            ++gFailures;
            std::printf("FAIL  %s\n", lpcLabel);
        }
    }

    // The candidate loop: the index the argmin ends on (-1 when none is taken).
    template <typename AcceptFn>
    int Loop(const float* lafWeights, int liCount, AcceptFn lAccept)
    {
        float lfBest = 3.4028235e38f;   // the FLT_MAX seed
        int liBest = -1;
        for (int li = 0; li < liCount; ++li)
        {
            if (lAccept(lafWeights[li], lfBest))
            {
                lfBest = lafWeights[li];
                liBest = li;
            }
        }
        return liBest;
    }
}

int main()
{
    const float kfNaN = std::numeric_limits<float>::quiet_NaN();
    const float kfInf = std::numeric_limits<float>::infinity();
    const float kfMax = 3.4028235e38f;

    const struct { float mfWeight, mfBest; const char* mpcWhat; } kaPairs[] = {
        { 1.0f, 2.0f,   "a lighter weight is taken" },
        { 2.0f, 2.0f,   "an equal weight is taken (bgt not taken)" },
        { 3.0f, 2.0f,   "a heavier weight is skipped" },
        { kfNaN, 2.0f,  "a NaN weight is TAKEN (unordered: bgt not taken)" },
        { 1.0f, kfNaN,  "any weight after a NaN best is TAKEN" },
        { kfNaN, kfNaN, "NaN against NaN is taken" },
        { -kfNaN, 5.0f, "a negative NaN weight is taken" },
        { kfInf, kfMax, "+inf against the FLT_MAX seed is skipped" },
        { kfMax, kfMax, "FLT_MAX against the seed is taken" },
        { -0.0f, 0.0f,  "-0 against +0 is taken" },
        { 0.0f, -0.0f,  "+0 against -0 is taken" },
        { 1e-30f, 0.0f, "a tiny positive weight against 0 is skipped" },
    };
    for (const auto& lrPair : kaPairs)
        Check(Accept(lrPair.mfWeight, lrPair.mfBest) == ConsoleAccept(lrPair.mfWeight, lrPair.mfBest), lrPair.mpcWhat);

    // Whole candidate loops.
    const float kaLoop1[] = { 5.0f, 3.0f, kfNaN, 4.0f };          // console: NaN taken, then 4.0 over the NaN best
    const float kaLoop2[] = { kfNaN };                            // console: the only candidate, taken
    const float kaLoop3[] = { 4.0f, 2.0f, 2.0f, 7.0f };           // equal weights: the later one wins
    const float kaLoop4[] = { 3.0f, kfNaN, kfNaN };               // the last NaN wins
    const float kaLoop5[] = { 9.0f, 8.0f, 7.5f, 8.5f, 1.0f };     // ordered argmin
    const float kaLoop6[] = { kfInf, kfInf };                     // nothing beats the seed
    struct { const float* mpWeights; int miCount; const char* mpcWhat; } kaLoops[] = {
        { kaLoop1, 4, "loop: 5, 3, NaN, 4 ends on the 4 (index 3), as the console" },
        { kaLoop2, 1, "loop: a lone NaN candidate is the target" },
        { kaLoop3, 4, "loop: equal weights -> the later one" },
        { kaLoop4, 3, "loop: 3, NaN, NaN -> the last NaN" },
        { kaLoop5, 5, "loop: ordered argmin" },
        { kaLoop6, 2, "loop: +inf never beats the FLT_MAX seed" },
    };
    for (const auto& lrLoop : kaLoops)
    {
        const int liActual = Loop(lrLoop.mpWeights, lrLoop.miCount, Accept);
        const int liConsole = Loop(lrLoop.mpWeights, lrLoop.miCount, ConsoleAccept);
        Check(liActual == liConsole, lrLoop.mpcWhat);
    }

    // The console's answers themselves (so a wrong model cannot pass both sides): loop 1 -> 3, loop 2 -> 0,
    // loop 3 -> 2, loop 4 -> 2.
    Check(Loop(kaLoop1, 4, ConsoleAccept) == 3, "model: loop 1 ends on index 3");
    Check(Loop(kaLoop2, 1, ConsoleAccept) == 0, "model: loop 2 takes the NaN");
    Check(Loop(kaLoop3, 4, ConsoleAccept) == 2, "model: loop 3 takes the later equal weight");
    Check(Loop(kaLoop4, 3, ConsoleAccept) == 2, "model: loop 4 ends on the last NaN");

    std::printf("FxGateTargetAssistArgmin: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
