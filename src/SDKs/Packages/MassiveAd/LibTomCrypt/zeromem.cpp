// LibTomCrypt zeromem, as linked into the MassiveAd client.

#include "SDKs/Packages/MassiveAd/LibTomCrypt/tomcrypt_massive.h"

void zeromem(void* dst, unsigned long len)
{
    unsigned char* mem = static_cast<unsigned char*>(dst);
    if (!dst)
    {
        crypt_argchk("dst != NULL", ".\\zeromem.c", 16);
    }
    while (len-- > 0)
    {
        *mem++ = 0;
    }
}
