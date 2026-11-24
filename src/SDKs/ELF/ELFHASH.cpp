#include "SDKs/Packages/Lion/Final/eauk_common/Common/ElfHash.h"

s32 ELFHASH::BuildHash(const char* lpcText, char lcTerminator)
{
    u32 luHash = 0;

    for (; *lpcText != 0; ++lpcText)
    {
        char lcChar = *lpcText;
        if (lcTerminator == lcChar)
            break;

        if (lcChar >= 'A' && lcChar <= 'Z')
        {
            lcChar = static_cast<char>(lcChar + ('a' - 'A'));
        }

        // ARTIST 0x8290816C sign-extends the byte before adding it to the hash.
        luHash = (luHash << 4) + lcChar;
        const u32 luHighNibble = luHash & 0xf0000000;
        if (luHighNibble != 0)
        {
            luHash ^= luHighNibble >> 24;
        }

        luHash &= ~luHighNibble;
    }

    return static_cast<s32>(luHash);
}
