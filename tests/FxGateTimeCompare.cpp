// FX-GATE: CgsSystem::Time (src/GameShared/GameClasses/System/Timer/CgsTime.h, compiled from the
// production header by run_fxgate_time_compare.py) against the console's unordered (NaN) arms.
//
// After fcmpu, `bge` (bc 4,lt) and `ble` (bc 4,gt) are TAKEN on an unordered compare; `blt` / `bgt` are
// not. The X360 bodies:
//   Time::Time(f32)       @0x821F1FA0  0x821F1FC0 fcmpu t,0.0 ; 0x821F1FC4 bge -> past the assert
//   Time::SetFloatVal     @0x821F2140  0x821F2160 fcmpu t,0.0 ; 0x821F2164 bge -> past the assert
//   Time::operator=(f32)  @0x8230E500  0x8230E520 fcmpu t,0.0 ; 0x8230E524 bge -> past the assert
//       -> a NaN time does NOT fire "Time out of range" (the PC `t >= 0` fired it).
//   Time::operator+       @0x8230E5E0  0x8230E618 fcmpu f,1.0 ; 0x8230E61C blt -> no carry, fall-through
//                                      0x8230E620 carries (seconds + 1, fraction - 1)
//   Time::operator+=      @0x8230E7B8  0x8230E7F0 fcmpu f,1.0 ; 0x8230E7F4 blt -> no carry, fall-through
//                                      0x8230E7F8 carries
//       -> a NaN fraction CARRIES (the PC `f >= 1` did not); the "Fraction out of range" assert that
//          follows (0x8230E634/0x8230E638 and 0x8230E810/0x8230E814, blt past) fires on it either way.
//   operator>= (inlined, e.g. GenerateFirstOrLastMessage 0x823958A8/0x823958AC and 0x825503B8/0x825503BC):
//       seconds cmpw, then `li r11,1 ; fcmpu fa,fb ; bge keep ; li r11,0` -> a NaN fraction is >= (TRUE).
//   operator<= (inlined, 0x82550414/0x82550418): `li r11,1 ; fcmpu fa,fb ; ble keep ; li r11,0` -> TRUE.
//   operator- / operator-= (@0x8230E6C0 bge 0x8230E704 / @0x8230E8A0 bge 0x8230E8E4) borrow only on an
//   ordered f < 0, and operator> / operator< are strict: those already matched and are checked unchanged.
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    int giFires = 0;
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giFires; return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    void Check(bool lbOk, const char* lpcWhat)
    {
        ++giChecks;
        if (!lbOk)
        {
            ++giFailures;
            std::printf("FAIL: %s\n", lpcWhat);
        }
    }

    volatile float gfNaN  = std::numeric_limits<float>::quiet_NaN();
    volatile float gfHalf = 0.5f;
}

int main()
{
    using CgsSystem::Time;
    const float lfNaN = gfNaN;

    // "Time out of range": a NaN skips all three asserts, a negative time still fires them.
    giFires = 0;
    { Time lTime(lfNaN); (void)lTime; }
    Check(giFires == 0, "Time(NaN) fires the assert; 0x821F1FC4 bge skips it");
    giFires = 0;
    { Time lTime; lTime = lfNaN; }
    Check(giFires == 0, "operator=(NaN) fires the assert; 0x8230E524 bge skips it");
    giFires = 0;
    { Time lTime; lTime.SetFloatVal(lfNaN); }
    Check(giFires == 0, "SetFloatVal(NaN) fires the assert; 0x821F2164 bge skips it");
    giFires = 0;
    { Time lTime(-1.0f); (void)lTime; }
    Check(giFires == 1, "Time(-1) must still fire (ordered t < 0)");

    // operator+ / operator+= carry a NaN fraction.
    giFires = 0;
    {
        const Time lA(3, lfNaN);
        const Time lB(4, 0.0f);
        const Time lSum = lA + lB;
        Check(lSum.GetSeconds() == 8, "operator+ with a NaN fraction does not carry; 0x8230E61C blt falls through to the carry");
        Check(std::isnan(lSum.GetFraction()), "operator+ NaN fraction must stay NaN");
        Check(giFires == 1, "operator+ must fire \"Fraction out of range\" on the NaN (0x8230E638 blt not taken)");
    }
    giFires = 0;
    {
        Time lA(3, lfNaN);
        lA += Time(4, 0.0f);
        Check(lA.GetSeconds() == 8, "operator+= with a NaN fraction does not carry; 0x8230E7F4 blt falls through to the carry");
        Check(giFires == 1, "operator+= must fire \"Fraction out of range\" on the NaN (0x8230E814 blt not taken)");
    }

    // Ordered carries and borrows are unchanged.
    {
        const Time lSum = Time(1, 0.75f) + Time(2, 0.5f);
        Check(lSum.GetSeconds() == 4 && lSum.GetFraction() == 0.25f, "ordered carry 1.75 + 2.5 = 4.25");
        const Time lNoCarry = Time(1, 0.25f) + Time(2, 0.5f);
        Check(lNoCarry.GetSeconds() == 3 && lNoCarry.GetFraction() == 0.75f, "ordered no-carry 1.25 + 2.5 = 3.75");
        const Time lDiff = Time(3, 0.25f) - Time(1, 0.5f);
        Check(lDiff.GetSeconds() == 1 && lDiff.GetFraction() == 0.75f, "ordered borrow 3.25 - 1.5 = 1.75");
        Time lNanBorrow(3, lfNaN);
        giFires = 0;
        lNanBorrow -= Time(1, 0.5f);
        Check(lNanBorrow.GetSeconds() == 2, "operator-= must not borrow on NaN (0x8230E8E4 bge taken)");
    }

    // operator>= / operator<= with equal seconds: a NaN fraction answers TRUE; strict ones FALSE.
    {
        const Time lNaNTime(5, lfNaN);
        const Time lHalf(5, gfHalf);
        Check(lNaNTime >= lHalf, "Time(5,NaN) >= Time(5,0.5) is FALSE; the inlined bge 0x823958AC keeps 1");
        Check(lHalf >= lNaNTime, "Time(5,0.5) >= Time(5,NaN) is FALSE; bge keeps 1");
        Check(lNaNTime <= lHalf, "Time(5,NaN) <= Time(5,0.5) is FALSE; the inlined ble 0x82550418 keeps 1");
        Check(lHalf <= lNaNTime, "Time(5,0.5) <= Time(5,NaN) is FALSE; ble keeps 1");
        Check(!(lNaNTime > lHalf) && !(lNaNTime < lHalf), "operator> / operator< on a NaN fraction must stay FALSE");

        // Ordered answers and the seconds arms are unchanged.
        Check(Time(5, 0.5f) >= Time(5, 0.5f) && Time(5, 0.5f) <= Time(5, 0.5f), "equal times are >= and <=");
        Check(!(Time(5, 0.25f) >= Time(5, 0.5f)) && Time(5, 0.25f) <= Time(5, 0.5f), "0.25 vs 0.5");
        Check(Time(6, lfNaN) >= Time(5, 0.5f) && !(Time(4, lfNaN) >= Time(5, 0.5f)),
              "the seconds compare decides before the fraction (NaN or not)");
        Check(!(Time(6, lfNaN) <= Time(5, 0.5f)) && Time(4, lfNaN) <= Time(5, 0.5f),
              "the seconds compare decides before the fraction for <=");
    }

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
