// LibTomCrypt MD5 (LTC_SMALL_CODE, LTC_CLEAN_STACK), as linked into the MassiveAd client.

#include "SDKs/Packages/MassiveAd/LibTomCrypt/tomcrypt_massive.h"

#include <cstring>

#define F_(x, y, z) (z ^ (x & (y ^ z)))
#define G_(x, y, z) (y ^ (z & (y ^ x)))
#define H_(x, y, z) (x ^ y ^ z)
#define I_(x, y, z) (y ^ (x | (~z)))

#define ROLc(x, y) ((((ulong32)(x) << (ulong32)((y) & 31)) | ((ulong32)(x) >> (ulong32)(32 - ((y) & 31)))) & 0xFFFFFFFFUL)

#define FF(a, b, c, d, M, s, t) a = (a + F_(b, c, d) + M + t); a = ROLc(a, s) + b;
#define GG(a, b, c, d, M, s, t) a = (a + G_(b, c, d) + M + t); a = ROLc(a, s) + b;
#define HH(a, b, c, d, M, s, t) a = (a + H_(b, c, d) + M + t); a = ROLc(a, s) + b;
#define II(a, b, c, d, M, s, t) a = (a + I_(b, c, d) + M + t); a = ROLc(a, s) + b;

#define LOAD32L(x, y)                                                                         \
    do                                                                                        \
    {                                                                                         \
        x = ((ulong32)((y)[3] & 255) << 24) | ((ulong32)((y)[2] & 255) << 16) |               \
            ((ulong32)((y)[1] & 255) << 8) | ((ulong32)((y)[0] & 255));                       \
    } while (0)

#define STORE32L(x, y)                                                                        \
    do                                                                                        \
    {                                                                                         \
        (y)[3] = (unsigned char)(((x) >> 24) & 255);                                          \
        (y)[2] = (unsigned char)(((x) >> 16) & 255);                                          \
        (y)[1] = (unsigned char)(((x) >> 8) & 255);                                           \
        (y)[0] = (unsigned char)((x) & 255);                                                  \
    } while (0)

#define STORE64L(x, y)                                                                        \
    do                                                                                        \
    {                                                                                         \
        (y)[7] = (unsigned char)(((x) >> 56) & 255);                                          \
        (y)[6] = (unsigned char)(((x) >> 48) & 255);                                          \
        (y)[5] = (unsigned char)(((x) >> 40) & 255);                                          \
        (y)[4] = (unsigned char)(((x) >> 32) & 255);                                          \
        (y)[3] = (unsigned char)(((x) >> 24) & 255);                                          \
        (y)[2] = (unsigned char)(((x) >> 16) & 255);                                          \
        (y)[1] = (unsigned char)(((x) >> 8) & 255);                                           \
        (y)[0] = (unsigned char)((x) & 255);                                                  \
    } while (0)

static const unsigned char Worder[64] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12,
    5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2,
    0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9
};

