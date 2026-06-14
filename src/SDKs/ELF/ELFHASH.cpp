#include "SDKs/Packages/Lion/Final/eauk_common/Common/ElfHash.h"

s32 ELFHASH::BuildHash(const char* lpcText, char lcTerminator)
{
    s32 liHash = 0;

    for (char lcChar = *lpcText; lcChar != 0 && lcChar != lcTerminator; lcChar = *++lpcText)
    {
        if (lcChar >= 'A' && lcChar <= 'Z')
        {
            lcChar = static_cast<char>(lcChar + ('a' - 'A'));
        }

        s32 liNextHash = (liHash << 4) + static_cast<unsigned char>(lcChar);
        const u32 luHighNibble = static_cast<u32>(liNextHash) & 0xf0000000;
        if (luHighNibble != 0)
        {
            liNextHash ^= static_cast<s32>(luHighNibble >> 24);
        }

        liHash = static_cast<s32>(static_cast<u32>(liNextHash) & ~luHighNibble);
    }

    return liHash;
}
