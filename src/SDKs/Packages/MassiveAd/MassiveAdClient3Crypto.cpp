// MassiveAd client hashing: MD5 hex digest, the SHA-1 core and HMAC-SHA1 (vendor middleware).

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Crypto.h"

#include <cstring>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/LibTomCrypt/tomcrypt_massive.h"

#define SHA1CircularShift(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

static void SHA1PadMessage(SHA1Context* context);
static void SHA1ProcessMessageBlock(SHA1Context* context);

namespace MassiveAdClient3
{

static char sacMD5Hash[33];

char* CalculateMD5Hash(const void* pData, int nDataLength)
{
    hash_state lState;
    unsigned char lacDigest[16];

    md5_init(&lState);
    md5_process(&lState, static_cast<const unsigned char*>(pData), nDataLength);
    md5_done(&lState, lacDigest);

    char* lpcOut = sacMD5Hash;
    for (unsigned int i = 0; i < 16; ++i)
    {
        MassiveFormatString(lpcOut, 33, "%02x", lacDigest[i]);
        lpcOut += 2;
    }
    return sacMD5Hash;
}

} // namespace MassiveAdClient3

int SHA1Reset(SHA1Context* context)
{
    if (!context)
    {
        return shaNull;
    }

    context->Length_Low = 0;
    context->Length_High = 0;
    context->Message_Block_Index = 0;

    context->Intermediate_Hash[0] = 0x67452301;
    context->Intermediate_Hash[1] = 0xEFCDAB89;
    context->Intermediate_Hash[2] = 0x98BADCFE;
    context->Intermediate_Hash[3] = 0x10325476;
    context->Intermediate_Hash[4] = 0xC3D2E1F0;

    context->Computed = 0;
    context->Corrupted = 0;

    return shaSuccess;
}

static void SHA1ProcessMessageBlock(SHA1Context* context)
{
    const unsigned int K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    int t;
    unsigned int temp;
    unsigned int W[80];
    unsigned int A, B, C, D, E;

    for (t = 0; t < 16; t++)
    {
        W[t] = context->Message_Block[t * 4] << 24;
        W[t] |= context->Message_Block[t * 4 + 1] << 16;
        W[t] |= context->Message_Block[t * 4 + 2] << 8;
        W[t] |= context->Message_Block[t * 4 + 3];
    }

    for (t = 16; t < 80; t++)
    {
        W[t] = SHA1CircularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
    }

    A = context->Intermediate_Hash[0];
    B = context->Intermediate_Hash[1];
    C = context->Intermediate_Hash[2];
    D = context->Intermediate_Hash[3];
    E = context->Intermediate_Hash[4];

    for (t = 0; t < 20; t++)
    {
        temp = SHA1CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5, A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    for (t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
        E = D;
        D = C;
        C = SHA1CircularShift(30, B);
        B = A;
        A = temp;
    }

    context->Intermediate_Hash[0] += A;
    context->Intermediate_Hash[1] += B;
    context->Intermediate_Hash[2] += C;
    context->Intermediate_Hash[3] += D;
    context->Intermediate_Hash[4] += E;

    context->Message_Block_Index = 0;
}

static void SHA1PadMessage(SHA1Context* context)
{
    // The block cannot hold the 64-bit length: pad it out, process it, and continue padding
    // into a fresh block.
    if (context->Message_Block_Index > 55)
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while (context->Message_Block_Index < 64)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }

        SHA1ProcessMessageBlock(context);

        while (context->Message_Block_Index < 56)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }
    else
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while (context->Message_Block_Index < 56)
        {
            context->Message_Block[context->Message_Block_Index++] = 0;
        }
    }

    // The last 8 octets hold the message length, most significant first.
    context->Message_Block[56] = (unsigned char)(context->Length_High >> 24);
    context->Message_Block[57] = (unsigned char)(context->Length_High >> 16);
    context->Message_Block[58] = (unsigned char)(context->Length_High >> 8);
    context->Message_Block[59] = (unsigned char)(context->Length_High);
    context->Message_Block[60] = (unsigned char)(context->Length_Low >> 24);
    context->Message_Block[61] = (unsigned char)(context->Length_Low >> 16);
    context->Message_Block[62] = (unsigned char)(context->Length_Low >> 8);
    context->Message_Block[63] = (unsigned char)(context->Length_Low);

    SHA1ProcessMessageBlock(context);
}