static const unsigned char Rorder[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

static const ulong32 Korder[64] = {
    0xD76AA478UL, 0xE8C7B756UL, 0x242070DBUL, 0xC1BDCEEEUL, 0xF57C0FAFUL, 0x4787C62AUL,
    0xA8304613UL, 0xFD469501UL, 0x698098D8UL, 0x8B44F7AFUL, 0xFFFF5BB1UL, 0x895CD7BEUL,
    0x6B901122UL, 0xFD987193UL, 0xA679438EUL, 0x49B40821UL, 0xF61E2562UL, 0xC040B340UL,
    0x265E5A51UL, 0xE9B6C7AAUL, 0xD62F105DUL, 0x02441453UL, 0xD8A1E681UL, 0xE7D3FBC8UL,
    0x21E1CDE6UL, 0xC33707D6UL, 0xF4D50D87UL, 0x455A14EDUL, 0xA9E3E905UL, 0xFCEFA3F8UL,
    0x676F02D9UL, 0x8D2A4C8AUL, 0xFFFA3942UL, 0x8771F681UL, 0x6D9D6122UL, 0xFDE5380CUL,
    0xA4BEEA44UL, 0x4BDECFA9UL, 0xF6BB4B60UL, 0xBEBFBC70UL, 0x289B7EC6UL, 0xEAA127FAUL,
    0xD4EF3085UL, 0x04881D05UL, 0xD9D4D039UL, 0xE6DB99E5UL, 0x1FA27CF8UL, 0xC4AC5665UL,
    0xF4292244UL, 0x432AFF97UL, 0xAB9423A7UL, 0xFC93A039UL, 0x655B59C3UL, 0x8F0CCC92UL,
    0xFFEFF47DUL, 0x85845DD1UL, 0x6FA87E4FUL, 0xFE2CE6E0UL, 0xA3014314UL, 0x4E0811A1UL,
    0xF7537E82UL, 0xBD3AF235UL, 0x2AD7D2BBUL, 0xEB86D391UL
};

static int _md5_compress(hash_state* md, unsigned char* buf)
{
    ulong32 i, W[16], a, b, c, d, t;

    // copy the state into 512-bits into W[0..15]
    for (i = 0; i < 16; i++)
    {
        LOAD32L(W[i], buf + (4 * i));
    }

    // copy state
    a = md->md5.state[0];
    b = md->md5.state[1];
    c = md->md5.state[2];
    d = md->md5.state[3];

    for (i = 0; i < 16; ++i)
    {
        FF(a, b, c, d, W[Worder[i]], Rorder[i], Korder[i]);
        t = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 32; ++i)
    {
        GG(a, b, c, d, W[Worder[i]], Rorder[i], Korder[i]);
        t = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 48; ++i)
    {
        HH(a, b, c, d, W[Worder[i]], Rorder[i], Korder[i]);
        t = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 64; ++i)
    {
        II(a, b, c, d, W[Worder[i]], Rorder[i], Korder[i]);
        t = d; d = c; c = b; b = a; a = t;
    }

    md->md5.state[0] = md->md5.state[0] + a;
    md->md5.state[1] = md->md5.state[1] + b;
    md->md5.state[2] = md->md5.state[2] + c;
    md->md5.state[3] = md->md5.state[3] + d;

    return CRYPT_OK;
}

static int md5_compress(hash_state* md, unsigned char* buf)
{
    int err;
    err = _md5_compress(md, buf);
    burn_stack(sizeof(ulong32) * 21);
    return err;
}

int md5_init(hash_state* md)
{
    if (!md)
    {
        crypt_argchk("md != NULL", ".\\md5.c", 209);
    }
    md->md5.state[0] = 0x67452301UL;
    md->md5.state[1] = 0xefcdab89UL;
    md->md5.state[2] = 0x98badcfeUL;
    md->md5.state[3] = 0x10325476UL;
    md->md5.curlen = 0;
    md->md5.length = 0;
    return CRYPT_OK;
}

// The LibTomCrypt HASH_PROCESS body for MD5 (64-byte blocks).
int md5_process(hash_state* md, const unsigned char* in, unsigned long inlen)
{
    unsigned long n;
    int err;
    if (!md)
    {
        crypt_argchk("md != NULL", ".\\md5.c", 218);
    }
    if (!in)
    {
        crypt_argchk("buf != NULL", ".\\md5.c", 218);
    }
    if (md->md5.curlen > sizeof(md->md5.buf))
    {
        return CRYPT_INVALID_ARG;
    }
    while (inlen > 0)
    {
        if (md->md5.curlen == 0 && inlen >= 64)
        {
            if ((err = md5_compress(md, (unsigned char*)in)) != CRYPT_OK)
            {
                return err;
            }
            md->md5.length += 64 * 8;
            in += 64;
            inlen -= 64;
        }
        else
        {
            n = 64 - md->md5.curlen;
            if (inlen < n)
            {
                n = inlen;
            }
            std::memcpy(md->md5.buf + md->md5.curlen, in, (size_t)n);
            md->md5.curlen += n;
            in += n;
            inlen -= n;
            if (md->md5.curlen == 64)
            {
                if ((err = md5_compress(md, md->md5.buf)) != CRYPT_OK)
                {
                    return err;
                }
                md->md5.length += 8 * 64;
                md->md5.curlen = 0;
            }
        }
    }
    return CRYPT_OK;
}

int md5_done(hash_state* md, unsigned char* out)
{
    int i;

    if (!md)
    {
        crypt_argchk("md != NULL", ".\\md5.c", 224);
    }
    if (!out)
    {
        crypt_argchk("hash != NULL", ".\\md5.c", 225);
    }

    if (md->md5.curlen >= sizeof(md->md5.buf))
    {
        return CRYPT_INVALID_ARG;
    }

    // increase the length of the message
    md->md5.length += md->md5.curlen * 8;

    // append the '1' bit
    md->md5.buf[md->md5.curlen++] = (unsigned char)0x80;

    // if the length is currently above 56 bytes append zeros then compress, then fall back to
    // padding zeros and length encoding like normal
    if (md->md5.curlen > 56)
    {
        while (md->md5.curlen < 64)
        {
            md->md5.buf[md->md5.curlen++] = (unsigned char)0;
        }
        md5_compress(md, md->md5.buf);
        md->md5.curlen = 0;
    }

    // pad upto 56 bytes of zeroes
    while (md->md5.curlen < 56)
    {
        md->md5.buf[md->md5.curlen++] = (unsigned char)0;
    }

    // store length
    STORE64L(md->md5.length, md->md5.buf + 56);
    md5_compress(md, md->md5.buf);

    // copy output
    for (i = 0; i < 4; i++)
    {
        STORE32L(md->md5.state[i], out + (4 * i));
    }
    return CRYPT_OK;
}
