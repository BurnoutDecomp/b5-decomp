// Native DirtySDK NetPrintMemCode, ARTIST 0x82B30160.
#include "types.hpp"
#include "dirtylib.h"
#include <ctype.h>
#include <string.h>

extern "C" void NetPrintMemCode(const void* pMem, s32 iSize, const char* pTitle)
{
    static const char kHex[] = "0123456789ABCDEF";
    char strLine[128];
    memset(strLine, ' ', 127);
    strLine[127] = 0;
    s32 iPos = 2;
    NetPrintfCode("dirtylib: dumping memory for object %s (%d bytes)\n", pTitle, iSize);
    const u8* pData = static_cast<const u8*>(pMem);
    s32 i;
    for (i = 0; i < iSize; ++i)
    {
        u32 uByte = pData[i];
        strLine[iPos] = kHex[uByte >> 4];
        strLine[iPos + 1] = kHex[uByte & 15];
        if ((__pctype_func()[uByte] & 0x157) == 0)
        {
            uByte = '.';
        }
        strLine[40 + iPos / 2] = static_cast<char>(uByte);
        if (i > 0)
        {
            if (((i + 1) % 16) == 0)
            {
                strLine[41 + iPos / 2] = 0;
                NetPrintfCode("%s\n", strLine);
                memset(strLine, ' ', 127);
                strLine[127] = 0;
                iPos = 0;
            }
            else if (((i + 1) % 4) == 0)
            {
                ++iPos;
            }
        }
        iPos += 2;
    }
    if ((i % 16) != 0)
    {
        strLine[41 + iPos / 2] = 0;
        NetPrintfCode("%s\n", strLine);
    }
}
