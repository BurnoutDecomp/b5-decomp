// DirtySDK ds_strnzcpy, ARTIST 0x82B2AD30.
#include "types.hpp"
#include <cstring>

extern "C" char* ds_strnzcpy(char* dst, const char* src, s32 size)
{
    std::strncpy(dst, src, size);
    dst[size - 1] = 0;
    return dst;
}
// ARTIST 0x82B2AC40: ASCII case folding, retaining signed-byte comparison.
extern "C" s32 ds_stricmp(const char* lhs, const char* rhs)
{
    s32 a, b;
    do
    {
        a = static_cast<signed char>(*lhs++);
        b = static_cast<signed char>(*rhs++);
        if (a >= 'A' && a <= 'Z') a ^= 0x20;
        if (b >= 'A' && b <= 'Z') b ^= 0x20;
    } while (a && a == b);
    return a - b;
}

// ARTIST 0x82B2AB60: empty strings and null arguments do not match.
#include <cctype>
extern "C" char* ds_stristr(const char* text, const char* pattern)
{
    if (!text || !*text || !pattern || !*pattern) return 0;
    const s32 first = std::tolower(static_cast<unsigned char>(*pattern));
    for (const char* p = text; *p; ++p)
    {
        if (std::tolower(static_cast<unsigned char>(*p)) != first) continue;
        const char* a = p + 1;
        const char* b = pattern + 1;
        while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) ==
                            std::tolower(static_cast<unsigned char>(*b)))
        {
            ++a;
            ++b;
        }
        if (!*b) return const_cast<char*>(p);
    }
    return 0;
}

// ARTIST 0x82B2AD78.
extern "C" s32 ds_strnzcat(char* dst, const char* src, s32 size)
{
    const s32 end = size - 1;
    s32 len = 0;
    if (end >= 0)
    {
        while (len < end && dst[len]) ++len;
        while (len < end && *src) dst[len++] = *src++;
        dst[len] = 0;
    }
    return len;
}
// ARTIST 0x82B2ACA8, bounded signed-byte ASCII case comparison.
extern "C" s32 ds_strnicmp(const char* lhs, const char* rhs, u32 count)
{
    if (!count) return 0;
    s32 a, b;
    u32 i = 0;
    do
    {
        a = static_cast<signed char>(*lhs++);
        b = static_cast<signed char>(*rhs++);
        if (a >= 'A' && a <= 'Z') a ^= 0x20;
        if (b >= 'A' && b <= 'Z') b ^= 0x20;
        ++i;
    } while (a && a == b && i < count);
    return a - b;
}
