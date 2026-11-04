#include "types.hpp"

// ARTIST 0x82B302C8. Emits one UCS-2 code point without a terminator.
extern "C" s32 Utf8EncodeFromUCS2CodePt(char* pOut, u16 uCodePoint)
{
    if (uCodePoint < 0x80)
    {
        pOut[0] = static_cast<char>(uCodePoint);
        return 1;
    }
    if (uCodePoint < 0x800)
    {
        pOut[1] = static_cast<char>(0x80 | (uCodePoint & 0x3f));
        pOut[0] = static_cast<char>(0xc0 | (uCodePoint >> 6));
        return 2;
    }
    pOut[2] = static_cast<char>(0x80 | (uCodePoint & 0x3f));
    pOut[1] = static_cast<char>(0x80 | ((uCodePoint >> 6) & 0x3f));
    pOut[0] = static_cast<char>(0xe0 | (uCodePoint >> 12));
    return 3;
}

// ARTIST 0x82B30340. The return count includes the terminating byte.
extern "C" s32 Utf8EncodeFromUCS2(char* pOutStr, s32 iBufLen, const u16* pInStr)
{
    const s32 iLimit = iBufLen - 2;
    s32 iStrLen = 0;
    while (*pInStr && iStrLen < iLimit)
    {
        iStrLen += Utf8EncodeFromUCS2CodePt(pOutStr + iStrLen, *pInStr);
        ++pInStr;
    }
    pOutStr[iStrLen] = 0;
    return iStrLen + 1;
}
