// LibTomCrypt zeromem, as linked into the MassiveAd client.

// XDK size optimization matches ARTIST at 0x82BDF0A0 (1 function).
// MassiveAd was built for size: /Os turns 63 more of its functions exact across 31 files, none worse.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

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
