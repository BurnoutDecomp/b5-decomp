#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// CgsSystem::DateAndTime - X360 ARTIST build, Win32-subset time API.
//
// Every body maps 1:1 to the recovered X360 calls. The C++ member mFileTime is accessed
// by name and only reinterpreted to the OS FILETIME at the syscall boundary (it has the
// same {dwLowDateTime, dwHighDateTime} layout). FileTime <-> FILETIME and the local
// SYSTEMTIME stack temporaries are the platform-shim seam.

namespace
{
    inline FILETIME* AsFileTime(CgsSystem::DateAndTime::FileTime& lFileTime)
    {
        return reinterpret_cast<FILETIME*>(&lFileTime);
    }

    inline const FILETIME* AsFileTime(const CgsSystem::DateAndTime::FileTime& lFileTime)
    {
        return reinterpret_cast<const FILETIME*>(&lFileTime);
    }
}

namespace CgsSystem
{
    DateAndTime::DateAndTime()
        : mbIsLocal(false)
    {
        mFileTime.muLowDateTime  = 0;
        mFileTime.muHighDateTime = 0;
    }

    // 0x828D6A68 - GetLocalTime/GetSystemTime depending on mbIsLocal, then store as FILETIME.
    void DateAndTime::Update()
    {
        SYSTEMTIME lSystemTime;
        if (mbIsLocal)
        {
            GetLocalTime(&lSystemTime);
        }
        else
        {
            GetSystemTime(&lSystemTime);
        }
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    // --- getters: FileTimeToSystemTime(this+4, &st); return the matching SYSTEMTIME field ---

    s32 DateAndTime::GetYear() const   // 0x828D6AE8 -> wYear   (+0)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wYear;
    }

