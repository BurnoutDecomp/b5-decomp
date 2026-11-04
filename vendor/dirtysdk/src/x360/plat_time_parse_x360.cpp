// DirtySDK ds_strtotime, ARTIST 0x82B306E8.
#include "types.hpp"
#include "platform.h"
#include <ctime>
#include <cstring>

namespace {
bool Digit(char c) { return c >= '0' && c <= '9'; }
s32 Number(const char*& s)
{
    s32 value = 0;
    while (Digit(*s)) { value = (*s++ & 15) + 10 * value; }
    return value;
}
s32 ShortTimeNumber(const char*& s)
{
    s32 value = *s++ & 15;
    if (Digit(*s)) value = (*s++ & 15) + 10 * value;
    return value;
}
s32 Match3(const char* s, const char* const* words)
{
    for (s32 i = 0; words[i]; ++i)
        if (s[0] == words[i][0] && s[1] == words[i][1] && s[2] == words[i][2]) return i;
    return -1;
}
}

extern "C" u32 ds_strtotime(const char* s)
{
    static const char* const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", 0};
    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 0};
    tm value;
    std::memset(&value, 0xff, sizeof(value));
    while (*s && *s <= ' ') ++s;
    s32 weekday = Match3(s, weekdays);
    if (weekday >= 0)
    {
        value.tm_wday = weekday;
        while (*s != ',' && *s != ' ' && *s) ++s;
        while (*s == ',' || *s == ' ') ++s;
    }
    if (!Digit(*s))
    {
        s32 month = Match3(s, months);
        if (month >= 0)
        {
            value.tm_mon = month;
            while (*s && !Digit(*s)) ++s;
            s32 day = Number(s);
            if (day > 0) value.tm_mday = day;
        }
    }
    if (value.tm_mon < 0 && Digit(*s) && (s[1] > '@' || s[2] > '@' || s[3] > '@'))
    {
        value.tm_mday = Number(s);
        while (*s < '@') ++s;
        s32 month = Match3(s, months);
        if (month >= 0)
        {
            value.tm_mon = month;
            while (*s && *s != ' ') ++s;
        }
    }
    if (Digit(*s) && value.tm_mon < 0)
    {
        value.tm_mon = Number(s);
        if (*s) ++s;
        value.tm_mday = Number(s);
        if (*s) ++s;
    }
    while (*s && !Digit(*s)) ++s;
    if (Digit(*s) && s[1] != ':' && s[2] != ':')
    {
        s32 year = Number(s);
        if (year <= 999) year += year < 50 ? 2000 : 1900;
        value.tm_year = year;
        while (*s && !Digit(*s)) ++s;
    }
    if (Digit(*s))
    {
        value.tm_hour = ShortTimeNumber(s);
        if (*s == ':') ++s;
    }
    if (Digit(*s))
    {
        value.tm_min = ShortTimeNumber(s);
        if (*s == ':') ++s;
    }
    if (Digit(*s)) value.tm_sec = ShortTimeNumber(s);
    if (value.tm_year < 0)
    {
        s32 year = Number(s);
        if (year > 999) value.tm_year = year;
    }
    if (value.tm_year > 1900) value.tm_year -= 1900;
    return ds_timetosecs(&value);
}
