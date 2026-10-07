// LibTomCrypt burn_stack, as linked into the MassiveAd client: clears the stack in 32-byte
// frames by recursing until len bytes have been covered.

#include "SDKs/Packages/MassiveAd/LibTomCrypt/tomcrypt_massive.h"

void burn_stack(unsigned long len)
{
    unsigned char buf[32];
    zeromem(buf, sizeof(buf));
    if (len > (unsigned long)sizeof(buf))
    {
        burn_stack(len - sizeof(buf));
    }
}
