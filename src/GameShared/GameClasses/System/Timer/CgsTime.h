// b5-decomp/src/GameShared/GameClasses/System/Timer/CgsTime.h
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSystem::Time - a lightweight game-time value type: an integer second count plus a
// normalised [0,1) sub-second fraction. Defined entirely inline (a header-only value
// class). Layout + API authoritative from DecFIGS DWARF (System/Timer/CgsTime.h:33):
// miSeconds @ +0 (int32), mfFraction @ +4 (float32); full inline logic matches the
// Feb-2007 partial source source verbatim (the X360 binary agrees, including operator-'s
// clamp-fraction-to-0 branch). The original streamed the assert message through a
// CgsDev::StrStream temporary; that lowers to the project's CGS_ASSERT.
//
// This TU's six binary-recovered functions are operator=(f32), operator+, operator-,
// operator+=, operator-=, SetFraction. The remaining inline bodies (ctors, copy/assign,
// comparisons, Get*/Set*) are filled from the Feb-2007 partial source so the header links
// standalone (operator+/operator- construct `Time lNewTime;` and return by value, so
// they depend on the default ctor being defined here).
//
// NaN polarity (crash parity FX-GATE, read against the X360 bodies): the console compiles each `a >= b`
// / `a <= b` below as ONE condition bit of an fcmpu, `bge` (bc 4,lt) / `ble` (bc 4,gt), which is TAKEN
// on an unordered compare, and each `a < b` / `a > b` guard as `blt` / `bgt`, which is not. So where the
// console's answer for a NaN differs from the literal x64 comparison the test is spelled as the branch:
// the three `t >= 0` asserts are `!(t < 0)` (a NaN does not fire them), the two carries test `!(f < 1)`
// (a NaN fraction carries) and the inlined operator>= / operator<= are `!(a < b)` / `!(a > b)` (a NaN
// fraction answers TRUE). Each site's addresses are at its body.
namespace CgsSystem
{
    class Time
    {
    public:

        Time();
        Time(const Time& lTime);
        Time(f32 lfTime);
        Time(s32 liSeconds, f32 lfFraction);

        Time& operator=(const Time& lTime);
        Time& operator=(f32 lfTime);

        Time operator+(const Time& lTime) const;
        Time operator-(const Time& lTime) const;
        Time& operator+=(const Time& lTime);
        Time& operator-=(const Time& lTime);

        bool operator>(const Time& lTime) const;
        bool operator<(const Time& lTime) const;
        bool operator>=(const Time& lTime) const;
        bool operator<=(const Time& lTime) const;

        f32  GetFraction() const;
        s32  GetSeconds() const;
        f32  GetFloatVal() const;

        void SetFraction(f32 lfFraction);
        void SetSeconds(s32 liSeconds);
        void SetFloatVal(f32 lfFloatVal);

    private:

        s32 miSeconds;   // DWARF CgsTime.h:94  @ +0
        f32 mfFraction;  // DWARF CgsTime.h:95  @ +4
    };

    // ---- ctors / assignment (filled from Feb-2007 leak) ------------------------

    inline Time::Time()
    {
        miSeconds = 0;
        mfFraction = 0.f;
    }

    inline Time::Time(const Time& lTime)
    {
        miSeconds = lTime.miSeconds;
        mfFraction = lTime.mfFraction;
    }

    // X360 @0x821F1FA0: 0x821F1FC0 fcmpu t, 0.0 (flt_82001CC0) ; 0x821F1FC4 bge -> past the assert.
    inline Time::Time(f32 lfTime)
    {
        CGS_ASSERT(!(lfTime < 0.f), "Time out of range\n");   // CgsTime.h:115
        miSeconds  = static_cast<s32>(lfTime);
        mfFraction = static_cast<f32>(lfTime - static_cast<f32>(miSeconds));
    }

    inline Time::Time(s32 liSeconds, f32 lfFraction)
    {
        miSeconds = liSeconds;
        mfFraction = lfFraction;
    }

    inline Time&
    Time::operator=(const Time& lTime)
    {
        miSeconds = lTime.miSeconds;
        mfFraction = lTime.mfFraction;
        return *this;
    }

    // ---- owned by this TU (6 functions) ----------------------------------------

    // X360 @0x8230E500: 0x8230E520 fcmpu t, 0.0 (flt_82001CC0) ; 0x8230E524 bge -> past the assert.
    inline Time&
    Time::operator=(f32 lfTime)
    {
        CGS_ASSERT(!(lfTime < 0.f), "Time out of range\n");   // CgsTime.h:137
        miSeconds  = static_cast<s32>(lfTime);
        mfFraction = static_cast<f32>(lfTime - static_cast<f32>(miSeconds));
        return *this;
    }

