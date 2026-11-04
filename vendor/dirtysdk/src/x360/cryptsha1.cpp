#include "cryptsha1.h"
#include <cstddef>
#include <cstring>

CGS_STATIC_ASSERT(sizeof(CryptSha1T) == 0x5C, "ARTIST SHA-1 state extent");
CGS_STATIC_ASSERT(offsetof(CryptSha1T, strData) == 0x1C, "ARTIST SHA-1 input block");

static inline u32 RotateLeft(u32 word, u32 shift)
{
    return (word << shift) | (word >> (32 - shift));
}

static inline void Sha1Round(u32 word, u32 mix, u32 constant,
                            u32& a, u32& b, u32& c, u32& d, u32& e)
{
    const u32 next = RotateLeft(a, 5) + mix + e + word + constant;
    e = d;
    d = c;
    c = RotateLeft(b, 30);
    b = a;
    a = next;
}

// ARTIST 0x82B41720
static void CryptSha1ProcessBlock(CryptSha1T* pSha1, const u8* pBlock)
{
    u32 words[80];
    for (u32 i = 0; i < 16; ++i, pBlock += 4)
    {
        words[i] = (u32(pBlock[0]) << 24) | (u32(pBlock[1]) << 16)
                 | (u32(pBlock[2]) << 8) | u32(pBlock[3]);
    }
    for (u32 i = 16; i < 80; ++i)
        words[i] = RotateLeft(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);

    u32 a = pSha1->H[0];
    u32 b = pSha1->H[1];
    u32 c = pSha1->H[2];
    u32 d = pSha1->H[3];
    u32 e = pSha1->H[4];
    for (u32 i = 0; i < 20; ++i)
        Sha1Round(words[i], (b & c) | (~b & d), 0x5A827999u, a, b, c, d, e);
    for (u32 i = 20; i < 40; ++i)
        Sha1Round(words[i], b ^ c ^ d, 0x6ED9EBA1u, a, b, c, d, e);
    for (u32 i = 40; i < 60; ++i)
        Sha1Round(words[i], (b & c) | ((b | c) & d), 0x8F1BBCDCu, a, b, c, d, e);
    for (u32 i = 60; i < 80; ++i)
        Sha1Round(words[i], b ^ c ^ d, 0xCA62C1D6u, a, b, c, d, e);

    pSha1->H[0] += a;
    pSha1->H[1] += b;
    pSha1->H[2] += c;
    pSha1->H[3] += d;
    pSha1->H[4] += e;
}

// ARTIST 0x82B41980
extern "C" void CryptSha1Init(CryptSha1T* pSha1)
{
    pSha1->uCount = 0;
    pSha1->uPartialCount = 0;
    pSha1->H[0] = 0x67452301u;
    pSha1->H[1] = 0xEFCDAB89u;
    pSha1->H[2] = 0x98BADCFEu;
    pSha1->H[3] = 0x10325476u;
    pSha1->H[4] = 0xC3D2E1F0u;
}

// ARTIST 0x82B419D0
extern "C" void CryptSha1Update(CryptSha1T* pSha1, const u8* pInput, u32 uInputLen)
{
    if (pSha1->uPartialCount != 0)
    {
        const u32 wanted = 64 - pSha1->uPartialCount;
        const u32 copied = uInputLen < wanted ? uInputLen : wanted;
        std::memcpy(pSha1->strData + pSha1->uPartialCount, pInput, copied);
        pInput += copied;
        uInputLen -= copied;

        if (copied == wanted)
        {
            CryptSha1ProcessBlock(pSha1, pSha1->strData);
            pSha1->uCount += 64;
            pSha1->uPartialCount = 0;
        }
        else
        {
            pSha1->uPartialCount += copied;
        }
    }

    while (uInputLen >= 64)
    {
        CryptSha1ProcessBlock(pSha1, pInput);
        pSha1->uCount += 64;
        pInput += 64;
        uInputLen -= 64;
    }

    if (uInputLen != 0)
    {
        std::memcpy(pSha1->strData + pSha1->uPartialCount, pInput, uInputLen);
        pSha1->uPartialCount += uInputLen;
    }
}

// ARTIST 0x82B41AC8
extern "C" void CryptSha1Final(CryptSha1T* pSha1, void* pBuffer, u32 uLength)
{
    pSha1->uCount += pSha1->uPartialCount;
    u8 pad = 0x80;
    if (pSha1->uPartialCount >= 56)
    {
        u32 next = pSha1->uPartialCount;
        pSha1->strData[next++] = pad;
        while (next < 64)
            pSha1->strData[next++] = 0;
        CryptSha1ProcessBlock(pSha1, pSha1->strData);
        pSha1->uPartialCount = 0;
        pad = 0;
    }

    u32 next = pSha1->uPartialCount;
    pSha1->strData[next++] = pad;
    while (next < 56)
        pSha1->strData[next++] = 0;
    const u64 bitCount = static_cast<u64>(pSha1->uCount) << 3;
    for (u32 i = 0; i < 8; ++i)
        pSha1->strData[56 + i] = static_cast<u8>(bitCount >> (8 * (7 - i)));
    CryptSha1ProcessBlock(pSha1, pSha1->strData);

    u8* output = static_cast<u8*>(pBuffer);
    const u32 copied = uLength < 20 ? uLength : 20;
    for (u32 i = 0; i < copied; ++i)
        output[i] = static_cast<u8>(pSha1->H[i / 4] >> (8 * (3 - (i % 4))));
}
