// b5-decomp/src/GameShared/GameClasses/System/Timer/CgsTime.h
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSystem::Time - a lightweight game-time value type: an integer second count plus a
// normalised [0,1) sub-second fraction. Defined entirely inline (a header-only value
// class). Layout + API authoritative from DecFIGS DWARF (System/Timer/CgsTime.h:33):
// miSeconds @ +0 (int32), mfFraction @ +4 (float32); full inline logic matches the
// Feb-2007 leak source verbatim (the X360 binary agrees, including operator-'s
// clamp-fraction-to-0 branch). The original streamed the assert message through a
// CgsDev::StrStream temporary; that lowers to the project's CGS_ASSERT.
//
// This TU's six binary-recovered functions are operator=(f32), operator+, operator-,
// operator+=, operator-=, SetFraction. The remaining inline bodies (ctors, copy/assign,
// comparisons, Get*/Set*) are filled from the Feb-2007 leak so the header links
// standalone (operator+/operator- construct `Time lNewTime;` and return by value, so
// they depend on the default ctor being defined here).
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

    inline Time::Time(f32 lfTime)
    {
        CGS_ASSERT(lfTime >= 0.f, "Time out of range\n");   // CgsTime.h:115
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

    inline Time&
    Time::operator=(f32 lfTime)
    {
        CGS_ASSERT(lfTime >= 0.f, "Time out of range\n");   // CgsTime.h:137
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

        if (lNewTime.mfFraction >= 1.f)
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
            if (lNewTime.mfFraction >= 1.0f)
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

        if (mfFraction >= 1.f)
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

    inline bool
    Time::operator>=(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return true; }
        else if (miSeconds < lTime.miSeconds) { return false; }
        else                                  { return (mfFraction >= lTime.mfFraction); }
    }

    inline bool
    Time::operator<=(const Time& lTime) const
    {
        if (miSeconds > lTime.miSeconds)      { return false; }
        else if (miSeconds < lTime.miSeconds) { return true; }
        else                                  { return (mfFraction <= lTime.mfFraction); }
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

    inline void
    Time::SetFloatVal(f32 lfFloatVal)
    {
        CGS_ASSERT(lfFloatVal >= 0.f, "Time out of range\n");   // CgsTime.h:329
        miSeconds  = static_cast<s32>(lfFloatVal);
        mfFraction = static_cast<f32>(lfFloatVal - static_cast<f32>(miSeconds));
    }
}
