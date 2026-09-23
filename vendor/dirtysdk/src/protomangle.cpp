// DirtySDK protomangle -- session encoding (ProtoMangleEncodeSession).
//
// The invite pipeline carries a session as text: '$' followed by the host address (36 bytes),
// the key-exchange key (16 bytes) and the session id (8 bytes), each re-packed seven bits per
// output byte (least significant bits first, top bit always set), then a terminator. The SDK's
// debug dumps of the three fields and of the result are not reproduced.

#include "protomangle.h"
#include "dirtylib.h"   // NetPrintf

// Pack iInputLen bytes of pInput into pOutput, seven bits per byte with bit 7 set; append a
// terminator when bTerminate. Returns the next output position, or 0 (after a debug print)
// when pOutput cannot hold the result.
static char* _ProtoMangleEncode7(char* pOutput, s32 iOutputLen, const u8* pInput, s32 iInputLen, s32 bTerminate)
{
    u32 uBits = 0;
    s32 iNumBits = 0;

    if (iOutputLen < ((8 * iInputLen + 6) / 7) + bTerminate)
    {
        NetPrintf(("protomanglexenon: not enough room to encode system link advert\n"));
        return 0;
    }

    for (; iInputLen > 0; --iInputLen)
    {
        uBits |= (u32)(*pInput++) << iNumBits;
        iNumBits += 8;
        if (iNumBits >= 7)
        {
            u32 uCount = (u32)iNumBits / 7u;
            iNumBits = (s32)((u32)iNumBits % 7u);
            do
            {
                *pOutput++ = (char)((uBits & 0x7F) | 0x80);
                uBits >>= 7;
            } while (--uCount != 0);
        }
    }
    if (iNumBits > 0)
    {
        *pOutput++ = (char)((uBits & 0x7F) | 0x80);
    }
    if (bTerminate)
    {
        *pOutput = 0;
    }
    return pOutput;
}

extern "C" void ProtoMangleEncodeSession(char* pBuffer, s32 iBufSize, const void* pSessionInfo)
{
    const u8* pInfo = static_cast<const u8*>(pSessionInfo);
    char* pEnd = pBuffer + iBufSize;

    pBuffer[0] = '$';
    char* pOutput = _ProtoMangleEncode7(pBuffer + 1, (s32)(pEnd - (pBuffer + 1)), pInfo + 0x08, 36, 0);
    pOutput = _ProtoMangleEncode7(pOutput, (s32)(pEnd - pOutput), pInfo + 0x2C, 16, 0);
    _ProtoMangleEncode7(pOutput, (s32)(pEnd - pOutput), pInfo + 0x00, 8, 1);
}
