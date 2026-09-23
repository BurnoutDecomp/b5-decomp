// DirtySDK platform -- time helpers (platform/plat-time.c): the epoch clock and the
// epoch <-> struct tm conversions the tagfield epoch fields use (UTC, no time zone).

#include "platform.h"

#include <ctime>

extern "C" u32 ds_timeinsecs(void)
{
    return (u32)time(NULL);
}

extern "C" struct tm* ds_secstotime(struct tm* tm, u32 elap)
{
    s32 year;
    s32 leap;
    s32 next;
    s32 days;
    s32 secs;
    const s32* mon;
    // days per month, leap year row first
    static const s32 dayspermonth[24] =
    {
        31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };

    days = (s32)(elap / 86400);
    secs = (s32)(elap % 86400);
    tm->tm_sec = secs % 60;
    tm->tm_hour = (secs / 60) / 60;
    tm->tm_min = (secs / 60) % 60;

    // whole years; jump ahead by a lower bound of the years left, then correct
    for (year = 1970; ; year = next)
    {
        leap = (((year & 3) == 0) && (((year % 100) != 0) || ((year % 400) == 0))) ? 366 : 365;
        if (days < leap)
        {
            break;
        }
        next = year + (days / 366);
        if (next == year)
        {
            ++next;
        }
        days += (365 * (year - next)) + ((year - 1) / 400) - ((next - 1) / 400) - ((year - 1) / 100) +
                ((next - 1) / 100) + ((year - 1) / 4) - ((next - 1) / 4);
    }
    tm->tm_yday = days;
    tm->tm_year = year - 1900;

    // whole months (a 365-day year uses the second row)
    tm->tm_mon = 0;
    for (mon = dayspermonth + (12 * (leap & 1)); days >= *mon; ++mon)
    {
        days -= *mon;
        tm->tm_mon += 1;
    }
    tm->tm_isdst = 0;
    tm->tm_mday = days + 1;
    return tm;
}

// Binary search over the whole 32-bit range with ds_secstotime.
extern "C" u32 ds_timetosecs(const struct tm* tm)
{
    s32 res;
    struct tm cmp;
    u32 min = 0;
    u32 max = 0xFFFFFFFFu;
    u32 mid;

    do
    {
        mid = (min >> 1) + (max >> 1) + (min & max & 1);
        ds_secstotime(&cmp, mid);
        if (((res = cmp.tm_year - tm->tm_year) == 0) && ((res = cmp.tm_mon - tm->tm_mon) == 0) &&
            ((res = cmp.tm_mday - tm->tm_mday) == 0) && ((res = cmp.tm_hour - tm->tm_hour) == 0) &&
            ((res = cmp.tm_min - tm->tm_min) == 0) && ((res = cmp.tm_sec - tm->tm_sec) == 0))
        {
            break;
        }
        if (min == max)
        {
            break;
        }
        if (res > 0)
        {
            max = mid - 1;
        }
        else
        {
            min = mid + 1;
        }
    }
    while (min <= max);

    return (res == 0) ? mid : 0;
}