    s32 DateAndTime::GetMonth() const  // wMonth (+2)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wMonth;
    }

    s32 DateAndTime::GetDay() const    // 0x828D6B48 -> wDay    (+6)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wDay;
    }

    s32 DateAndTime::GetHour() const   // 0x828D6B78 -> wHour   (+8)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wHour;
    }

    s32 DateAndTime::GetMinute() const // 0x828D6BA8 -> wMinute (+10)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wMinute;
    }

    s32 DateAndTime::GetSecond() const // 0x828D6BD8 -> wSecond (+12)
    {
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        return lSystemTime.wSecond;
    }

    // --- setters: assert bounds; FileTimeToSystemTime; set one field; SystemTimeToFileTime back ---

    void DateAndTime::SetYear(s32 liYear)   // 0x828D6C08
    {
        CGS_ASSERT(liYear >= 0,    "liYear >= 0");
        CGS_ASSERT(liYear < 5000,  "liYear < 5000");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wYear = static_cast<WORD>(liYear);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    void DateAndTime::SetMonth(s32 liMonth) // 0x828D6C98
    {
        CGS_ASSERT(liMonth >= 0,   "liMonth >= 0");
        CGS_ASSERT(liMonth <= 12,  "liMonth <= 12");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wMonth = static_cast<WORD>(liMonth);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    void DateAndTime::SetDay(s32 liDay)     // 0x828D6D28
    {
        CGS_ASSERT(liDay >= 0,     "liDay >= 0");
        CGS_ASSERT(liDay <= 31,    "liDay <= 31");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wDay = static_cast<WORD>(liDay);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    void DateAndTime::SetHour(s32 liHour)   // 0x828D6DB8
    {
        CGS_ASSERT(liHour >= 0,    "liHour >= 0");
        CGS_ASSERT(liHour < 24,    "liHour < 24");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wHour = static_cast<WORD>(liHour);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    void DateAndTime::SetMinute(s32 liMinute) // 0x828D6E48
    {
        CGS_ASSERT(liMinute >= 0,  "liMinute >= 0");
        CGS_ASSERT(liMinute < 60,  "liMinute < 60");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wMinute = static_cast<WORD>(liMinute);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    void DateAndTime::SetSecond(s32 liSecond) // 0x828D6ED8
    {
        CGS_ASSERT(liSecond >= 0,  "liSecond >= 0");
        CGS_ASSERT(liSecond < 60,  "liSecond < 60");
        SYSTEMTIME lSystemTime;
        FileTimeToSystemTime(AsFileTime(mFileTime), &lSystemTime);
        lSystemTime.wSecond = static_cast<WORD>(liSecond);
        SystemTimeToFileTime(&lSystemTime, AsFileTime(mFileTime));
    }

    // 0x828D6F68 - the raw 64-bit time key.
    //
    // The X360 composes this in STORAGE ORDER, not in Win32-FILETIME-semantic order:
    // the asm does `(*(this+4) << 32) + *(this+8)` (lwz r11,4 ; extldi r11,r11,64,32 ;
    // lwz r10,8 ; add). Since mFileTime.muLowDateTime lives at +4 and muHighDateTime at
    // +8, that places the FILETIME's LOW dword in the HIGH 32 bits -- i.e. NOT the
    // semantic (high<<32)|low FILETIME value. This is faithful to the binary: the value
    // is only ever used as a monotonic comparison/subtraction key (the operators and
    // GetDifferenceInSeconds all run on it), and matching the X360 composition exactly is
    // what keeps those callers (e.g. BrnProgression::Profile) in parity. Do NOT "correct"
    // this to the semantic FILETIME order.
    u64 DateAndTime::GetRawTimeValue() const
    {
        return (static_cast<u64>(mFileTime.muLowDateTime) << 32)
             + static_cast<u64>(mFileTime.muHighDateTime);
    }

    // 0x828E12E0 - difference in whole seconds between two times.
    //
    // The X360 body asserts both operands are initialised (high and low both non-zero),
    // then computes the signed 64-bit FILETIME delta and divides by 10,000,000 (0x989680,
    // the number of 100-ns ticks per second). The lengthy debug-stream path in the asm
    // (the `Difference is :` / `Hex:` formatting via the StrStream operator<< at
    // sub_828E0518 / sub_821F0E50) only runs inside an overflow assert and contributes
    // nothing to the return value, so it is intentionally omitted. The overflow check
    // itself (result == 0x7FFFFFFF) is preserved as an assert.
    s32 DateAndTime::GetDifferenceInSeconds(const DateAndTime& lDateAndTime) const
    {
        CGS_ASSERT(mFileTime.muHighDateTime != 0 && mFileTime.muLowDateTime != 0,
                   "Time not initalised\n");
        CGS_ASSERT(lDateAndTime.mFileTime.muHighDateTime != 0
                       && lDateAndTime.mFileTime.muLowDateTime != 0,
                   "Time not initalised\n");

        const s64 lThisTicks  = static_cast<s64>(GetRawTimeValue());
        const s64 lOtherTicks = static_cast<s64>(lDateAndTime.GetRawTimeValue());
        const s64 lDifference = (lThisTicks - lOtherTicks) / 10000000;

        CGS_ASSERT(lDifference != 0x7FFFFFFF, "Difference overflow\n");

        return static_cast<s32>(lDifference);
    }

    // --- inlined-only in the X360 build (no standalone address); behaviour from DWARF ---

    void DateAndTime::SetLocal(bool lbIsLocal)
    {
        mbIsLocal = lbIsLocal;
    }

    void DateAndTime::Clear()
    {
        mFileTime.muLowDateTime  = 0;
        mFileTime.muHighDateTime = 0;
    }

    bool DateAndTime::IsLocal() const
    {
        return mbIsLocal;
    }

    bool DateAndTime::IsZero() const
    {
        return mFileTime.muLowDateTime == 0 && mFileTime.muHighDateTime == 0;
    }

    bool DateAndTime::operator==(const DateAndTime& lDateAndTime) const
    {
        return GetRawTimeValue() == lDateAndTime.GetRawTimeValue();
    }

    bool DateAndTime::operator<(const DateAndTime& lDateAndTime) const
    {
        return GetRawTimeValue() < lDateAndTime.GetRawTimeValue();
    }

    bool DateAndTime::operator<=(const DateAndTime& lDateAndTime) const
    {
        return GetRawTimeValue() <= lDateAndTime.GetRawTimeValue();
    }

    bool DateAndTime::operator>(const DateAndTime& lDateAndTime) const
    {
        return GetRawTimeValue() > lDateAndTime.GetRawTimeValue();
    }

    bool DateAndTime::operator>=(const DateAndTime& lDateAndTime) const
    {
        return GetRawTimeValue() >= lDateAndTime.GetRawTimeValue();
    }
}
