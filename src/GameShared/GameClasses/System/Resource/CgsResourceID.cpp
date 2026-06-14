#include "types.hpp"

namespace CgsResource
{
class ID
{
public:
    static s32 HashString(const u8* lpString);
};

extern const u32 gauCrcTable[256];

s32 ID::HashString(const u8* lpString)
{
    if (!*lpString)
        return 0;

    u32 luCrc = 0xffffffff;
    for (u8 luChar = *lpString; luChar; luChar = *++lpString)
    {
        if (luChar >= 'A' && luChar <= 'Z')
            luChar = static_cast<u8>(luChar + ('a' - 'A'));

        luCrc = gauCrcTable[(luCrc ^ luChar) & 0xff] ^ (luCrc >> 8);
    }

    return static_cast<s32>(~luCrc);
}
}
