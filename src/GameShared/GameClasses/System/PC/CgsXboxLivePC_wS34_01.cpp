// CgsXboxLivePC_wS34_01.cpp -- PC bodies for the service-lookup calls the Massive ad client's
// address resolver makes (CgsXboxLivePC.cpp family).
// FLAG PC-platform leaf: no console online service exists on the host, so the service lookup
// fails the way the console's does when the title has no service, and the resolver keeps the
// configured server addresses.

#define _CRT_RAND_S
#include <cstdlib>

#include "types.hpp"

namespace
{
    // A failure HRESULT: the callers only test the sign and log the value.
    const s32 KI_E_FAIL = static_cast<s32>(0x80004005u);

    // WSAENETDOWN: no network service to translate a server address through.
    const s32 KI_WSAENETDOWN = 10050;
}

// [PC platform leaf] Service record lookup; fails, no online service is reachable.
extern "C" long XOnlineGetServiceInfo(u32 /*luServiceId*/, void* /*lpServiceInfo*/)
{
    return KI_E_FAIL;
}

// [PC platform leaf] Title-server enumeration; fails, there is no service to enumerate.
extern "C" long XTitleServerCreateEnumerator(const char* /*lpszServerInfo*/, u32 /*luItems*/,
                                             u32* lpcbBuffer, void** lphEnum)
{
    if (lpcbBuffer != 0)
        *lpcbBuffer = 0u;
    if (lphEnum != 0)
        *lphEnum = 0;
    return KI_E_FAIL;
}

// [PC platform leaf] Secure-address translation of a title server; fails, no service.
extern "C" s32 XNetServerToInAddr(u32 /*luServerAddress*/, u32 /*luServiceId*/, u32* /*lpSecureAddress*/)
{
    return KI_WSAENETDOWN;
}

// [PC platform leaf] Fills cb bytes with cryptographically random data (the host CRT generator).
extern "C" long XNetRandom(u8* lpBuffer, u32 luCount)
{
    for (u32 lu = 0u; lu < luCount; ++lu)
    {
        unsigned int luValue = 0u;
        rand_s(&luValue);
        lpBuffer[lu] = static_cast<u8>(luValue);
    }
    return 0;
}
