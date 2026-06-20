#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

// CgsResource::ID::HashString -- faithful to ARTIST 0x828D84A8: a reflected CRC32 (polynomial 0xEDB88320,
// init 0xFFFFFFFF, final bitwise-NOT) over the string LOWERCASED (A-Z -> a-z first). The X360 uses a
// precomputed table (dword_820F71F0 = the standard reflected CRC32 table); the bit-by-bit form here is
// numerically identical and self-contained (the table's definition was never carried over). Resource ids
// are therefore case-insensitive name hashes -- VIDEOLIST.BUNDLE's VideoDataResource ids are HashString of
// the (lowercased) video names (e.g. "eafranchise", "criterion").

namespace CgsResource
{
    s32 ID::HashString(const u8* lpString)
    {
        if (lpString == 0 || *lpString == 0)
            return 0;

        u32 luCrc = 0xFFFFFFFFu;
        for (u8 luChar = *lpString; luChar; luChar = *++lpString)
        {
            if (luChar >= 'A' && luChar <= 'Z')
                luChar = static_cast<u8>(luChar + ('a' - 'A'));

            luCrc ^= luChar;
            for (s32 li = 0; li < 8; ++li)
                luCrc = (luCrc >> 1) ^ (0xEDB88320u & (0u - (luCrc & 1u)));
        }
        return static_cast<s32>(~luCrc);
    }
}