    inline Time
    Time::operator+(const Time& lTime) const
    {
        Time lNewTime;
        lNewTime.miSeconds  = miSeconds + lTime.miSeconds;
        lNewTime.mfFraction = mfFraction + lTime.mfFraction;

        // X360 @0x8230E5E0: 0x8230E618 fcmpu f, 1.0 (flt_82001C98) ; 0x8230E61C blt -> no carry, so a
        // NaN fraction falls through to the carry at 0x8230E620.
        if (!(lNewTime.mfFraction < 1.f))
        {
            lNewTime.mfFraction -= 1.f;
            lNewTime.miSeconds++;
        }

        CGS_ASSERT(lNewTime.mfFraction < 1.f, "Fraction out of range\n");   // CgsTime.h:157
        return lNewTime;
    }

    inline Time
    Time::operator-(const Time& lTime) const
    {
        Time lNewTime;
        lNewTime.miSeconds  = miSeconds - lTime.miSeconds;
        lNewTime.mfFraction = mfFraction - lTime.mfFraction;

        if (lNewTime.mfFraction < 0.f)
        {
            lNewTime.mfFraction += 1.f;
            // X360 @0x8230E6C0: 0x8230E710 fcmpu f, 1.0 ; 0x8230E714 blt -> seconds - 1, else the
            // fraction is cleared (0x8230E718). Only an ordered f < 0 reaches here, so f + 1 is never NaN.
            if (!(lNewTime.mfFraction < 1.0f))
            {
                lNewTime.mfFraction = 0.0f;
            }
            else
            {
                lNewTime.miSeconds--;
            }
        }

        CGS_ASSERT(lNewTime.mfFraction < 1.f, "Fraction out of range\n");   // CgsTime.h:188
        return lNewTime;
    }

    inline Time&
    Time::operator+=(const Time& lTime)
    {
        miSeconds  += lTime.miSeconds;
        mfFraction += lTime.mfFraction;

        // X360 @0x8230E7B8: 0x8230E7F0 fcmpu f, 1.0 (flt_82001C98) ; 0x8230E7F4 blt -> no carry, so a
        // NaN fraction falls through to the carry at 0x8230E7F8.
        if (!(mfFraction < 1.f))
        {
            mfFraction -= 1.f;
            miSeconds++;
        }

        CGS_ASSERT(mfFraction < 1.f, "Fraction out of range\n");   // CgsTime.h:205
        return *this;
    }

    inline Time&
    Time::operator-=(const Time& lTime)
    {
        miSeconds  -= lTime.miSeconds;
        mfFraction -= lTime.mfFraction;

        if (mfFraction < 0.f)
        {
            mfFraction += 1.f;
            miSeconds--;
        }

        CGS_ASSERT(mfFraction < 1.f, "Fraction out of range\n");   // CgsTime.h:222
        return *this;
    }

    inline void
    Time::SetFraction(f32 lfFraction)
    {
        CGS_ASSERT((lfFraction >= 0.f) && (lfFraction < 1.f), "Fraction out of range\n");   // CgsTime.h:316
        mfFraction = lfFraction;
    }

    // ---- comparisons / accessors (filled from Feb-2007 leak) -------------------

    inline bool
    Time::operator>(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return true; }
        else if (miSeconds < lTime.miSeconds) { return false; }
        else                                  { return (mfFraction > lTime.mfFraction); }
    }

    inline bool
    Time::operator<(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return false; }
        else if (miSeconds < lTime.miSeconds) { return true; }
        else                                  { return (mfFraction < lTime.mfFraction); }
    }

    // Inlined, e.g. GenerateFirstOrLastMessage 0x823958A0 li r11, 1 ; 0x823958A8 fcmpu fa, fb ;
    // 0x823958AC bge keep ; li r11, 0 (and 0x825503B0..0x825503BC): a NaN fraction answers TRUE.
    inline bool
    Time::operator>=(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return true; }
        else if (miSeconds < lTime.miSeconds) { return false; }
        else                                  { return !(mfFraction < lTime.mfFraction); }
    }

    // Inlined, e.g. 0x8255040C li r11, 1 ; 0x82550414 fcmpu fa, fb ; 0x82550418 ble keep ; li r11, 0:
    // a NaN fraction answers TRUE.
    inline bool
    Time::operator<=(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return false; }
        else if (miSeconds < lTime.miSeconds) { return true; }
        else                                  { return !(mfFraction > lTime.mfFraction); }
    }

    inline f32
    Time::GetFraction() const
    {
        return mfFraction;
    }

    inline s32
    Time::GetSeconds() const
    {
        return miSeconds;
    }

    inline f32
    Time::GetFloatVal() const
    {
        return (static_cast<f32>(miSeconds) + mfFraction);
    }

    inline void
    Time::SetSeconds(s32 liSeconds)
    {
        miSeconds = liSeconds;
    }

    // X360 @0x821F2140: 0x821F2160 fcmpu t, 0.0 (flt_82001CC0) ; 0x821F2164 bge -> past the assert.
    inline void
    Time::SetFloatVal(f32 lfFloatVal)
    {
        CGS_ASSERT(!(lfFloatVal < 0.f), "Time out of range\n");   // CgsTime.h:329
        miSeconds  = static_cast<s32>(lfFloatVal);
        mfFraction = static_cast<f32>(lfFloatVal - static_cast<f32>(miSeconds));
    }
}
