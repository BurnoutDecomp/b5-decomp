// LibTomCrypt argument-check failure hook, as linked into the MassiveAd client (the
// diagnostic print is compiled out on this build; only the abort signal remains).

#include "SDKs/Packages/MassiveAd/LibTomCrypt/tomcrypt_massive.h"

#include <csignal>

void crypt_argchk(const char* /*v*/, const char* /*s*/, int /*d*/)
{
    (void)raise(SIGABRT);
}