int SHA1Result(SHA1Context* context, unsigned char Message_Digest[SHA1HashSize])
{
    short i;

    if (!context || !Message_Digest)
    {
        return shaNull;
    }

    if (context->Corrupted)
    {
        return context->Corrupted;
    }

    if (!context->Computed)
    {
        SHA1PadMessage(context);
        for (i = 0; i < 64; ++i)
        {
            // message may be sensitive, clear it out
            context->Message_Block[i] = 0;
        }
        context->Length_Low = 0;
        context->Length_High = 0;
        context->Computed = 1;
    }

    for (i = 0; i < SHA1HashSize; ++i)
    {
        Message_Digest[i] = (unsigned char)(context->Intermediate_Hash[i >> 2] >> 8 * (3 - (i & 0x03)));
    }

    return shaSuccess;
}

int SHA1Input(SHA1Context* context, const unsigned char* message_array, unsigned int length)
{
    if (!length)
    {
        return shaSuccess;
    }

    if (!context || !message_array)
    {
        return shaNull;
    }

    if (context->Computed)
    {
        context->Corrupted = shaStateError;
        return shaStateError;
    }

    if (context->Corrupted)
    {
        return context->Corrupted;
    }

    while (length-- && !context->Corrupted)
    {
        context->Message_Block[context->Message_Block_Index++] = (*message_array & 0xFF);

        context->Length_Low += 8;
        if (context->Length_Low == 0)
        {
            context->Length_High++;
            if (context->Length_High == 0)
            {
                // message is too long
                context->Corrupted = 1;
            }
        }

        if (context->Message_Block_Index == 64)
        {
            SHA1ProcessMessageBlock(context);
        }

        message_array++;
    }

    return shaSuccess;
}

namespace MassiveAdClient3
{

void* CalculateSHA1HMac(const void* pData, int nDataLength, const char* pcKey, int nKeyLength)
{
    SHA1Context lContext;
    unsigned char lacDigest[SHA1HashSize];
    unsigned char lacInnerPad[65];
    unsigned char lacOuterPad[65];

    const unsigned char* lpKey = reinterpret_cast<const unsigned char*>(pcKey);
    if (nKeyLength > 64)
    {
        SHA1Reset(&lContext);
        SHA1Input(&lContext, lpKey, nKeyLength);
        SHA1Result(&lContext, lacDigest);
        lpKey = lacDigest;
        nKeyLength = SHA1HashSize;
    }

    std::memset(lacInnerPad, 0, sizeof(lacInnerPad));
    std::memset(lacOuterPad, 0, sizeof(lacOuterPad));
    std::memcpy(lacInnerPad, lpKey, nKeyLength);
    std::memcpy(lacOuterPad, lpKey, nKeyLength);

    for (int i = 0; i < 64; ++i)
    {
        lacInnerPad[i] ^= 0x36;
        lacOuterPad[i] ^= 0x5C;
    }

    SHA1Reset(&lContext);
    SHA1Input(&lContext, lacInnerPad, 64);
    SHA1Input(&lContext, static_cast<const unsigned char*>(pData), nDataLength);
    SHA1Result(&lContext, lacDigest);

    SHA1Reset(&lContext);
    SHA1Input(&lContext, lacOuterPad, 64);
    SHA1Input(&lContext, lacDigest, SHA1HashSize);
    SHA1Result(&lContext, lacDigest);

    void* lpResult = MassiveMalloc(SHA1HashSize);
    if (!lpResult)
    {
        return 0;
    }
    std::memcpy(lpResult, lacDigest, SHA1HashSize);
    return lpResult;
}

} // namespace MassiveAdClient3
