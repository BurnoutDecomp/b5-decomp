// DirtySDK dirtysock -- machine address conversion (dirtyaddr).
//
// On this platform a DirtyAddrT is "$" followed by 16 lowercase hex digits of the
// 64-bit host address (the XUID). Only the two conversions the game build carries are
// reconstructed.

#include "dirtyaddr.h"
#include "dirtylib.h"   // NetPrintf

#include <cstdint>
#include <cstring>

static const char _DirtyAddr_strHex[16] =
{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};

extern "C" u32 DirtyAddrToHostAddr(void* pOutput, s32 iBufLen, const DirtyAddrT* pAddr)
{
    const char* pDigit;
    u64 uAddress;

    if ((u32)iBufLen < sizeof(uAddress))
    {
        NetPrintf(("dirtyaddr: output buffer too small\n"));
        return 0;
    }

    // skip the '$', then accumulate hex digits. An upper-case digit passes the digit
    // test but is converted with the lower-case offset, exactly as the SDK does.
    for (pDigit = pAddr->strMachineAddr + 1, uAddress = 0; ; ++pDigit)
    {
        const char cDigit = *pDigit;
        const unsigned char uDigit = (unsigned char)cDigit;
        if (!(((cDigit >= 'a') && (cDigit <= 'f')) || ((cDigit >= 'A') && (cDigit <= 'F')) ||
              ((u32)(cDigit - '0') <= 9)))
        {
            break;
        }
        uAddress = (uAddress << 4) |
                   (unsigned char)(uDigit - (((uDigit >= '0') && (uDigit <= '9')) ? '0' : ('a' - 10)));
    }
    memcpy(pOutput, &uAddress, sizeof(uAddress));
    return 1;
}

extern "C" u32 DirtyAddrFromHostAddr(DirtyAddrT* pAddr, const void* pInput)
{
    u64 uAddress;
    s32 iIndex;

    if ((pInput == NULL) || ((reinterpret_cast<uintptr_t>(pInput) & 3) != 0))
    {
        return 0;
    }
    memcpy(&uAddress, pInput, sizeof(uAddress));
    memset(pAddr, 0, sizeof(*pAddr));
    for (iIndex = 16; iIndex >= 1; --iIndex)
    {
        pAddr->strMachineAddr[iIndex] = _DirtyAddr_strHex[uAddress & 15];
        uAddress >>= 4;
    }
    pAddr->strMachineAddr[0] = '$';
    return 1;
}
