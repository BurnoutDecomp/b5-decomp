// DirtySDK lobbycheat.c, ARTIST 82B34700; checks only configured native ranges.
#include "types.hpp"
#include "dirtylib.h"
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
extern "C" void CryptMD5Init(CryptMD5T*);
extern "C" void CryptMD5Update(CryptMD5T*, const void*, s32);
extern "C" void CryptMD5Final(CryptMD5T*, void*, s32);
static s32 _Cheat_uMinRange = 0x00100000;
static s32 _Cheat_uMaxRange = 0x02000000;
static const char* _CheatParseHex(const char* p, s32* pValue)
{
    u32 value = 0;
    char c;
    while ((c = *p) != 0 && ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
    {
        if (c > '9') c = static_cast<char>(c + 9);
        value = (value << 4) | (c & 15);
        ++p;
    }
    *pValue = static_cast<s32>(value);
    return p;
}
static const char* _CheatParseRange(const char* p, s32* pStart, s32* pEnd)
{
    p = _CheatParseHex(p, pStart);
    if (*p != ':') return 0;
    p = _CheatParseHex(p + 1, pEnd);
    return *p == ',' ? p + 1 : p;
}
extern "C" void CheatCalcResponse(const char* pStrRange, u32 uStrSize, unsigned char* pOutput, u32 uOutputSize)
{
    CryptMD5T ctx;
    CryptMD5Init(&ctx);
    CryptMD5Update(&ctx, pStrRange, static_cast<s32>(uStrSize));
    const char* pNext = pStrRange;
    // ARTIST 82B34744 loads the end value without initialization; a malformed
    // range without ':' can reach the same indeterminate comparison.
    s32 iEnd;
    while (pNext)
    {
        s32 iStart;
        pNext = _CheatParseRange(pNext, &iStart, &iEnd);
        if (!iStart || !iEnd) break;
        NetPrintfCode("CheatCalcResponse: Range to check: [%08X:%08X]\n", iStart, iEnd);
        if (iStart < _Cheat_uMinRange || iEnd >= _Cheat_uMaxRange || iStart >= iEnd)
            NetPrintfCode("CheatCalcReponse: WARNING, invalid range [0x%08x, 0x%08x) rejected.\n", iStart, iEnd);
        else
            CryptMD5Update(&ctx, reinterpret_cast<const void*>(iStart), iEnd - iStart);
    }
    CryptMD5Update(&ctx, pStrRange, static_cast<s32>(uStrSize));
    CryptMD5Final(&ctx, pOutput, static_cast<s32>(uOutputSize));
}
